/*
    pmacct (Promiscuous mode IP Accounting package)
    pmacct is Copyright (C) 2003-2025 by Paolo Lucente
*/

/*
 * SAV (Source Address Validation) Parser
 * 
 * PURPOSE:
 *   Parse RFC 6313 subTemplateList structures containing SAV rules from
 *   IPFIX messages per draft-cao-opsawg-ipfix-sav-01.
 *
 * FUNCTIONALITY:
 *   - Decode RFC 7011 variable-length fields
 *   - Parse subTemplateList semantic field and template ID
 *   - Extract SAV rules from 4 sub-template formats (901-904)
 *   - Convert rules to human-readable strings for debugging
 *
 * SUPPORTED SUB-TEMPLATES:
 *   901: IPv4 Interface→Prefix (interface_id + ipv4_prefix + prefix_len)
 *   902: IPv6 Interface→Prefix (interface_id + ipv6_prefix + prefix_len)
 *   903: IPv4 Prefix→Interface (ipv4_prefix + prefix_len + interface_id)
 *   904: IPv6 Prefix→Interface (ipv6_prefix + prefix_len + interface_id)
 *
 * COMPLIANCE:
 *   RFC 6313 - Export of Structured Data in IPFIX
 *   RFC 7011 - IPFIX Protocol Specification (variable-length encoding)
 *   draft-cao-opsawg-ipfix-sav-01
 *
 * AUTHOR:
 *   Generated for pmacct SAV IPFIX integration (Hackathon MVP)
 *
 * VERSION:
 *   1.0 (2025-12-05)
 */

#include "pmacct.h"
#include "../include/sav_parser.h"
#include "nfv9_template.h"
#include <arpa/inet.h>

/*
 * decode_varlen - Decode RFC 7011 variable-length field
 * 
 * Encoding rules:
 *   - If length < 255: encoded as single byte
 *   - If length >= 255: first byte = 0xFF, followed by 2-byte length (network order)
 *
 * @param data      Pointer to data pointer (will be advanced)
 * @param remaining Pointer to remaining bytes counter (will be decremented)
 * @return Decoded length value, or 0 on error
 */
static uint16_t decode_varlen(u_char **data, uint16_t *remaining) {
    uint16_t len;
    
    if (*remaining < 1) {
        return 0;
    }
    
    len = **data;
    (*data)++;
    (*remaining)--;
    
    /* RFC 7011: if first byte == 255, next 2 bytes contain actual length */
    if (len == 255) {
        if (*remaining < 2) {
            return 0;
        }
        len = ntohs(*(uint16_t*)(*data));
        (*data) += 2;
        (*remaining) -= 2;
    }
    
    return len;
}

/**
 * Parse a single SAV rule from sub-template record
 */
int parse_sav_rule(u_char *data, uint16_t template_id, struct sav_rule *rule) {
    int bytes_consumed = 0;
    u_char *ptr = data;
    
    if (!data || !rule) {
        return -1;
    }
    
    memset(rule, 0, sizeof(struct sav_rule));
    
    switch (template_id) {
        case SAV_TPL_IPV4_IF2PREFIX:  /* 901: interface_id, ipv4_prefix, prefix_len */
            rule->interface_id = ntohl(*(uint32_t*)ptr);
            ptr += 4;
            rule->prefix.ipv4[0] = ntohl(*(uint32_t*)ptr);
            ptr += 4;
            rule->prefix_len = *ptr;
            ptr += 1;
            bytes_consumed = 9;
            break;
            
        case SAV_TPL_IPV6_IF2PREFIX:  /* 902: interface_id, ipv6_prefix, prefix_len */
            rule->interface_id = ntohl(*(uint32_t*)ptr);
            ptr += 4;
            memcpy(rule->prefix.ipv6, ptr, 16);
            ptr += 16;
            rule->prefix_len = *ptr;
            ptr += 1;
            bytes_consumed = 21;
            break;
            
        case SAV_TPL_IPV4_PREFIX2IF:  /* 903: ipv4_prefix, prefix_len, interface_id */
            rule->prefix.ipv4[0] = ntohl(*(uint32_t*)ptr);
            ptr += 4;
            rule->prefix_len = *ptr;
            ptr += 1;
            rule->interface_id = ntohl(*(uint32_t*)ptr);
            ptr += 4;
            bytes_consumed = 9;
            break;
            
        case SAV_TPL_IPV6_PREFIX2IF:  /* 904: ipv6_prefix, prefix_len, interface_id */
            memcpy(rule->prefix.ipv6, ptr, 16);
            ptr += 16;
            rule->prefix_len = *ptr;
            ptr += 1;
            rule->interface_id = ntohl(*(uint32_t*)ptr);
            ptr += 4;
            bytes_consumed = 21;
            break;
            
        default:
            Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_rule(): unknown template ID %u\n",
                config.name, template_id);
            return -1;
    }
    
    return bytes_consumed;
}

/**
 * Parse RFC 6313 subTemplateList
 */
