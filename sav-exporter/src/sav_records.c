/*
 * sav_records.c
 * 
 * Helper functions for SAV record manipulation
 */

#include <string.h>
#include "../include/sav_records.h"

/*
 * Initialize a main record for export
 */
void sav_main_record_init(sav_main_record_t *record, 
                          uint16_t rule_type, 
                          uint8_t target_type)
{
    if (!record) return;
    
    memset(record, 0, sizeof(sav_main_record_t));
    
    record->rule_type = rule_type;
    record->target_type = target_type;
    record->match_count = 0;
    
    /* SubTemplateList will be initialized by fbSubTemplateListInit() */
}

/*
 * Clear a main record after export
 */
void sav_main_record_clear(sav_main_record_t *record)
{
    if (!record) return;
    
    /* Free SubTemplateList memory */
    fbSubTemplateListClear(&record->content_list);
    
    /* Zero out the record */
    memset(record, 0, sizeof(sav_main_record_t));
}
