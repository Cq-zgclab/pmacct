# Hackathon PoC: SAV IPFIX with ipfixcol2

**Quick prototyping implementation** using ipfixcol2 for SAV IPFIX testing.

⚠️ **This is NOT production-ready**: No SCTP support, Python performance limitations.

✅ **Best for**: Demos, testing, rapid development.

## What's Here

- `parse_subtemplatelist.py` - Python parser for SubTemplateList (442 lines)
- `run_collector.sh` - Quick start script for ipfixcol2

## Features

✅ Parses SubTemplateList from ipfixcol2 JSON output  
✅ Supports SAV templates 901-904 (IPv4/IPv6)  
✅ JSON output formatter  
❌ No SCTP (TCP/UDP only)  
❌ Requires ipfixcol2 pre-installed  

## Quick Start

### 1. Prerequisites

```bash
# Install ipfixcol2 (if not already)
sudo apt-get install ipfixcol2

# Or on Alpine
sudo apk add ipfixcol2
```

### 2. Start ipfixcol2

```bash
# Use existing config or:
./run_collector.sh
```

### 3. Send IPFIX Data

Point your exporter to `tcp://localhost:4739` (or configured port).

### 4. Parse Results

```bash
# Parse ipfixcol2 JSON output
python3 parse_subtemplatelist.py /var/ipfixcol2/output/*.json

# Or with custom output
python3 parse_subtemplatelist.py --input output.json --output sav_rules.json
```

## Usage: parse_subtemplatelist.py

```bash
python3 parse_subtemplatelist.py [OPTIONS] <json_files>

Options:
  --input, -i FILE      Input JSON file from ipfixcol2
  --output, -o FILE     Output file for parsed SAV rules (default: stdout)
  --verbose, -v         Enable debug logging
```

### Example Output

```json
[
  {
    "template_id": 901,
    "type": "ipv4_interface_to_prefix",
    "interface": 2,
    "prefix": "192.0.2.0/24"
  },
  {
    "template_id": 902,
    "type": "ipv6_interface_to_prefix",
    "interface": 3,
    "prefix": "2001:db8::/32"
  }
]
```

## How It Works

### ipfixcol2 Pipeline

1. **Receive IPFIX** (TCP/UDP from exporter)
2. **Decode messages** (templates + data records)
3. **Export JSON** (with SubTemplateList as hex strings)

### Python Parser

1. **Read JSON** from ipfixcol2
2. **Extract hex data** from `savMatchedContentList` field
3. **Parse SubTemplateList** according to RFC 6313
4. **Decode SAV rules** (templates 901-904)
5. **Output structured JSON**

## SubTemplateList Format

```
Header (3 bytes):
  - Semantic (1 byte): 0xFF (all-of)
  - Template ID (2 bytes): 901/902/903/904

Content (variable):
  Template 901 (9 bytes): [interface(4)] [ipv4(4)] [len(1)]
  Template 902 (21 bytes): [interface(4)] [ipv6(16)] [len(1)]
  Template 903 (9 bytes): [ipv4(4)] [len(1)] [interface(4)]
  Template 904 (21 bytes): [ipv6(16)] [len(1)] [interface(4)]
```

## Testing with Sample Data

Create test IPFIX data:

```python
# test_sav_data.py
import json

sample = {
    "savRuleType": 0,
    "savMatchedContentList": "ff038501000002c00002001810"  # Template 901 example
}

with open('test_input.json', 'w') as f:
    json.dump(sample, f)
```

Parse:
```bash
python3 parse_subtemplatelist.py test_input.json
```

## Limitations

### 🚫 No SCTP Support
ipfixcol2 doesn't support SCTP (RFC 7011 requirement). Use TCP/UDP only.

**Workaround**: For production, migrate to `../production-libfixbuf/`.

### 🐌 Performance
Python interpreter overhead. Not suitable for high-throughput environments.

**Typical**: ~1000 records/sec  
**Production needs**: Use C implementation.

### 🔧 Manual Hex Parsing
SubTemplateList decoded manually from hex strings. Fragile if format changes.

**Better**: libfixbuf has native RFC 6313 support.

## Troubleshooting

### ipfixcol2 not receiving data

```bash
# Check listening port
sudo netstat -tulpn | grep 4739

# Check ipfixcol2 logs
tail -f /var/log/ipfixcol2/ipfixcol2.log
```

### JSON parsing errors

```bash
# Validate JSON output
cat output.json | jq .

# Check field names match
grep "savMatchedContentList" output.json
```

### SubTemplateList decode fails

Enable verbose mode:
```bash
python3 parse_subtemplatelist.py -v input.json
```

Check hex format:
```python
# Expected: "ff" + template_id(2 bytes hex) + data
# Example: "ff038501..." = semantic 0xff, template 901 (0x0385)
```

## Migration to Production

When ready for deployment:

1. **Validate** rules work correctly with this implementation
2. **Switch** to `../production-libfixbuf/sav_collector`
3. **Enable SCTP** in exporter configuration
4. **Test** end-to-end with SCTP transport

## Development

### Add New Template Support

Edit `parse_subtemplatelist.py`:

```python
def parse_sav_rule_905(data, offset):
    # Your new template logic
    return rule, new_offset
```

### Modify Output Format

Change `output_json()` function to customize JSON structure.

## Files

```
hackathon-ipfixcol2/
├── README.md                      # This file
├── parse_subtemplatelist.py       # Main parser (442 lines)
└── run_collector.sh               # ipfixcol2 startup script
```

## See Also

- **Production version**: `../production-libfixbuf/README.md`
- **Architecture**: `../README.md`
- **Draft spec**: `../../docs/draft-cao-opsawg-ipfix-sav-01.md`

## Status

✅ **Complete** - Fully functional for hackathon/testing purposes.

Not recommended for production due to SCTP limitation.
