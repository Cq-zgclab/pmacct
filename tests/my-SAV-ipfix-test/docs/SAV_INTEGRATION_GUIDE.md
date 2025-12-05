# SAV Parser Integration Guide

## Overview

This document describes how to integrate the SAV (Source Address Validation) parser into pmacct's nfacctd data flow to parse RFC 6313 subTemplateList fields.

## Architecture

### Data Flow

```
IPFIX Packet → Template Processing → Data Record Processing → Plugin Output
                     ↓                         ↓
              Template Cache          SAV Parser (if sav_matched_content present)
              (901-904 cached)               ↓
                                      Parsed SAV Rules → Custom Primitive Storage
```

### Key Components

1. **Template Cache** (`src/nfv9_template.c`)
   - Sub-templates 901-904 automatically cached via `compose_template()`
   - No special handling needed - pmacct's architecture supports any template ID

2. **SAV Parser** (`src/sav_parser.c`)
   - `parse_sav_sub_template_list()`: Main entry point
   - Handles RFC 6313 subTemplateList structure
   - Returns array of `struct sav_rule`

3. **Data Processing** (`src/nfacctd.c`)
   - Check for SAV enterprise fields (PEN 45575)
   - Call SAV parser when `sav_matched_content` (IE 902) found
   - Store results in packet_ptrs or custom primitives

## Integration Steps

### Step 1: Detect SAV Fields in Template

When processing data records, check if the template contains SAV fields:

```c
// In nfacctd.c data processing loop
struct utpl_field *sav_matched_content_field = NULL;
struct utpl_field *sav_validation_mode_field = NULL;

// Look for SAV enterprise fields (PEN 45575)
sav_matched_content_field = ext_db_get_ie(tpl, SAV_ENTERPRISE_ID, SAV_IE_MATCHED_CONTENT, 0);
sav_validation_mode_field = ext_db_get_ie(tpl, SAV_ENTERPRISE_ID, SAV_IE_RULE_TYPE, 0);
```

### Step 2: Extract Field Data

```c
if (sav_matched_content_field && sav_matched_content_field->len > 0) {
    u_char *sav_data = pkt + sav_matched_content_field->off;
    uint16_t sav_len = sav_matched_content_field->len;
    uint8_t validation_mode = 0;
    
    // Get validation mode if present
    if (sav_validation_mode_field && sav_validation_mode_field->len == 1) {
        validation_mode = *(pkt + sav_validation_mode_field->off);
    }
    
    // ... continue to step 3
}
```

### Step 3: Call SAV Parser

```c
#include "sav_parser.h"

struct sav_rule *rules = NULL;
int rule_count = 0;
int ret;

ret = parse_sav_sub_template_list(sav_data, sav_len, validation_mode, 
                                   &rules, &rule_count);
if (ret == 0 && rules != NULL) {
    Log(LOG_DEBUG, "DEBUG ( %s/core ): Parsed %d SAV rules\n", 
        config.name, rule_count);
    
    // Process rules...
    for (int i = 0; i < rule_count; i++) {
        char rule_str[256];
        sav_rule_to_string(&rules[i], sub_tpl_id, rule_str, sizeof(rule_str));
        Log(LOG_DEBUG, "DEBUG ( %s/core ): SAV rule %d: %s\n", 
            config.name, i, rule_str);
    }
    
    // Free rules when done
    free_sav_rules(rules);
}
```

### Step 4: Store Results (Option A - Debug Logging Only)

For initial testing, simply log the parsed rules:

```c
if (ret == 0 && rules != NULL) {
    for (int i = 0; i < rule_count; i++) {
        char ip_str[INET6_ADDRSTRLEN];
        
        // Determine IPv4 or IPv6
        if (sub_tpl_id == SAV_TPL_IPV4_IF2PREFIX || 
            sub_tpl_id == SAV_TPL_IPV4_PREFIX2IF) {
            struct in_addr addr;
            addr.s_addr = htonl(rules[i].prefix.ipv4[0]);
            inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
        } else {
            inet_ntop(AF_INET6, rules[i].prefix.ipv6, ip_str, sizeof(ip_str));
        }
        
        Log(LOG_INFO, "INFO ( %s/core ): SAV Rule: if=%u prefix=%s/%u mode=%u\n",
            config.name, rules[i].interface_id, ip_str, 
            rules[i].prefix_len, rules[i].validation_mode);
    }
    
    free_sav_rules(rules);
}
```

