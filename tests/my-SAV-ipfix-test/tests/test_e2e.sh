#!/bin/bash
# Complete End-to-End SAV Test
# Tests all phases: Parser → Integration → JSON Output

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PMACCT_ROOT="/workspaces/pmacct"

echo "=============================================="
echo "  SAV Complete E2E Test (Phase 1B.4)"
echo "=============================================="
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test results
TESTS_PASSED=0
TESTS_FAILED=0

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Step 1: Check build status
echo "Step 1: Checking build status..."
if [ ! -f "$PMACCT_ROOT/src/nfacctd" ]; then
    log_warn "nfacctd not compiled. Attempting to build..."
    cd "$PMACCT_ROOT"
    
    if [ ! -f "configure" ]; then
        log_info "Running autogen.sh..."
        ./autogen.sh
    fi
    
    if [ ! -f "Makefile" ]; then
        log_info "Running configure..."
        ./configure --enable-jansson || {
            log_error "Configure failed. Please install dependencies:"
            echo "  apk add autoconf automake libtool jansson-dev"
            exit 1
        }
    fi
    
    log_info "Building pmacct..."
    make -j4 || {
        log_error "Build failed. Check compilation errors above."
        exit 1
    }
fi

if [ -f "$PMACCT_ROOT/src/nfacctd" ]; then
    log_info "✓ nfacctd binary found"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    log_error "✗ nfacctd binary not found after build attempt"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    exit 1
fi

# Step 2: Start nfacctd with test config
echo ""
echo "Step 2: Starting nfacctd..."
LOG_FILE="/tmp/nfacctd_sav_e2e.log"
OUTPUT_FILE="/tmp/nfacctd_sav_output.json"

# Clean old files
rm -f "$LOG_FILE" "$OUTPUT_FILE"

# Create a simple config
cat > /tmp/nfacctd_sav_test.conf << EOF
daemonize: false
debug: false
pidfile: /tmp/nfacctd_sav_test.pid

nfacctd_ip: 0.0.0.0
nfacctd_port: 9991
nfacctd_pre_processing_checks: true

plugins: print
print_output: json
print_output_file: $OUTPUT_FILE
print_refresh_time: 1
print_history: 1m
print_history_roundoff: m
EOF

# Start nfacctd in background
cd "$PMACCT_ROOT"
./src/nfacctd -f /tmp/nfacctd_sav_test.conf > "$LOG_FILE" 2>&1 &
NFACCTD_PID=$!

log_info "nfacctd started (PID: $NFACCTD_PID)"
sleep 3

# Check if running
if ! kill -0 $NFACCTD_PID 2>/dev/null; then
    log_error "✗ nfacctd failed to start"
    cat "$LOG_FILE"
    TESTS_FAILED=$((TESTS_FAILED + 1))
    exit 1
fi

log_info "✓ nfacctd is running"
TESTS_PASSED=$((TESTS_PASSED + 1))

# Step 3: Send test IPFIX messages
echo ""
echo "Step 3: Sending test IPFIX messages..."

# Test 1: IPv4 template 901
log_info "Test 3.1: Template 901 (IPv4 if2prefix) with 3 rules"
python3 "$PROJECT_ROOT/scripts/send_ipfix_with_ip.py" \
    --sav-rules "$PROJECT_ROOT/test-data/sav_rules_example.json" \
    --sub-template-id 901 \
    --use-complete-message \
    --host localhost \
    --port 9991 2>&1 | grep -q "Sent.*bytes" && {
    log_info "✓ Template 901 message sent successfully"
    TESTS_PASSED=$((TESTS_PASSED + 1))
} || {
    log_error "✗ Failed to send Template 901 message"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

sleep 2

# Test 2: IPv6 template 902
log_info "Test 3.2: Template 902 (IPv6 if2prefix) with 2 rules"
python3 "$PROJECT_ROOT/scripts/send_ipfix_with_ip.py" \
    --sav-rules "$PROJECT_ROOT/test-data/sav_rules_ipv6_example.json" \
    --sub-template-id 902 \
    --use-complete-message \
    --host localhost \
    --port 9991 2>&1 | grep -q "Sent.*bytes" && {
    log_info "✓ Template 902 message sent successfully"
    TESTS_PASSED=$((TESTS_PASSED + 1))
} || {
    log_error "✗ Failed to send Template 902 message"
    TESTS_FAILED=$((TESTS_FAILED + 1))
}

sleep 3

# Step 4: Check logs for SAV processing
echo ""
echo "Step 4: Verifying SAV parser execution..."

if grep -q "SAV:" "$LOG_FILE"; then
    log_info "✓ SAV parser executed"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    
    # Count parsed rules
    RULE_COUNT=$(grep -c "SAV:" "$LOG_FILE" || echo "0")
    log_info "  Detected $RULE_COUNT SAV log entries"
else
    log_warn "✗ No SAV processing detected in logs"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

# Step 5: Check JSON output
echo ""
echo "Step 5: Verifying JSON output..."

# Wait for print plugin to flush
sleep 5

if [ -f "$OUTPUT_FILE" ] && [ -s "$OUTPUT_FILE" ]; then
    log_info "✓ JSON output file created"
    TESTS_PASSED=$((TESTS_PASSED + 1))
    
    # Check for SAV fields in JSON
    if grep -q "sav_validation_mode" "$OUTPUT_FILE"; then
        log_info "✓ JSON contains sav_validation_mode field"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        log_warn "✗ JSON missing sav_validation_mode field"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    
    if grep -q "sav_matched_rules" "$OUTPUT_FILE"; then
        log_info "✓ JSON contains sav_matched_rules array"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        
        # Show sample
        echo ""
        echo "Sample JSON output:"
        echo "-------------------"
        head -20 "$OUTPUT_FILE" | python3 -m json.tool 2>/dev/null || head -20 "$OUTPUT_FILE"
        echo "-------------------"
    else
        log_warn "✗ JSON missing sav_matched_rules array"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
    
    # Validate JSON structure
    if python3 -m json.tool "$OUTPUT_FILE" > /dev/null 2>&1; then
        log_info "✓ JSON is valid"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        log_error "✗ JSON is malformed"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
else
    log_error "✗ JSON output file not created or empty"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

# Cleanup
echo ""
echo "Step 6: Cleanup..."
kill $NFACCTD_PID 2>/dev/null || true
wait $NFACCTD_PID 2>/dev/null || true
log_info "✓ nfacctd stopped"

# Final summary
echo ""
echo "=============================================="
echo "  Test Results Summary"
echo "=============================================="
echo ""
echo "Tests Passed: $TESTS_PASSED"
echo "Tests Failed: $TESTS_FAILED"
echo ""

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ ALL TESTS PASSED${NC}"
    echo ""
    echo "Verification Details:"
    echo "  ✓ SAV parser compiled and integrated"
    echo "  ✓ IPFIX messages received and parsed"
    echo "  ✓ SAV rules extracted from subTemplateList"
    echo "  ✓ JSON output generated with structured SAV data"
    echo ""
    echo "Output files:"
    echo "  - Logs: $LOG_FILE"
    echo "  - JSON: $OUTPUT_FILE"
    echo ""
    echo "Phase 1B Complete! 🎉"
    exit 0
else
    echo -e "${RED}✗ SOME TESTS FAILED${NC}"
    echo ""
    echo "Debug information:"
    echo "  - Check logs: $LOG_FILE"
    echo "  - Check output: $OUTPUT_FILE"
    echo ""
    echo "Last 50 lines of log:"
    tail -50 "$LOG_FILE"
    exit 1
fi
