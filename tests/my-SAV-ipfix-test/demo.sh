#!/bin/bash
# Hackathon SAV IPFIX Demo Script - Complete SAV Field Parsing

echo "=========================================="
echo "  SAV IPFIX Hackathon Demo"
echo "  RFC 6313 subTemplateList Implementation"
echo "  ✓ Complete Field Extraction"
echo "=========================================="
echo ""

# Kill any existing nfacctd
echo "[Setup] Stopping existing nfacctd..."
pkill -9 nfacctd 2>/dev/null || true
sleep 1

# Start nfacctd
echo "[Setup] Starting nfacctd..."
/workspaces/pmacct/src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd.log 2>&1 &
sleep 3

if ! pgrep -x "nfacctd" > /dev/null; then
    echo "❌ nfacctd failed to start"
    echo "Check log: tail /tmp/nfacctd.log"
    exit 1
fi

echo "✅ nfacctd is running"
echo ""

# Test 1: Standard IANA mode
echo "Test 1: Standard IANA Mode (IE 30001-30004)"
echo "--------------------------------------------"
python3 scripts/send_ipfix_with_ip.py \
    --host 127.0.0.1 --port 9995 \
    --sav-rules data/sav_example.json \
    --count 1
echo ""

# Test 2: Enterprise mode
echo "Test 2: Enterprise Mode (PEN=0, IE 1-4)"
echo "----------------------------------------"
python3 scripts/send_ipfix_with_ip.py \
    --host 127.0.0.1 --port 9995 \
    --sav-rules data/sav_example.json \
    --enterprise --pen 0 \
    --count 1
echo ""

# Show parsed SAV rules
echo ""
echo "=========================================="
echo "  SAV Rules Parsed:"
echo "=========================================="
tail -30 /tmp/nfacctd.log | grep "SAV:" | tail -10
echo ""
echo "✓ Demo complete!"
echo "Full log: /tmp/nfacctd.log"
echo "Stop nfacctd: pkill -9 nfacctd"
echo ""
grep "template ID   : 400" /tmp/nfacctd.log | wc -l | xargs echo "Template 400 received: " 
echo ""

echo "✅ Demo complete!"
echo ""
echo "Next steps:"
echo "  - Check /tmp/nfacct.log for flow records"
echo "  - Send more messages with --count 10"
echo "  - Try different sub-template IDs (901-904)"
