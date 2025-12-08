# SAV IPFIX - RFC 7011 Compliant Implementation

**Status**: Planning Phase (2025-12-08)  
**Architecture**: Standalone collector using libfixbuf  
**RFC Compliance**: RFC 7011 (IPFIX), RFC 6313 (SubTemplateList), draft-cao-opsawg-ipfix-sav-01

---

## 🎯 Project Goals

Build a **production-grade** SAV IPFIX collector that:
- ✅ Fully complies with RFC 7011 (including SCTP MUST requirement)
- ✅ Uses proven library (libfixbuf from CERT/NetSA)
- ✅ Supports all transport protocols (SCTP, TCP, UDP)
- ✅ Handles template management correctly
- ✅ Decodes SAV-specific IEs and subTemplateLists
- ✅ Provides clean JSON API for integration

---

## 📋 Quick Start

### Phase 0: Setup (Start Here!)

```bash
# 1. Install dependencies
sudo apt-get update
sudo apt-get install -y \
    build-essential \
    git \
    autoconf \
    automake \
    libtool \
    pkg-config \
    libglib2.0-dev \
    libsctp-dev \
    lksctp-tools \
    python3-pip

# 2. Install libfixbuf
cd /tmp
git clone https://github.com/cert-netsa/libfixbuf.git
cd libfixbuf
./autogen.sh
./configure --prefix=/usr/local
make -j4
sudo make install
sudo ldconfig

# 3. Test libfixbuf installation
ipfixDump --version

# 4. (Optional) Install Python bindings
pip3 install pyfixbuf

# 5. Read RFC 7011 Section 10
# Focus on SCTP requirements and transport layer
```

### Phase 1-4: Implementation

See detailed plan in: **`TODO_RFC7011_COMPLIANT.md`**

---

## 📁 Project Structure (After Implementation)

```
pmacct/
├── sav-collector/              # NEW: RFC-compliant collector
│   ├── src/
│   │   ├── main.c             # Entry point
│   │   ├── collector.c        # libfixbuf wrapper
│   │   ├── sav_ie.c           # SAV IE registration
│   │   ├── template_handler.c # Template callbacks
│   │   ├── sav_decoder.c      # SAV-specific decoding
│   │   └── output.c           # JSON/API output
│   ├── tests/
│   ├── Makefile
│   └── README.md
│
├── tests/sav-sender-rfc7011/   # NEW: RFC-compliant sender
│   ├── send_sav_ipfix.py      # pyfixbuf-based
│   └── data/
│
├── tests/my-SAV-ipfix-test/    # LEGACY: PoC implementation
│   └── README_LEGACY.md       # Marked as reference only
│
├── docs/
│   ├── RFC7011_IMPLEMENTATION_REPORT.md
│   ├── ARCHITECTURE.md
│   └── USER_GUIDE.md
│
├── TODO_RFC7011_COMPLIANT.md   # Main implementation plan
└── TODO_NEXT_WEEK.md           # Legacy PoC plan
```

---

## 🔄 Architecture Comparison

### Legacy PoC (Dec 2025)
```
Python Sender → UDP → nfacctd (pmacct) → Direct file output
              (手写IPFIX)    (单进程限制)    (绕过IPC)
```

**Issues**:
- ❌ No SCTP support (RFC 7011 violation)
- ❌ No template management
- ❌ IPC architecture mismatch
- ❌ Not production-ready

### New RFC 7011 Implementation
```
Sender (pyfixbuf) → SCTP/TCP/UDP → Collector (libfixbuf) → JSON API
                    (RFC framing)   (Template mgmt)         (Standard output)
                                           ↓
                                    (Optional) pmacct plugin
```

**Benefits**:
- ✅ Full RFC 7011 compliance
- ✅ SCTP primary transport
- ✅ Template lifecycle management
- ✅ Production-grade reliability
- ✅ Easier maintenance and updates

---

## 📚 Key RFCs and Documents

### Must Read
1. **RFC 7011** - IPFIX Protocol Specification
   - Section 3: Message Format
   - Section 8: Template Management
   - **Section 10: Transport Protocols** ⭐
     * 10.1 SCTP - **REQUIRED**
     * 10.2 TCP - MAY
     * 10.3 UDP - MAY

2. **RFC 6313** - Export of Structured Data in IPFIX
   - Section 4.5: SubTemplateList
   - Section 4.5.3: Semantic field

