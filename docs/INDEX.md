# SAV IPFIX Documentation Index

Complete documentation for SAV (Source Address Validation) IPFIX implementation.

## 📖 Start Reading Here

| Document | Purpose | Audience |
|----------|---------|----------|
| **[QUICKSTART_SAV.md](../QUICKSTART_SAV.md)** | 30-second decision guide | Everyone |
| **[README_SAV_IPFIX.md](../README_SAV_IPFIX.md)** | Complete project overview | Everyone |
| **[sav-ipfix/README.md](../sav-ipfix/README.md)** | Implementation comparison | Architects, developers |

## 📂 Implementation Guides

### Hackathon/PoC
- **[hackathon-ipfixcol2/README.md](../sav-ipfix/hackathon-ipfixcol2/README.md)**
  - Quick Python implementation
  - Uses ipfixcol2 (no SCTP)
  - Best for: demos, testing, development

### Production
- **[production-libfixbuf/README.md](../sav-ipfix/production-libfixbuf/README.md)**
  - C implementation with libfixbuf
  - Full SCTP support (RFC 7011)
  - Best for: production deployment

## 📋 Specifications

### IETF Draft
- **[draft-cao-opsawg-ipfix-sav-01.md](draft-cao-opsawg-ipfix-sav-01.md)**
  - Official IETF draft specification
  - Information Element definitions
  - SubTemplateList structures (templates 901-904)

### Implementation Notes
- **[SAV_IPFIX_ENCODING_STRATEGY.md](SAV_IPFIX_ENCODING_STRATEGY.md)**
  - Detailed encoding strategies
  - Field mappings
  - Integration guidelines

### Related Protocols
- **[BGP_BMP_METRICS.md](BGP_BMP_METRICS.md)**
  - BGP and BMP metric export
  - Related to SAV exporter integration

## 🗃️ Archive

Historical development documents (reference only):

### Session History
- **[archive/session-history/](archive/session-history/)** - Development session logs
  - `SESSION_SUMMARY_20251208.md` - Latest session summary
  - `RESUME_HERE.md` - Previous continuation points
  - `WORKSTATE.md` - Development state snapshots

### Planning Documents
- **[archive/TODO_RFC7011_COMPLIANT.md](archive/TODO_RFC7011_COMPLIANT.md)**
  - Original 32-hour implementation plan
  - libfixbuf-based approach
  
- **[archive/TODO_NEXT_WEEK.md](archive/TODO_NEXT_WEEK.md)**
  - Week-by-week roadmap
  
- **[archive/HACKATHON_DEMO.md](archive/HACKATHON_DEMO.md)**
  - Original hackathon demo plan
  
- **[archive/README_SAV_RFC7011.md](archive/README_SAV_RFC7011.md)**
  - Early SAV RFC documentation

## 🔗 External References

### Standards
- [RFC 7011 - IPFIX Protocol](https://tools.ietf.org/html/rfc7011)
- [RFC 6313 - SubTemplateList](https://tools.ietf.org/html/rfc6313)
- [RFC 5102 - IPFIX Information Model](https://tools.ietf.org/html/rfc5102)

### Tools & Libraries
- [libfixbuf Documentation](https://tools.netsa.cert.org/fixbuf/)
- [ipfixcol2 GitHub](https://github.com/CESNET/ipfixcol2)
- [pmacct Official Site](http://www.pmacct.net/)

### Draft Updates
- [IETF Datatracker](https://datatracker.ietf.org/doc/draft-cao-opsawg-ipfix-sav/)

## 📚 Reading Order by Role

### 🎯 I'm a Developer
1. [QUICKSTART_SAV.md](../QUICKSTART_SAV.md)
2. [sav-ipfix/README.md](../sav-ipfix/README.md) - Choose implementation
3. Implementation-specific README (hackathon or production)
4. [draft-cao-opsawg-ipfix-sav-01.md](draft-cao-opsawg-ipfix-sav-01.md) - Spec details

### 🏗️ I'm an Architect
1. [README_SAV_IPFIX.md](../README_SAV_IPFIX.md) - Full overview
2. [sav-ipfix/README.md](../sav-ipfix/README.md) - Architecture comparison
3. [SAV_IPFIX_ENCODING_STRATEGY.md](SAV_IPFIX_ENCODING_STRATEGY.md)
4. [draft-cao-opsawg-ipfix-sav-01.md](draft-cao-opsawg-ipfix-sav-01.md)

### 🎪 I'm Doing a Demo
1. [QUICKSTART_SAV.md](../QUICKSTART_SAV.md)
2. [hackathon-ipfixcol2/README.md](../sav-ipfix/hackathon-ipfixcol2/README.md)
3. Run the hackathon implementation

### 🚀 I'm Deploying to Production
1. [QUICKSTART_SAV.md](../QUICKSTART_SAV.md)
2. [production-libfixbuf/README.md](../sav-ipfix/production-libfixbuf/README.md)
3. [SAV_IPFIX_ENCODING_STRATEGY.md](SAV_IPFIX_ENCODING_STRATEGY.md)
4. [RFC 7011](https://tools.ietf.org/html/rfc7011) - SCTP requirements

## 🔍 Quick Lookups

### Find Information About...

| Topic | Document |
|-------|----------|
| SubTemplateList parsing | [hackathon README](../sav-ipfix/hackathon-ipfixcol2/README.md#subtemplatelist-format) |
| SCTP configuration | [production README](../sav-ipfix/production-libfixbuf/README.md#sctp-support) |
| IE definitions | [draft spec](draft-cao-opsawg-ipfix-sav-01.md#sav-ipfix-information-elements) |
| Template IDs 901-904 | [draft spec](draft-cao-opsawg-ipfix-sav-01.md#sub-template-ids) |
| libfixbuf setup | [production README](../sav-ipfix/production-libfixbuf/README.md#build-requirements) |
| Performance comparison | [sav-ipfix README](../sav-ipfix/README.md#two-approaches-comparison) |

## 📝 Document Status

| Document | Status | Last Updated |
|----------|--------|--------------|
| QUICKSTART_SAV.md | ✅ Current | 2025-12-08 |
| README_SAV_IPFIX.md | ✅ Current | 2025-12-08 |
| sav-ipfix/README.md | ✅ Current | 2025-12-08 |
| hackathon README | ✅ Current | 2025-12-08 |
| production README | ✅ Current | 2025-12-08 |
| draft-cao-opsawg-ipfix-sav-01.md | ✅ Current | 2025-12-08 |
| Archive/* | 📦 Archived | Various |

## 🆘 Getting Help

1. **Check the appropriate README first** (see above)
2. **Search this index** for your topic
3. **Read the specification** for protocol details
4. **Check archive/** for historical context (if needed)

---

**Quick Navigation**: [Quickstart](../QUICKSTART_SAV.md) | [Overview](../README_SAV_IPFIX.md) | [Hackathon](../sav-ipfix/hackathon-ipfixcol2/) | [Production](../sav-ipfix/production-libfixbuf/)