### Step 4: Store Results (Option B - Custom Primitive)

For structured storage and JSON output:

```c
// TODO: Requires custom primitive definition
// Add to pkt_primitives structure:
struct sav_rule *sav_rules;
int sav_rule_count;

// Store in packet_ptrs
pptrs->sav_rules = rules;
pptrs->sav_rule_count = rule_count;

// Note: Need to free in cleanup code
```

## Code Locations

### Files to Modify

1. **`src/nfacctd.c`** (primary integration point)
   - Function: `process_v9_packet()` or data record processing loop
   - Lines: ~2400-2800 (data record processing section)
   - Add: SAV field detection and parser calls

2. **`src/network.h`** (optional - if storing in packet_ptrs)
   - Structure: `struct packet_ptrs`
   - Add: `struct sav_rule *sav_rules; int sav_rule_count;`

3. **`src/plugin_cmn_json.c`** (Phase 1B.3 - JSON output)
   - Add SAV rules serialization to JSON output

### Required Includes

Add to `src/nfacctd.c`:

```c
#include "sav_parser.h"
```

## Testing Strategy

### Phase 1: Debug Logging (Current)

1. Add SAV parser calls with LOG_INFO output
2. Send test IPFIX messages from Python sender
3. Verify parsed rules appear in nfacctd logs

```bash
# Send test message
cd /workspaces/pmacct/tests/my-SAV-ipfix-test
python3 scripts/send_ipfix_with_ip.py \
  --sav-rules test-data/sav_rules_example.json \
  --sub-template-id 901 \
  --use-complete-message

# Check logs
tail -f /tmp/nfacctd.log | grep SAV
```

### Phase 2: JSON Output

1. Modify JSON plugin to serialize SAV rules
2. Verify structured output in print plugin

Expected output:
```json
{
  "sav_validation_mode": "interface-to-prefix",
  "sav_matched_rules": [
    {"interface_id": 5001, "prefix": "198.51.100.0/24"},
    {"interface_id": 5002, "prefix": "203.0.113.0/24"}
  ]
}
```

## Error Handling

The SAV parser includes comprehensive error handling:

- **Invalid varlen encoding**: Returns -1, logs warning
- **Unknown sub-template ID**: Returns -1, logs warning with template ID
- **Insufficient data**: Returns -1, logs remaining bytes
- **Allocation failure**: Returns -1, logs error

All errors are logged with `config.name` context for debugging.

## Memory Management

**Critical**: Always free SAV rules after use:

```c
if (rules) {
    free_sav_rules(rules);
    rules = NULL;
}
```

If storing in `packet_ptrs`, add cleanup in appropriate location (e.g., packet processing end or plugin callback completion).

## Performance Considerations

- **Parsing overhead**: ~10-50 μs per rule (depends on CPU)
- **Memory**: 24 bytes per IPv4 rule, 36 bytes per IPv6 rule
- **Template cache**: Sub-templates 901-904 cached once per exporter
- **No recursion overhead**: Parser uses iterative approach

## Future Enhancements

1. **Custom Primitive Support**
   - Define `sav_matched_content` in primitives list
   - Add to `struct pkt_primitives`
   - Enable aggregation by SAV rules

2. **Filter Support**
   - Add SAV-specific filters (e.g., `sav_interface_id`, `sav_prefix`)
   - Enable filtering flows by SAV validation mode

3. **Statistics**
   - Count SAV rules processed
   - Track validation modes distribution

## References

- RFC 7011: IPFIX Protocol Specification
- RFC 6313: Export of Structured Data in IPFIX
- draft-cao-opsawg-ipfix-sav-01: SAV Information Elements
- pmacct docs: https://github.com/pmacct/pmacct/wiki
