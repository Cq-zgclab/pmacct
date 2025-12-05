/*
    pmacct (Promiscuous mode IP Accounting package)
    pmacct is Copyright (C) 2003-2025 by Paolo Lucente
*/

/*
    SAV (Source Address Validation) Parser
    RFC 6313 subTemplateList implementation for draft-cao-opsawg-ipfix-sav-01
*/

#ifndef SAV_PARSER_H
#define SAV_PARSER_H

/* SAV IPFIX Information Elements (Enterprise ID: 45575) */
#define SAV_ENTERPRISE_ID               45575

/* SAV Information Element IDs (draft-cao-opsawg-ipfix-sav-01) */
#define SAV_IE_RULE_TYPE                900   /* sav_rule_type */
#define SAV_IE_TARGET_TYPE              901   /* sav_target_type */
#define SAV_IE_MATCHED_CONTENT          902   /* sav_matched_content (subTemplateList) */
#define SAV_IE_POLICY_ACTION            903   /* sav_policy_action */

/* Legacy compatibility */
#define SAV_RULE_TYPE                   SAV_IE_RULE_TYPE
#define SAV_TARGET_TYPE                 SAV_IE_TARGET_TYPE
#define SAV_MATCHED_CONTENT             SAV_IE_MATCHED_CONTENT
#define SAV_POLICY_ACTION               SAV_IE_POLICY_ACTION

/* SAV Sub-Template IDs */
#define SAV_TPL_IPV4_IF2PREFIX          901  /* interface_id, ipv4_prefix, prefix_len */
#define SAV_TPL_IPV6_IF2PREFIX          902  /* interface_id, ipv6_prefix, prefix_len */
#define SAV_TPL_IPV4_PREFIX2IF          903  /* ipv4_prefix, prefix_len, interface_id */
#define SAV_TPL_IPV6_PREFIX2IF          904  /* ipv6_prefix, prefix_len, interface_id */

/* SAV Validation Modes */
#define SAV_MODE_INTERFACE_TO_PREFIX    0    /* ACL-based */
#define SAV_MODE_PREFIX_TO_INTERFACE    1    /* uRPF */
#define SAV_MODE_PREFIX_TO_AS           2    /* BGP AS Path */
#define SAV_MODE_INTERFACE_TO_AS        3    /* BGP Peer */

/* SAV Rule Structure */
struct sav_rule {
    uint32_t interface_id;          /* Interface ID */
    union {
        uint32_t ipv4[4];           /* IPv4 prefix (host order, only first element used) */
        uint8_t ipv6[16];           /* IPv6 prefix */
    } prefix;
    uint8_t prefix_len;             /* Prefix length */
    uint8_t validation_mode;        /* Validation mode from main template */
};

/* Function Prototypes */

/**
 * Parse RFC 6313 subTemplateList from binary data
 * 
 * @param data           Pointer to subTemplateList data
 * @param len            Total length of data
 * @param validation_mode Validation mode from parent template field
 * @param rules          Output: pointer to allocated array of rules
 * @param count          Output: number of rules parsed
 * @return 0 on success, -1 on error
 */
int parse_sav_sub_template_list(u_char *data, uint16_t len, uint8_t validation_mode,
                                  struct sav_rule **rules, int *count);

/**
 * Parse a single SAV rule from a sub-template record
 * 
 * @param data           Pointer to record data
 * @param template_id    Sub-template ID (901-904)
 * @param rule           Output: parsed rule structure
 * @return Number of bytes consumed, or -1 on error
 */
int parse_sav_rule(u_char *data, uint16_t template_id, struct sav_rule *rule);

/**
 * Free allocated SAV rules array
 * 
 * @param rules          Array to free
 */
void free_sav_rules(struct sav_rule *rules);

/**
 * Convert SAV rule to human-readable string
 * 
 * @param rule           Rule to convert
 * @param template_id    Sub-template ID (for field order)
 * @param buf            Output buffer
 * @param buf_len        Buffer length
 * @return Number of chars written, or -1 on error
 */
int sav_rule_to_string(struct sav_rule *rule, uint16_t template_id, char *buf, size_t buf_len);

#endif /* SAV_PARSER_H */
