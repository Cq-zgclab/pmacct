# SAV IPFIX Collector

RFC 7011-compliant IPFIX collector for Source Address Validation (SAV) using libfixbuf.

## Features

- ✅ **SCTP Support**: Uses libfixbuf 2.5.3 with native SCTP
- ✅ **Multi-transport**: Supports SCTP, TCP, and UDP
- ✅ **SAV IE Support**: Implements draft-cao-opsawg-ipfix-sav-01 IEs
- ✅ **Command-line interface**: Flexible configuration options
- 🚧 **SubTemplateList decoding**: In progress (RFC 6313)
- 🚧 **JSON output**: Planned

## Build Requirements

- **libfixbuf 2.5.3+** (installed in `/usr/local/`)
- **glib-2.0 >= 2.34.0**
- **lksctp-tools-dev** (for SCTP support)
- **GCC** with C11 support

## Build

```bash
cd sav-collector
make
```

## Run

### SCTP (default)
```bash
LD_LIBRARY_PATH=/usr/local/lib ./sav_collector --listen=sctp://0.0.0.0:4739
```

### TCP (for testing)
```bash
LD_LIBRARY_PATH=/usr/local/lib ./sav_collector --listen=tcp://0.0.0.0:4739
```

### With output file
```bash
LD_LIBRARY_PATH=/usr/local/lib ./sav_collector \
    --listen=sctp://0.0.0.0:4739 \
    --output=sav_rules.json \
    --verbose
```

## Usage

```
Usage: ./sav_collector [OPTIONS]
Options:
  --listen=CONNSPEC    Listen specification (default: sctp://0.0.0.0:4739)
                       Formats: sctp://HOST:PORT, tcp://HOST:PORT, udp://HOST:PORT
  --output=FILE        Output file for SAV rules (default: stdout)
  --verbose            Enable verbose logging
  --help               Show this help message
```

## Testing

### Test with ipfixcol2
1. Configure ipfixcol2 to send to this collector
2. Start collector: `make run`
3. Check logs for received IPFIX messages

### Test with ipfixDump
```bash
# Convert IPFIX file to see template structure
ipfixDump -f input.ipfix
```

## Implementation Status

### Completed ✅
- [x] SCTP/TCP/UDP listener using libfixbuf API
- [x] Command-line argument parsing
- [x] Signal handling for graceful shutdown
- [x] Information Model initialization
- [x] Basic IPFIX message reception
- [x] SAV IE definitions (placeholders)

### In Progress 🚧
- [ ] SubTemplateList decoding (RFC 6313)
- [ ] Template management for SAV records
- [ ] Field extraction for IEs 901-904

### Planned 📋
- [ ] JSON output formatter
- [ ] Complete SAV rule extraction
- [ ] Integration with pmacct
- [ ] Performance optimization

## SAV Information Elements

According to `draft-cao-opsawg-ipfix-sav-01`:

| IE Name | IE ID | Type | Description |
|---------|-------|------|-------------|
| savRuleType | TBD1 | uint8 | 0=allowlist, 1=blocklist |
| savTargetType | TBD2 | uint8 | 0=interface-based, 1=prefix-based |
| savMatchedContentList | TBD3 | subTemplateList | SAV rule content |
| savPolicyAction | TBD4 | uint8 | 0=permit, 1=discard, etc. |

### Sub-Template IDs

- **901**: IPv4 Interface-to-Prefix
- **902**: IPv6 Interface-to-Prefix
- **903**: IPv4 Prefix-to-Interface
- **904**: IPv6 Prefix-to-Interface

## Architecture

```
┌─────────────────┐
│  IPFIX Exporter │ (sends SAV rules via SCTP/TCP/UDP)
└────────┬────────┘
         │ IPFIX Messages
         │ (RFC 7011)
         ▼
┌─────────────────┐
│  SAV Collector  │ (this application)
│                 │
│ ┌─────────────┐ │
│ │  libfixbuf  │ │ (IPFIX parsing + SCTP)
│ │   v2.5.3    │ │
│ └─────────────┘ │
│        │        │
│        ▼        │
│ ┌─────────────┐ │
│ │ SubTemplate │ │ (decode SAV rules)
│ │   Decoder   │ │
│ └─────────────┘ │
│        │        │
│        ▼        │
│ ┌─────────────┐ │
│ │JSON Output  │ │
│ └─────────────┘ │
└─────────────────┘
```

## Troubleshooting

### Library not found
```bash
# Add to shell profile or run before each execution
export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH
```

### SCTP not available
```bash
# Load SCTP kernel module
sudo modprobe sctp

# Verify
lsmod | grep sctp
```

### Port already in use
```bash
# Check what's using port 4739
sudo netstat -tulpn | grep 4739

# Or use a different port
./sav_collector --listen=sctp://0.0.0.0:14739
```

## Development

### Enable debug logging
Modify `CFLAGS` in `Makefile`:
```makefile
CFLAGS = -Wall -Wextra -DDEBUG -g
```

### Add new IE
1. Define in header section of `sav_collector.c`
2. Add to `init_sav_info_model()` using `FB_IE_INIT_FULL` macro
3. Update template in `process_buffer()`

## References

- [draft-cao-opsawg-ipfix-sav-01](https://datatracker.ietf.org/doc/draft-cao-opsawg-ipfix-sav/)
- [RFC 7011 - IPFIX Protocol](https://tools.ietf.org/html/rfc7011)
- [RFC 6313 - SubTemplateList](https://tools.ietf.org/html/rfc6313)
- [libfixbuf Documentation](https://tools.netsa.cert.org/fixbuf/)

## License

Follows pmacct license (see parent directory).

## Author

Part of pmacct SAV IPFIX implementation project.
