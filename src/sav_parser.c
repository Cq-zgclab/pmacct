/*
    pmacct (Promiscuous mode IP Accounting package)
    pmacct is Copyright (C) 2003-2025 by Paolo Lucente
*/

/*
    SAV (Source Address Validation) Parser
    RFC 6313 subTemplateList implementation for draft-cao-opsawg-ipfix-sav-01
*/

#include "pmacct.h"
#include "../include/sav_parser.h"
#include "nfv9_template.h"
#include <arpa/inet.h>

/**
 * Decode RFC 7011 variable-length field
 * Returns the decoded length and advances the data pointer
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
                                  struct sav_rule **rules, int *count) {
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
    
    /* Decode variable-length total length */
    total_len = decode_varlen(&ptr, &remaining);
    if (total_len == 0 || total_len > remaining) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): invalid varlen %u (remaining %u)\n",
            config.name, total_len, remaining);
        return -1;
    }
    
    /* Read semantic (1 byte) */
    if (remaining < 1) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): insufficient data for semantic\n",
            config.name);
        return -1;
    }
    semantic = *ptr++;
    remaining--;
    
    /* Read sub-template ID (2 bytes) */
    if (remaining < 2) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): insufficient data for template ID\n",
            config.name);
        return -1;
    }
    sub_tpl_id = ntohs(*(uint16_t*)ptr);
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
    /* total_len includes semantic(1) + template_id(2) + records */
    if (total_len < 3) {
        Log(LOG_WARNING, "WARN ( %s/core ): parse_sav_sub_template_list(): total_len too small %u\n",
            config.name, total_len);
        return -1;
    }
    
    num_rules = (total_len - 3) / record_size;
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
