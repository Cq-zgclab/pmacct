#!/bin/bash
# Quick SAV Integration Test
# Tests Phase 1B.2 - SAV parser integration into nfacctd

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

echo "========================================="
echo "  SAV Integration Test (Phase 1B.2)"
echo "========================================="
echo ""

# Check if nfacctd exists
if [ ! -f "/workspaces/pmacct/src/nfacctd" ]; then
    echo "ERROR: nfacctd not found. Please compile pmacct first:"
    echo "  cd /workspaces/pmacct && ./configure && make"
    exit 1
fi

# Start nfacctd in background
echo "1. Starting nfacctd..."
cd /workspaces/pmacct
./src/nfacctd -f "$PROJECT_ROOT/config/nfacctd-00.conf" 2>&1 | tee /tmp/nfacctd_sav_test.log &
NFACCTD_PID=$!

echo "   nfacctd PID: $NFACCTD_PID"
sleep 2

# Check if nfacctd is running
if ! kill -0 $NFACCTD_PID 2>/dev/null; then
    echo "ERROR: nfacctd failed to start"
    cat /tmp/nfacctd_sav_test.log
    exit 1
fi

echo "   ✓ nfacctd started successfully"
echo ""

# Send test IPFIX messages
echo "2. Sending test IPFIX messages..."

# Test 1: Template 901 (IPv4 if2prefix)
echo "   Test 1: Template 901 (IPv4 if2prefix)"
python3 "$PROJECT_ROOT/scripts/send_ipfix_with_ip.py" \
    --sav-rules "$PROJECT_ROOT/test-data/sav_rules_example.json" \
    --sub-template-id 901 \
    --use-complete-message \
    --host localhost \
    --port 9991 \
    2>&1 | grep -E "Sent|bytes"

sleep 1

# Test 2: Template 902 (IPv6 if2prefix)
echo "   Test 2: Template 902 (IPv6 if2prefix)"
python3 "$PROJECT_ROOT/scripts/send_ipfix_with_ip.py" \
    --sav-rules "$PROJECT_ROOT/test-data/sav_rules_ipv6_example.json" \
    --sub-template-id 902 \
    --use-complete-message \
    --host localhost \
    --port 9991 \
    2>&1 | grep -E "Sent|bytes"

sleep 1

echo ""
echo "3. Checking nfacctd logs for SAV processing..."
echo ""

# Check for SAV log entries
if grep -q "SAV:" /tmp/nfacctd_sav_test.log; then
    echo "✓ SAV processing detected in logs:"
    echo ""
    grep "SAV:" /tmp/nfacctd_sav_test.log | tail -20
    echo ""
    SUCCESS=1
else
    echo "✗ No SAV processing found in logs"
    echo ""
    echo "Last 50 lines of log:"
    tail -50 /tmp/nfacctd_sav_test.log
    echo ""
    SUCCESS=0
fi

# Cleanup
echo ""
echo "4. Cleanup..."
kill $NFACCTD_PID 2>/dev/null || true
wait $NFACCTD_PID 2>/dev/null || true
echo "   ✓ nfacctd stopped"

echo ""
echo "========================================="
if [ $SUCCESS -eq 1 ]; then
    echo "  ✓ Test PASSED"
    echo "  SAV parser successfully integrated!"
else
    echo "  ✗ Test FAILED"
    echo "  SAV processing not detected"
    exit 1
fi
echo "========================================="
echo ""

# Show summary
if [ $SUCCESS -eq 1 ]; then
    echo "Summary:"
    echo "--------"
    RULE_COUNT=$(grep -c "SAV Rule" /tmp/nfacctd_sav_test.log || echo "0")
    echo "- Total SAV rules parsed: $RULE_COUNT"
    echo "- Log file: /tmp/nfacctd_sav_test.log"
    echo ""
    echo "Next: Phase 1B.3 - JSON output enhancement"
fi
