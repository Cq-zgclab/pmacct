# SAV-Exporter

**SAV IPFIX Exporter/Collector based on libfixbuf**

## Overview

This is a dedicated IPFIX exporter and collector for Source Address Validation (SAV) data, built on top of CERT NetSA's libfixbuf library. Unlike the previous approach of integrating IPFIX into pmacct, this project is designed from the ground up around IPFIX and SubTemplateList support.

### Architecture

```
SAV Validation Engine → sav-exporter → IPFIX (SCTP/TCP/UDP) → sav-collector → JSON/Database
                        (libfixbuf)                              (libfixbuf)
```

### Why a Separate Project?

1. **Native IPFIX Support**: Designed around libfixbuf from day one, following YAF's proven architecture
2. **SubTemplateList First-Class**: SAV matched rules are encoded using RFC 6313 SubTemplateList
3. **Reference Implementation**: Based on YAF's patterns for handling complex IPFIX structures
4. **Maintainability**: Clean separation from pmacct's accounting logic

## SAV Information Elements

We define custom IPFIX IEs (will register proper PEN with IANA):

| IE Name | ID | Length | Type | Description |
|---------|-----|--------|------|-------------|
| `savRuleType` | 1 | 1 | uint8 | SAV rule type (901-904) |
| `savTargetType` | 2 | 1 | uint8 | Target address type (0=IPv4, 1=IPv6) |
| `savMatchedContentList` | 3 | variable | SubTemplateList | List of matched SAV rules |
| `savMatchCount` | 4 | 1 | uint8 | Number of matched rules |

### SAV Rule Templates

- **Template 901**: IPv4 Interface-to-Prefix (9 bytes)
- **Template 902**: IPv6 Interface-to-Prefix (21 bytes)
- **Template 903**: IPv4 Prefix-to-Interface (9 bytes)
- **Template 904**: IPv6 Prefix-to-Interface (21 bytes)

## Building

### Prerequisites

- libfixbuf >= 2.5.3 (with SCTP support)
- GLib 2.0
- libsctp

### Compile

```bash
cd sav-exporter
make
```

### Test

```bash
# Terminal 1: Start collector
./bin/sav_collector --listen tcp://127.0.0.1:4739 --output results.json

# Terminal 2: Send test data
./bin/sav_exporter --connect tcp://127.0.0.1:4739 --test-mode
```

## Project Status

- [x] Project structure created (Dec 8, 2025)
- [ ] Extract initialization patterns from YAF source
- [ ] Implement sav_exporter with SubTemplateList encoding
- [ ] Implement sav_collector with SubTemplateList decoding
- [ ] Integration testing with YAF tools (yafscii)
- [ ] Register Private Enterprise Number with IANA
- [ ] Production deployment

## References

- **libfixbuf**: https://tools.netsa.cert.org/fixbuf/
- **YAF**: https://tools.netsa.cert.org/yaf/
- **RFC 7011**: IPFIX Protocol Specification
- **RFC 6313**: Export of Structured Data in IPFIX
- **Previous Work**: See `../sav-ipfix/` for hackathon prototypes

## License

[To be determined - align with pmacct or standalone]
