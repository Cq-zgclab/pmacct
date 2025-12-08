# SAV IPFIX Implementation

Two implementation approaches for IPFIX-based Source Address Validation (SAV) according to `draft-cao-opsawg-ipfix-sav-01`.

## Directory Structure

```
sav-ipfix/
├── README.md                    # This file - implementation overview
├── hackathon-ipfixcol2/        # Hackathon PoC using ipfixcol2 (fast prototyping)
│   ├── README.md
│   ├── parse_subtemplatelist.py    # Python parser for ipfixcol2 output
│   └── run_collector.sh            # Quick start script
├── production-libfixbuf/       # Production implementation using libfixbuf (RFC 7011)
│   ├── README.md
│   ├── Makefile
│   ├── sav_collector.c             # C collector with SCTP support
│   └── sav_collector               # Compiled binary
└── common/                     # Shared resources
    └── (to be added)
```

## Two Approaches Comparison

| Feature | Hackathon (ipfixcol2) | Production (libfixbuf) |
|---------|----------------------|------------------------|
| **Purpose** | Quick PoC & testing | Production-ready deployment |
| **Language** | Python | C |
| **SCTP Support** | ❌ No (TCP/UDP only) | ✅ Yes (RFC 7011 compliant) |
| **Setup Time** | ~10 minutes | ~1 hour (compilation needed) |
| **Performance** | Low (Python interpreter) | High (native binary) |
| **SubTemplateList** | Manual hex parsing | Native RFC 6313 support |
| **Use Case** | Development, demos, testing | Production deployment |
| **Dependencies** | ipfixcol2, Python 3 | libfixbuf 2.5.3+, glib, lksctp |

## Quick Start

### Option 1: Hackathon PoC (Fastest)

**Use when**: Quick demo, testing with existing ipfixcol2 setup

```bash
cd sav-ipfix/hackathon-ipfixcol2
./run_collector.sh
# In another terminal, send IPFIX data
python3 parse_subtemplatelist.py ipfixcol2_output.json
```

**Pros**: 
- ✅ No compilation needed
- ✅ Works with existing ipfixcol2
- ✅ Easy to debug (Python)

**Cons**:
- ❌ No SCTP (RFC 7011 violation)
- ❌ Slow performance
- ❌ Not production-ready

### Option 2: Production Deployment (Recommended)

**Use when**: Production environment, need SCTP, performance matters

```bash
cd sav-ipfix/production-libfixbuf
make
LD_LIBRARY_PATH=/usr/local/lib ./sav_collector --listen=sctp://0.0.0.0:4739
```

**Pros**:
- ✅ Full SCTP support (RFC 7011 compliant)
- ✅ High performance (C native)
- ✅ Production-ready
- ✅ Native SubTemplateList decoding

**Cons**:
- ⚠️ Requires libfixbuf compilation
- ⚠️ More complex setup

## Architecture Diagrams

### Hackathon Flow (ipfixcol2)
```
┌─────────────┐
│   Exporter  │
│  (pmacct)   │
└──────┬──────┘
       │ IPFIX/TCP
       ▼
┌─────────────┐
│ ipfixcol2   │ (pre-installed)
└──────┬──────┘
       │ JSON files
       ▼
┌─────────────┐
│  Python     │ parse_subtemplatelist.py
│  Parser     │ (manual hex parsing)
└──────┬──────┘
       │
       ▼
   SAV Rules JSON
```

### Production Flow (libfixbuf)
```
┌─────────────┐
│   Exporter  │
│  (pmacct)   │
└──────┬──────┘
       │ IPFIX/SCTP (RFC 7011)
       ▼
┌─────────────┐
│ libfixbuf   │ (built-in)
│ sav_collector│
└──────┬──────┘
       │ Direct decode
       ▼
   SAV Rules JSON
```

## When to Use Which

### Use Hackathon Approach If:
- 🎯 Need quick demo for presentation
- 🎯 Testing SAV rule formats
- 🎯 ipfixcol2 already deployed
- 🎯 SCTP not required (dev environment)
- 🎯 Prefer Python for rapid iteration

### Use Production Approach If:
- 🎯 Production deployment
- 🎯 Need RFC 7011 compliance (SCTP mandatory)
- 🎯 Performance critical (high traffic)
- 🎯 Integration with pmacct core
- 🎯 Security hardening required

## Migration Path

Start with **hackathon** for PoC → Migrate to **production** for deployment:

1. **Phase 0**: Test concept with hackathon implementation
2. **Phase 1**: Validate SAV rules work correctly
3. **Phase 2**: Deploy production collector with SCTP
4. **Phase 3**: Optimize and integrate with existing systems

## Implementation Status

### Hackathon (ipfixcol2) ✅ Complete
- ✅ SubTemplateList parser (442 lines Python)
- ✅ Supports templates 901-904 (IPv4/IPv6)
- ✅ Tested with real ipfixcol2 data
- ✅ JSON output formatter
- ⚠️ TCP/UDP only (no SCTP)

### Production (libfixbuf) 🚧 In Progress
- ✅ SCTP listener implemented
- ✅ Information Model setup
- ✅ Basic IPFIX message reception
- 🚧 SubTemplateList decoder (needs implementation)
- 🚧 JSON output formatter (planned)
- 📋 Integration testing (pending)

## Development Setup

### Prerequisites for Both

```bash
# For hackathon
sudo apt-get install ipfixcol2 python3

# For production (Alpine Linux)
sudo apk add glib-dev liblksctp lksctp-tools-dev
# Build libfixbuf 2.5.3 from source (see production-libfixbuf/README.md)
```

## Testing

### End-to-End Test (Hackathon)
```bash
# Terminal 1: Start ipfixcol2
ipfixcol2 -c ipfixcol2_config.xml

# Terminal 2: Send test data
# (use existing exporter or test data)

# Terminal 3: Parse results
python3 hackathon-ipfixcol2/parse_subtemplatelist.py output/*.json
```

### End-to-End Test (Production)
```bash
# Terminal 1: Start collector
cd production-libfixbuf
make run

# Terminal 2: Send IPFIX/SCTP
# (use libfixbuf exporter or compatible tool)
```

## Documentation

- **Hackathon**: See `hackathon-ipfixcol2/README.md`
- **Production**: See `production-libfixbuf/README.md`
- **Draft Spec**: `../docs/draft-cao-opsawg-ipfix-sav-01.md`
- **IPFIX Encoding**: `../docs/SAV_IPFIX_ENCODING_STRATEGY.md`

## References

- [draft-cao-opsawg-ipfix-sav-01](https://datatracker.ietf.org/doc/draft-cao-opsawg-ipfix-sav/)
- [RFC 7011 - IPFIX Protocol](https://tools.ietf.org/html/rfc7011)
- [RFC 6313 - SubTemplateList](https://tools.ietf.org/html/rfc6313)
- [libfixbuf](https://tools.netsa.cert.org/fixbuf/)
- [ipfixcol2](https://github.com/CESNET/ipfixcol2)

## Contributing

When adding features:
1. Prototype in **hackathon** first (faster iteration)
2. Port to **production** once validated
3. Keep both implementations in sync for testing

## License

Follows pmacct license.
