# 🚀 SAV IPFIX Quick Start Guide

Choose your implementation path in **30 seconds**.

## I want to...

### 📊 **Demo SAV IPFIX in a hackathon/presentation**
```bash
cd sav-ipfix/hackathon-ipfixcol2
cat README.md  # Read the guide
./run_collector.sh  # Start ipfixcol2
python3 parse_subtemplatelist.py test_data.json  # Parse results
```
✅ No compilation  
✅ Works in 5 minutes  
⚠️ No SCTP support  

---

### 🏭 **Deploy to production with RFC 7011 compliance**
```bash
cd sav-ipfix/production-libfixbuf
cat README.md  # Read the guide
make  # Compile
LD_LIBRARY_PATH=/usr/local/lib ./sav_collector --listen=sctp://0.0.0.0:4739
```
✅ Full SCTP support  
✅ High performance (C)  
⚠️ Requires libfixbuf build  

---

### 📚 **Understand the architecture**
```bash
cat sav-ipfix/README.md
# Compares both approaches, feature matrix, migration guide
```

---

### 🔍 **See implementation details**
- **Hackathon**: `sav-ipfix/hackathon-ipfixcol2/README.md`
- **Production**: `sav-ipfix/production-libfixbuf/README.md`
- **Overview**: `README_SAV_IPFIX.md`

---

## Still confused?

**Start here**: [`README_SAV_IPFIX.md`](README_SAV_IPFIX.md) → Complete overview with navigation

**Need help?**
1. Check `sav-ipfix/README.md` for comparison
2. Read implementation-specific README
3. See `docs/draft-cao-opsawg-ipfix-sav-01.md` for spec

---

## File Structure at a Glance

```
pmacct/
├── README_SAV_IPFIX.md          ← Start here
├── QUICKSTART_SAV.md            ← You are here
└── sav-ipfix/
    ├── README.md                ← Architecture & comparison
    ├── hackathon-ipfixcol2/     ← Python implementation
    │   └── README.md            ← Hackathon guide
    └── production-libfixbuf/    ← C implementation  
        └── README.md            ← Production guide
```

**Pick your path → Read its README → Go!** 🎯
