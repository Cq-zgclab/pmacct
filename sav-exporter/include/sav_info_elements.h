/*
 * sav_info_elements.h
 * 
 * SAV (Source Address Validation) Information Element Definitions
 * for IPFIX export using libfixbuf
 * 
 * Based on CERT NetSA libfixbuf patterns
 */

#ifndef SAV_INFO_ELEMENTS_H
#define SAV_INFO_ELEMENTS_H

#include <fixbuf/public.h>

/* 
 * Private Enterprise Number (PEN)
 * TODO: Register with IANA - currently using private range
 */
#define SAV_PEN 9999999

/*
 * SAV Information Element IDs (within our PEN namespace)
 */
#define SAV_IE_RULE_TYPE              1
#define SAV_IE_TARGET_TYPE            2
#define SAV_IE_MATCHED_CONTENT_LIST   3
#define SAV_IE_MATCH_COUNT            4

/*
 * SAV Template IDs
 */
#define TMPL_SAV_MAIN           256   /* Main record with SubTemplateList */
#define TMPL_SAV_RULE_901       901   /* IPv4 Interface-to-Prefix */
#define TMPL_SAV_RULE_902       902   /* IPv6 Interface-to-Prefix */
#define TMPL_SAV_RULE_903       903   /* IPv4 Prefix-to-Interface */
#define TMPL_SAV_RULE_904       904   /* IPv6 Prefix-to-Interface */

/*
 * SAV Rule Types (values for savRuleType IE)
 * Note: Using defines instead of enum to avoid type overflow
 */
#define SAV_RULE_TYPE_901  901  /* IPv4 Interface-to-Prefix */
#define SAV_RULE_TYPE_902  902  /* IPv6 Interface-to-Prefix */
#define SAV_RULE_TYPE_903  903  /* IPv4 Prefix-to-Interface */
#define SAV_RULE_TYPE_904  904  /* IPv6 Prefix-to-Interface */

/*
 * Target Address Types (enum values for savTargetType IE)
 */
typedef enum {
    SAV_TARGET_TYPE_IPV4 = 0,
    SAV_TARGET_TYPE_IPV6 = 1
} sav_target_type_t;

/*
 * Information Element Registry
 * 
 * This array defines all SAV custom IEs to be registered with libfixbuf
 * Use FB_IE_INIT_FULL macro for full control over IE properties
 */
extern fbInfoElement_t sav_info_elements[];

/*
 * Register SAV Information Elements with libfixbuf InfoModel
 * 
 * @param model  The fbInfoModel to add IEs to
 * @return TRUE on success, FALSE on error
 */
gboolean sav_register_info_elements(fbInfoModel_t *model);

/*
 * Create SAV template for main record (Template 256)
 * Contains: savRuleType, savTargetType, savMatchedContentList, savMatchCount
 * 
 * @param model  The fbInfoModel with SAV IEs registered
 * @return       Newly allocated fbTemplate_t, caller must free
 */
fbTemplate_t *sav_create_main_template(fbInfoModel_t *model);

/*
 * Create SAV template for Rule 901 (IPv4 Interface-to-Prefix)
 * Contains: interface_id (4 bytes), ipv4_prefix (4 bytes), prefix_len (1 byte)
 * 
 * @param model  The fbInfoModel with standard IPFIX IEs
 * @return       Newly allocated fbTemplate_t, caller must free
 */
fbTemplate_t *sav_create_rule_901_template(fbInfoModel_t *model);

/*
 * Create SAV template for Rule 902 (IPv6 Interface-to-Prefix)
 * Contains: interface_id (4 bytes), ipv6_prefix (16 bytes), prefix_len (1 byte)
 * 
 * @param model  The fbInfoModel with standard IPFIX IEs
 * @return       Newly allocated fbTemplate_t, caller must free
 */
fbTemplate_t *sav_create_rule_902_template(fbInfoModel_t *model);

/*
 * Create SAV template for Rule 903 (IPv4 Prefix-to-Interface)
 * Same structure as 901, different semantic meaning
 */
fbTemplate_t *sav_create_rule_903_template(fbInfoModel_t *model);

/*
 * Create SAV template for Rule 904 (IPv6 Prefix-to-Interface)
 * Same structure as 902, different semantic meaning
 */
fbTemplate_t *sav_create_rule_904_template(fbInfoModel_t *model);

#endif /* SAV_INFO_ELEMENTS_H */
