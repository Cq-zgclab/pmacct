---
title: Export of Source Address Validation (SAV) Information in IPFIX
abbrev: SAV IPFIX
docname: draft-cao-opsawg-ipfix-sav-01

## Key Definitions from Draft

### SAV IPFIX Information Elements (Section 4)

**IMPORTANT**: These are IANA-assigned IEs, NOT enterprise fields (no PEN)

| IE Name | IE ID | Length | Type | Description |
|---------|-------|--------|------|-------------|
| savRuleType | **TBD1** | 1 | unsigned8 | 0=allowlist, 1=blocklist |
| savTargetType | **TBD2** | 1 | unsigned8 | 0=interface-based, 1=prefix-based |
| savMatchedContentList | **TBD3** | variable | subTemplateList | SAV rule content |
| savPolicyAction | **TBD4** | 1 | unsigned8 | 0=permit, 1=discard, 2=rate-limit, 3=redirect |

### Sub-Template IDs (Appendix A)

These are correctly defined:
- **901**: IPv4 Interface-to-Prefix (ingressInterface, sourceIPv4Prefix, sourceIPv4PrefixLength)
- **902**: IPv6 Interface-to-Prefix (ingressInterface, sourceIPv6Prefix, sourceIPv6PrefixLength)  
- **903**: IPv4 Prefix-to-Interface (sourceIPv4Prefix, sourceIPv4PrefixLength, ingressInterface)
- **904**: IPv6 Prefix-to-Interface (sourceIPv6Prefix, sourceIPv6PrefixLength, ingressInterface)

### Critical Notes

1. **No Enterprise Encoding**: Draft uses standard IANA IE space (no PEN, no enterprise bit)
2. **TBD Values**: IE numbers TBD1-TBD4 will be assigned by IANA when draft is approved
3. **For Testing**: Use placeholder values until IANA assignment