int parse_sav_sub_template_list(u_char *data, uint16_t len, uint8_t validation_mode,
                                  struct sav_rule **rules, int *count, uint16_t *template_id_out) {
    u_char *ptr = data;
    uint16_t remaining = len;
    uint16_t total_len, sub_tpl_id, record_size;
    uint8_t semantic;
    int num_rules, i;
    
    if (!data || !rules || !count) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): NULL parameters\n",
            config.name);
        return -1;
    }
    
    *rules = NULL;
    *count = 0;
    
    /* NOTE: Variable-length encoding already decoded by caller.
     * data points to semantic field, len is the total subTemplateList content length.
     */
    
    /* Read semantic (1 byte) */
    if (remaining < 1) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): insufficient data for semantic\n",
            config.name);
        return -1;
    }
    semantic = *ptr++;
    remaining--;
    
    if (config.debug) {
        Log(LOG_DEBUG, "DEBUG ( %s/core ): SAV subTemplateList semantic: 0x%02x\n",
            config.name, semantic);
    }
    
    /* Read sub-template ID (2 bytes) */
    if (remaining < 2) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): insufficient data for template ID\n",
            config.name);
        return -1;
    }
    
    if (config.debug) {
        Log(LOG_DEBUG, "DEBUG ( %s/core ): Template ID bytes: [0]=0x%02x [1]=0x%02x\n",
            config.name, ptr[0], ptr[1]);
    }
    
    sub_tpl_id = ntohs(*(uint16_t*)ptr);
    
    if (config.debug) {
        Log(LOG_DEBUG, "DEBUG ( %s/core ): SAV sub-template ID after ntohs: %u (0x%04x)\n",
            config.name, sub_tpl_id, sub_tpl_id);
    }
    
    ptr += 2;
    remaining -= 2;
    
    /* Validate sub-template ID */
    if (sub_tpl_id < SAV_TPL_IPV4_IF2PREFIX || sub_tpl_id > SAV_TPL_IPV6_PREFIX2IF) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): invalid sub-template ID %u\n",
            config.name, sub_tpl_id);
        return -1;
    }
    
    /* Determine record size based on template ID */
    switch (sub_tpl_id) {
        case SAV_TPL_IPV4_IF2PREFIX:  /* 901 */
        case SAV_TPL_IPV4_PREFIX2IF:  /* 903 */
            record_size = 9;   /* 4 + 4 + 1 */
            break;
        case SAV_TPL_IPV6_IF2PREFIX:  /* 902 */
        case SAV_TPL_IPV6_PREFIX2IF:  /* 904 */
            record_size = 21;  /* 4 + 16 + 1 */
            break;
        default:
            return -1;
    }
    
    /* Calculate number of rules */
    /* len includes semantic(1) + template_id(2) + records */
    if (len < 3) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): len too small %u\n",
            config.name, len);
        return -1;
    }
    
    num_rules = (len - 3) / record_size;
    if (num_rules <= 0) {
        Log(LOG_INFO, "INFO ( %s/core ): parse_sav_sub_template_list(): no SAV rules in subTemplateList\n",
            config.name);
        return 0;
    }
    
    /* Allocate rules array */
    *rules = malloc(num_rules * sizeof(struct sav_rule));
    if (!*rules) {
        Log(LOG_ERR, "ERROR ( %s/core ): parse_sav_sub_template_list(): unable to allocate rules array\n",
            config.name);
        return -1;
    }
    
    /* Parse each rule */
    for (i = 0; i < num_rules; i++) {
        int consumed = parse_sav_rule(ptr, sub_tpl_id, &(*rules)[i]);
        if (consumed < 0) {
            Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): failed to parse rule %d\n",
                config.name, i);
            free(*rules);
            *rules = NULL;
            return -1;
        }
        
        /* Set validation mode from parent template */
        (*rules)[i].validation_mode = validation_mode;
        
        ptr += consumed;
        remaining -= consumed;
    }
    
    *count = num_rules;
    
    /* Return template ID if requested */
    if (template_id_out) {
        *template_id_out = sub_tpl_id;
    }
    
    Log(LOG_DEBUG, "DEBUG ( %s/core ): parse_sav_sub_template_list(): parsed %d rules (semantic=%u, tpl_id=%u)\n",
        config.name, num_rules, semantic, sub_tpl_id);
    
    return 0;
}

/**
 * Free allocated SAV rules
 */
void free_sav_rules(struct sav_rule *rules) {
    if (rules) {
        free(rules);
    }
}

/**
 * Convert SAV rule to string
 */
int sav_rule_to_string(struct sav_rule *rule, uint16_t template_id, char *buf, size_t buf_len) {
    char ip_str[INET6_ADDRSTRLEN];
    int written = 0;
    
    if (!rule || !buf || buf_len == 0) {
        return -1;
    }
    
    /* Determine if IPv4 or IPv6 */
    if (template_id == SAV_TPL_IPV4_IF2PREFIX || template_id == SAV_TPL_IPV4_PREFIX2IF) {
        struct in_addr addr;
        addr.s_addr = htonl(rule->prefix.ipv4[0]);
        inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
    } else {
        inet_ntop(AF_INET6, rule->prefix.ipv6, ip_str, sizeof(ip_str));
    }
    
    written = snprintf(buf, buf_len, "interface=%u prefix=%s/%u mode=%u",
                       rule->interface_id, ip_str, rule->prefix_len, rule->validation_mode);
    
    return written;
}
