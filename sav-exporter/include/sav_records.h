/*
 * sav_records.h
 * 
 * C struct definitions for SAV IPFIX records
 * These structs map directly to IPFIX templates
 */

#ifndef SAV_RECORDS_H
#define SAV_RECORDS_H

#include <fixbuf/public.h>
#include <stdint.h>

/*
 * Main SAV Record (Template 256)
 * 
 * This is exported by sav-exporter and contains a SubTemplateList
 * of matched SAV rules
 */
typedef struct sav_main_record_st {
    uint16_t             rule_type;      /* SAV_IE_RULE_TYPE: 901-904 (needs 2 bytes) */
    uint8_t              target_type;    /* SAV_IE_TARGET_TYPE: 0=IPv4, 1=IPv6 */
    fbSubTemplateList_t  content_list;   /* SAV_IE_MATCHED_CONTENT_LIST */
    uint8_t              match_count;    /* SAV_IE_MATCH_COUNT */
} sav_main_record_t;

/*
 * SAV Rule 901: IPv4 Interface-to-Prefix
 * 
 * Validates that packets from specific interface have expected source prefix
 * Total: 9 bytes
 */
typedef struct sav_rule_901_st {
    uint32_t  interface_id;    /* IPFIX IE: ingressInterface */
    uint32_t  ipv4_prefix;     /* IPFIX IE: sourceIPv4Address (network byte order) */
    uint8_t   prefix_len;      /* IPFIX IE: sourceIPv4PrefixLength */
} sav_rule_901_t;

/*
 * SAV Rule 902: IPv6 Interface-to-Prefix
 * 
 * IPv6 version of rule 901
 * Total: 21 bytes
 */
typedef struct sav_rule_902_st {
    uint32_t  interface_id;    /* IPFIX IE: ingressInterface */
    uint8_t   ipv6_prefix[16]; /* IPFIX IE: sourceIPv6Address */
    uint8_t   prefix_len;      /* IPFIX IE: sourceIPv6PrefixLength */
} sav_rule_902_t;

/*
 * SAV Rule 903: IPv4 Prefix-to-Interface
 * 
 * Validates that packets from specific prefix arrive on expected interface
 * Total: 9 bytes (same structure as 901, different semantic)
 */
typedef struct sav_rule_903_st {
    uint32_t  interface_id;    /* IPFIX IE: ingressInterface */
    uint32_t  ipv4_prefix;     /* IPFIX IE: sourceIPv4Address */
    uint8_t   prefix_len;      /* IPFIX IE: sourceIPv4PrefixLength */
} sav_rule_903_t;

/*
 * SAV Rule 904: IPv6 Prefix-to-Interface
 * 
 * IPv6 version of rule 903
 * Total: 21 bytes
 */
typedef struct sav_rule_904_st {
    uint32_t  interface_id;    /* IPFIX IE: ingressInterface */
    uint8_t   ipv6_prefix[16]; /* IPFIX IE: sourceIPv6Address */
    uint8_t   prefix_len;      /* IPFIX IE: sourceIPv6PrefixLength */
} sav_rule_904_t;

/*
 * Helper function to initialize a main record for export
 * 
 * @param record      Pointer to sav_main_record_t to initialize
 * @param rule_type   SAV rule type (901-904)
 * @param target_type Target address type (0=IPv4, 1=IPv6)
 */
void sav_main_record_init(sav_main_record_t *record, 
                          uint16_t rule_type, 
                          uint8_t target_type);

/*
 * Helper function to clear a main record after export
 * Frees SubTemplateList memory
 * 
 * @param record  Pointer to sav_main_record_t to clear
 */
void sav_main_record_clear(sav_main_record_t *record);

#endif /* SAV_RECORDS_H */
