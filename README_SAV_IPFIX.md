# SAV IPFIX Implementation for pmacct

Implementation of Source Address Validation (SAV) information export using IPFIX protocol according to `draft-cao-opsawg-ipfix-sav-01`.

## 🎯 Quick Navigation

### For Hackathon/Demo/Testing
→ **Go to [`sav-ipfix/hackathon-ipfixcol2/`](sav-ipfix/hackathon-ipfixcol2/README.md)**
- Python-based, quick setup
- Uses ipfixcol2 (no SCTP)
- Best for: demos, testing, development

### For Production Deployment  
→ **Go to [`sav-ipfix/production-libfixbuf/`](sav-ipfix/production-libfixbuf/README.md)**
- C implementation with full RFC 7011 compliance
- Native SCTP support
- Best for: production, performance-critical systems

### Implementation Overview
→ **See [`sav-ipfix/README.md`](sav-ipfix/README.md)**
- Architecture comparison
- Feature matrix
- Migration guide

## Directory Structure

```
pmacct/
├── sav-ipfix/                          # SAV IPFIX implementations
│   ├── README.md                       # Implementation overview & comparison
│   ├── hackathon-ipfixcol2/           # Quick PoC (Python + ipfixcol2)
│   │   ├── README.md
│   │   ├── parse_subtemplatelist.py
│   │   └── run_collector.sh
│   ├── production-libfixbuf/          # Production collector (C + libfixbuf)
│   │   ├── README.md
│   │   ├── Makefile
│   │   ├── sav_collector.c
│   │   └── sav_collector              # Binary
│   └── common/                        # Shared resources
│
├── docs/                              # Documentation
│   ├── draft-cao-opsawg-ipfix-sav-01.md  # IETF draft spec
│   ├── SAV_IPFIX_ENCODING_STRATEGY.md    # Implementation notes
│   ├── BGP_BMP_METRICS.md
│   └── archive/                          # Historical docs
│       ├── TODO_RFC7011_COMPLIANT.md
│       └── session-history/
│
├── scripts/                           # Utility scripts
│   ├── capture_once.sh
│   └── server_setup_nosudo.sh
│
└── [pmacct source code...]           # Original pmacct
```

## What's New in This Implementation

### ✅ Completed Features

1. **Hackathon Implementation** (Phase 0)
   - ✅ SubTemplateList parser in Python (442 lines)
   - ✅ Tested with ipfixcol2
   - ✅ Supports SAV templates 901-904
   - ⚠️ TCP/UDP only (no SCTP)

2. **Production Implementation** (Phase 1)
   - ✅ libfixbuf 2.5.3 integration with SCTP
   - ✅ C-based high-performance collector
   - ✅ RFC 7011 compliant listener
   - 🚧 SubTemplateList decoder (in progress)

### 📋 Roadmap

- [ ] Complete SubTemplateList decoding in production collector
- [ ] JSON output formatter
- [ ] Integration with pmacct core
- [ ] Performance benchmarks
- [ ] Security hardening

## Getting Started

### 1. Choose Your Approach

| Need | Use |
|------|-----|
| Quick demo for hackathon | [`hackathon-ipfixcol2`](sav-ipfix/hackathon-ipfixcol2/) |
| Production deployment | [`production-libfixbuf`](sav-ipfix/production-libfixbuf/) |
| Understand differences | [`sav-ipfix/README.md`](sav-ipfix/README.md) |

### 2. Follow Implementation Guide

Each directory has detailed README with:
- Prerequisites
- Build/setup instructions  
- Usage examples
- Troubleshooting

## Key Documents

### Implementation Guides
- **[SAV IPFIX Overview](sav-ipfix/README.md)** - Start here for architecture
- **[Hackathon Guide](sav-ipfix/hackathon-ipfixcol2/README.md)** - Fast prototyping
- **[Production Guide](sav-ipfix/production-libfixbuf/README.md)** - Deployment-ready

### Specifications
- **[draft-cao-opsawg-ipfix-sav-01](docs/draft-cao-opsawg-ipfix-sav-01.md)** - IETF draft
- **[SAV IPFIX Encoding](docs/SAV_IPFIX_ENCODING_STRATEGY.md)** - Implementation details
- **[RFC 7011](https://tools.ietf.org/html/rfc7011)** - IPFIX Protocol
- **[RFC 6313](https://tools.ietf.org/html/rfc6313)** - SubTemplateList

### Archive
- **[docs/archive/](docs/archive/)** - Historical implementation notes

## Technical Highlights

### SubTemplateList Support (RFC 6313)
Both implementations support nested IPFIX structures:

```
SAV Record
├── savRuleType (uint8)
├── savTargetType (uint8) 
├── savMatchedContentList (SubTemplateList)
│   ├── Template 901: IPv4 Interface→Prefix
│   ├── Template 902: IPv6 Interface→Prefix
│   ├── Template 903: IPv4 Prefix→Interface
│   └── Template 904: IPv6 Prefix→Interface
└── savPolicyAction (uint8)
```

### SCTP Transport
Production implementation uses SCTP for:
- Reliable, ordered delivery
- Multi-homing support
- RFC 7011 compliance

## Development Status

| Component | Status | Notes |
|-----------|--------|-------|
| Hackathon Parser | ✅ Complete | Python, 442 lines, tested |
| Production Collector | 🚧 85% | C, SCTP working, needs STL decoder |
| Documentation | ✅ Complete | All READMEs updated |
| Testing Framework | 📋 Planned | E2E tests needed |
| Integration | 📋 Planned | pmacct core integration |

## Contributing

See implementation-specific READMEs for development setup.

## License

Follows pmacct license (see [COPYING](COPYING)).

## References

- [IETF draft-cao-opsawg-ipfix-sav](https://datatracker.ietf.org/doc/draft-cao-opsawg-ipfix-sav/)
- [libfixbuf](https://tools.netsa.cert.org/fixbuf/)
- [ipfixcol2](https://github.com/CESNET/ipfixcol2)
- [pmacct](http://www.pmacct.net/)

---

**Quick Start**: Choose [`hackathon`](sav-ipfix/hackathon-ipfixcol2/) or [`production`](sav-ipfix/production-libfixbuf/) → Read README → Run!
