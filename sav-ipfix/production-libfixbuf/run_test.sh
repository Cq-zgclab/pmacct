#!/bin/bash
# Test script for SAV collector integration testing

set -e

COLLECTOR_DIR="/workspaces/pmacct/sav-ipfix/production-libfixbuf"
COLLECTOR="$COLLECTOR_DIR/sav_collector"
EXPORTER="$COLLECTOR_DIR/test_exporter.py"
OUTPUT_FILE="$COLLECTOR_DIR/test_output.json"
PID_FILE="/tmp/sav_collector.pid"

cd "$COLLECTOR_DIR"

# Cleanup function
cleanup() {
    echo "Cleaning up..."
    if [ -f "$PID_FILE" ]; then
        PID=$(cat "$PID_FILE")
        if kill -0 "$PID" 2>/dev/null; then
            echo "Stopping collector (PID: $PID)"
            kill -INT "$PID" 2>/dev/null || true
            sleep 1
            kill -9 "$PID" 2>/dev/null || true
        fi
        rm -f "$PID_FILE"
    fi
    rm -f "$OUTPUT_FILE"
}

trap cleanup EXIT

# Remove old output
rm -f "$OUTPUT_FILE"

echo "=== SAV Collector Integration Test ==="
echo

# Start collector in background
echo "[1/3] Starting SAV collector..."
LD_LIBRARY_PATH=/usr/local/lib "$COLLECTOR" \
    --listen=tcp://127.0.0.1:4739 \
    --output="$OUTPUT_FILE" \
    2>&1 | tee collector.log &

COLLECTOR_PID=$!
echo "$COLLECTOR_PID" > "$PID_FILE"
echo "Collector started (PID: $COLLECTOR_PID)"

# Wait for collector to be ready
echo "Waiting for collector to bind..."
sleep 3

# Check if collector is still running
if ! kill -0 "$COLLECTOR_PID" 2>/dev/null; then
    echo "ERROR: Collector died during startup!"
    cat collector.log
    exit 1
fi

# Send test data
echo
echo "[2/3] Sending test IPFIX messages..."
python3 "$EXPORTER" --host 127.0.0.1 --port 4739 --transport tcp

# Give collector time to process
echo "Waiting for processing..."
sleep 2

# Stop collector gracefully
echo
echo "[3/3] Stopping collector..."
kill -INT "$COLLECTOR_PID"
sleep 2

# Check output
echo
echo "=== Test Results ==="
if [ -f "$OUTPUT_FILE" ]; then
    echo "Output file created: $OUTPUT_FILE"
    echo "Content:"
    cat "$OUTPUT_FILE"
    echo
    
    # Count decoded rules
    RULE_COUNT=$(grep -c '"template_id"' "$OUTPUT_FILE" || true)
    echo "Decoded SAV rules: $RULE_COUNT"
    
    if [ "$RULE_COUNT" -eq 3 ]; then
        echo "✅ SUCCESS: All 3 rules decoded correctly!"
        exit 0
    else
        echo "⚠️  WARNING: Expected 3 rules, got $RULE_COUNT"
        exit 1
    fi
else
    echo "❌ ERROR: Output file not created!"
    echo
    echo "Collector log:"
    cat collector.log
    exit 1
fi