3. **RFC 4960** - SCTP Protocol
   - Multi-streaming
   - Reliability features

4. **draft-cao-opsawg-ipfix-sav-01** - SAV Information Elements
   - IE definitions (30001-30004)
   - Sub-template specifications (901-904)

### Reference
- libfixbuf docs: https://tools.netsa.cert.org/fixbuf/
- YAF project: https://tools.netsa.cert.org/yaf/
- IANA IPFIX registry: https://www.iana.org/assignments/ipfix/

---

## 🧪 Testing Strategy

### Level 1: Unit Tests
- libfixbuf API integration
- IE registration
- Template decoding
- SubTemplateList parsing

### Level 2: Integration Tests
- SCTP connection establishment
- TCP framing correctness
- UDP message handling
- Template withdrawal
- Multi-stream SCTP

### Level 3: Compliance Tests
- RFC 7011 checklist
- Wireshark dissector validation
- ipfixDump compatibility
- Interop with commercial collectors

### Level 4: Performance Tests
- 1000+ messages/second
- Memory leak detection (valgrind)
- CPU usage < 50% under load
- Latency < 10ms

---

## 🎯 Success Criteria

### Minimum Viable Product (MVP)
- [ ] SCTP transport works
- [ ] Can receive and decode SAV IPFIX messages
- [ ] SubTemplateList (RFC 6313) decoding
- [ ] All 4 sub-templates (901-904) supported
- [ ] JSON output matches legacy format
- [ ] Passes ipfixDump validation

### Production Ready
- [ ] TCP and UDP transports work
- [ ] Template management (register/withdraw)
- [ ] Error handling and logging
- [ ] Performance: >1000 msg/s
- [ ] Documentation complete
- [ ] RFC 7011 Implementation Report submitted

### Nice to Have
- [ ] REST API
- [ ] Database storage
- [ ] Web UI
- [ ] Prometheus metrics
- [ ] pmacct plugin integration

---

## 🚀 Timeline

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| Phase 0: Setup & Research | 4 hours | libfixbuf installed, RFC notes |
| Phase 1: Collector Core | 12 hours | Working SCTP collector |
| Phase 2: Sender Refactor | 8 hours | pyfixbuf-based sender |
| Phase 3: Testing | 4 hours | Test reports, validation |
| Phase 4: Documentation | 4 hours | Implementation Report |
| **Total** | **32 hours** | **Production system** |

**Calendar**: 4-5 working days (6-8 hours/day)

---

## 💡 Why This Approach?

### Technical Reasons
1. **RFC Compliance**: SCTP is REQUIRED, not optional
2. **Template Management**: Critical for IPFIX, complex to implement
3. **Proven Library**: libfixbuf used by NSA, DoD, research institutions
4. **Maintenance**: Library updates track RFC changes
5. **Performance**: Optimized C implementation

### Practical Reasons
1. **Faster Development**: Don't reinvent IPFIX stack
2. **Better Testing**: Interop with existing tools
3. **Cleaner Code**: Separation of concerns
4. **Documentation**: Standard APIs, examples exist
5. **Community**: Active libfixbuf community

### PoC Lessons Learned
1. pmacct IPC not suitable for complex IPFIX data
2. Manual IPFIX parsing error-prone
3. SCTP support requires careful implementation
4. Template lifecycle management non-trivial
5. Standard library = standard compliance

---

## 🔗 Related Links

- **Main TODO**: [`TODO_RFC7011_COMPLIANT.md`](./TODO_RFC7011_COMPLIANT.md)
- **Legacy PoC**: [`tests/my-SAV-ipfix-test/`](./tests/my-SAV-ipfix-test/)
- **Session Summary**: [`SESSION_SUMMARY_20251208.md`](./SESSION_SUMMARY_20251208.md)

---

## 📞 Getting Started

1. ✅ Read this README
2. ✅ Install libfixbuf (Phase 0 instructions above)
3. ✅ Read `TODO_RFC7011_COMPLIANT.md` for detailed plan
4. ✅ Start with Phase 0: Library evaluation
5. 🚀 Build the future of SAV monitoring!

---

**Last Updated**: 2025-12-08  
**Status**: Planning → Implementation  
**Next Action**: Install libfixbuf and start Phase 0!
