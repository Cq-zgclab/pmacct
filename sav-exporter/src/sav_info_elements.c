/*
 * sav_info_elements.c
 * 
 * Implementation of SAV Information Element registration
 */

#include <fixbuf/public.h>
#include "../include/sav_info_elements.h"

/*
 * SAV Information Elements Registry
 * 
 * Define all custom IEs using FB_IE_INIT_FULL macro
 * Format: name, PEN, IE_num, length, flags, min, max, type, description
 */
fbInfoElement_t sav_info_elements[] = {
    /* SAV Rule Type (901-904) */
    FB_IE_INIT_FULL("savRuleType", SAV_PEN, SAV_IE_RULE_TYPE, 1,
                    FB_IE_F_ENDIAN, 0, 0, FB_UINT_8,
                    "SAV rule type identifier"),
    
    /* SAV Target Address Type (0=IPv4, 1=IPv6) */
    FB_IE_INIT_FULL("savTargetType", SAV_PEN, SAV_IE_TARGET_TYPE, 1,
                    FB_IE_F_ENDIAN, 0, 0, FB_UINT_8,
                    "SAV target address type"),
    
    /* SAV Matched Content List (SubTemplateList) */
    FB_IE_INIT_FULL("savMatchedContentList", SAV_PEN, SAV_IE_MATCHED_CONTENT_LIST,
                    FB_IE_VARLEN, 0, 0, 0, FB_SUB_TMPL_LIST,
                    "List of matched SAV rules as SubTemplateList"),
    
    /* SAV Match Count */
    FB_IE_INIT_FULL("savMatchCount", SAV_PEN, SAV_IE_MATCH_COUNT, 1,
                    FB_IE_F_ENDIAN, 0, 0, FB_UINT_8,
                    "Number of matched SAV rules"),
    
    /* Sentinel - do not remove */
    FB_IE_NULL
};

/*
 * Register SAV Information Elements with InfoModel
 */
gboolean sav_register_info_elements(fbInfoModel_t *model)
{
    if (!model) {
        return FALSE;
    }
    
    /* Add all SAV IEs to the model */
    fbInfoModelAddElementArray(model, sav_info_elements);
    
    return TRUE;
}

/*
 * Create main SAV template (Template 256)
 * 
 * Using fbInfoElementSpec_t for cleaner template building
 */
fbTemplate_t *sav_create_main_template(fbInfoModel_t *model)
{
    fbTemplate_t *tmpl = NULL;
    GError *err = NULL;
    
    /* Define template spec array */
    fbInfoElementSpec_t spec[] = {
        { (char *)"savRuleType", 0, 0 },
        { (char *)"savTargetType", 0, 0 },
        { (char *)"savMatchedContentList", 0, 0 },
        { (char *)"savMatchCount", 0, 0 },
        FB_IESPEC_NULL
    };
    
    tmpl = fbTemplateAlloc(model);
    
    if (!fbTemplateAppendSpecArray(tmpl, spec, 0, &err)) {
        fprintf(stderr, "Failed to append spec: %s\n", err->message);
        g_clear_error(&err);
        fbTemplateFreeUnused(tmpl);
        return NULL;
    }
    
    return tmpl;
}

/*
 * Create SAV Rule 901 template (IPv4 Interface-to-Prefix)
 * 
 * Uses standard IPFIX IEs: ingressInterface, sourceIPv4Address, sourceIPv4PrefixLength
 */
fbTemplate_t *sav_create_rule_901_template(fbInfoModel_t *model)
{
    fbTemplate_t *tmpl = NULL;
    GError *err = NULL;
    
    fbInfoElementSpec_t spec[] = {
        { (char *)"ingressInterface", 4, 0 },         /* IE 10 */
        { (char *)"sourceIPv4Address", 4, 0 },        /* IE 8 */
        { (char *)"sourceIPv4PrefixLength", 1, 0 },   /* IE 9 */
        FB_IESPEC_NULL
    };
    
    tmpl = fbTemplateAlloc(model);
    
    if (!fbTemplateAppendSpecArray(tmpl, spec, 0, &err)) {
        fprintf(stderr, "Failed to create rule 901 template: %s\n", err->message);
        g_clear_error(&err);
        fbTemplateFreeUnused(tmpl);
        return NULL;
    }
    
    return tmpl;
}

/*
 * Create SAV Rule 902 template (IPv6 Interface-to-Prefix)
 */
fbTemplate_t *sav_create_rule_902_template(fbInfoModel_t *model)
{
    fbTemplate_t *tmpl = NULL;
    GError *err = NULL;
    
    fbInfoElementSpec_t spec[] = {
        { (char *)"ingressInterface", 4, 0 },         /* IE 10 */
        { (char *)"sourceIPv6Address", 16, 0 },       /* IE 27 */
        { (char *)"sourceIPv6PrefixLength", 1, 0 },   /* IE 29 */
        FB_IESPEC_NULL
    };
    
    tmpl = fbTemplateAlloc(model);
    
    if (!fbTemplateAppendSpecArray(tmpl, spec, 0, &err)) {
        fprintf(stderr, "Failed to create rule 902 template: %s\n", err->message);
        g_clear_error(&err);
        fbTemplateFreeUnused(tmpl);
        return NULL;
    }
    
    return tmpl;
}

/*
 * Rule 903 and 904 have same structure as 901 and 902
 * (different semantic meaning but identical IPFIX encoding)
 */
fbTemplate_t *sav_create_rule_903_template(fbInfoModel_t *model)
{
    return sav_create_rule_901_template(model);
}

fbTemplate_t *sav_create_rule_904_template(fbInfoModel_t *model)
{
    return sav_create_rule_902_template(model);
}
