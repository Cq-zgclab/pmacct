# SAV Test Data Files

This directory contains JSON rule files for testing all 4 SAV sub-templates.

## File Descriptions

### Template 901: IPv4 Interface-to-Prefix Mapping
**File**: `sav_rules_example.json`
- **Purpose**: Interface-based allowlist for IPv4 prefixes
- **Scenario**: Interface 5001 permits 3 IPv4 prefixes
- **Fields**: ingressInterface (4B), sourceIPv4Prefix (4B), sourceIPv4PrefixLength (1B)
- **Record Size**: 9 bytes per rule
- **Usage**:
  ```bash
  ../scripts/send_ipfix_with_ip.py \
    --sav-rules sav_rules_example.json \
    --sub-template-id 901 \
    --use-complete-message
  ```

### Template 902: IPv6 Interface-to-Prefix Mapping
**File**: `sav_rules_ipv6_example.json`
- **Purpose**: Interface-based allowlist for IPv6 prefixes
- **Scenario**: Interface 5002 permits 2 IPv6 prefixes
- **Fields**: ingressInterface (4B), sourceIPv6Prefix (16B), sourceIPv6PrefixLength (1B)
- **Record Size**: 21 bytes per rule

### Template 903: IPv4 Prefix-to-Interface Mapping
**File**: `sav_rules_prefix2if_ipv4.json`
- **Purpose**: Prefix-based blocklist for IPv4
- **Scenario**: 2 IPv4 prefixes can only enter via interface 5001
- **Fields**: sourceIPv4Prefix (4B), sourceIPv4PrefixLength (1B), ingressInterface (4B)
- **Record Size**: 9 bytes per rule

### Template 904: IPv6 Prefix-to-Interface Mapping
**File**: `sav_rules_prefix2if_ipv6.json`
- **Purpose**: Prefix-based blocklist for IPv6
- **Scenario**: 2 IPv6 prefixes can only enter via interface 5003
- **Fields**: sourceIPv6Prefix (16B), sourceIPv6PrefixLength (1B), ingressInterface (4B)
- **Record Size**: 21 bytes per rule

## JSON Format

```json
[
  {
    "interface_id": 5001,
    "prefix": "198.51.100.0",
    "prefix_len": 24
  }
]
```

Or with metadata wrapper:

```json
{
  "_comment": "Description",
  "rules": [...]
}
```

## Standards Reference

- **RFC 6313**: Export of Structured Data in IPFIX
- **draft-cao-opsawg-ipfix-sav-01**: Appendix A (Sub-Template Definitions)
