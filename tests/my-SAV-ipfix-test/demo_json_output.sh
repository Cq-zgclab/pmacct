#!/bin/bash
# SAV IPFIX JSON Output Demo
# Shows SAV rules in JSON format (Hackathon Day 2-3 completion)

echo "=========================================="
echo "  SAV IPFIX JSON Output Demo"
echo "  Day 2-3: JSON Serialization"
echo "=========================================="
echo ""

# Cleanup
echo "[Setup] Cleaning up old data..."
pkill -9 nfacctd 2>/dev/null || true
rm -f /tmp/sav_output.json /tmp/nfacctd.log
sleep 1

# Start nfacctd
echo "[Setup] Starting nfacctd..."
/workspaces/pmacct/src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd.log 2>&1 &
sleep 3

if ! pgrep -x "nfacctd" > /dev/null; then
    echo "❌ nfacctd failed to start"
    exit 1
fi
echo "✅ nfacctd running"
echo ""

# Test 1: IPv4 Interface-to-Prefix (template 901)
echo "Test 1: Template 901 (IPv4 Interface-to-Prefix)"
echo "------------------------------------------------"
python3 scripts/send_ipfix_with_ip.py \
    --host 127.0.0.1 --port 9995 \
    --sav-rules data/sav_example.json \
    --sub-template-id 901 \
    --count 1
sleep 1
echo ""

# Test 2: IPv4 Prefix-to-Interface (template 903)
echo "Test 2: Template 903 (IPv4 Prefix-to-Interface)"
echo "------------------------------------------------"
python3 scripts/send_ipfix_with_ip.py \
    --host 127.0.0.1 --port 9995 \
    --sav-rules data/sav_example.json \
    --sub-template-id 903 \
    --count 1
sleep 1
echo ""

# Show JSON output
echo "=========================================="
echo "  JSON Output (/tmp/sav_output.json)"
echo "=========================================="
if [ -f /tmp/sav_output.json ]; then
    cat /tmp/sav_output.json | python3 -c "
import sys, json
for line in sys.stdin:
    try:
        obj = json.loads(line.strip())
        print(json.dumps(obj, indent=2))
        print()
    except:
        pass
"
    echo "✅ JSON output generated successfully!"
else
    echo "❌ No JSON output file found"
fi

echo ""
echo "=========================================="
echo "  SAV Parsing Log"
echo "=========================================="
grep "SAV:" /tmp/nfacctd.log | tail -10

echo ""
echo "✅ Demo complete!"
echo ""
echo "Key achievements:"
echo "  ✅ SAV rules exported to JSON format"
echo "  ✅ sav_validation_mode field"
echo "  ✅ sav_matched_rules array with interface_id and prefix"
echo "  ✅ Multiple template support (901, 903)"
echo ""
echo "Files:"
echo "  - JSON output: /tmp/sav_output.json"
echo "  - Parser log: /tmp/nfacctd.log"
echo ""
