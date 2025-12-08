/*
 * sav_exporter.c
 * 
 * SAV IPFIX Exporter using libfixbuf
 * 
 * This is a minimal skeleton - will be populated with YAF patterns
 * after studying YAF source code (yafcore.c)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fixbuf/public.h>
#include "../include/sav_info_elements.h"
#include "../include/sav_records.h"

/* Global session and buffer */
static fbSession_t *session = NULL;
static fBuf_t *fbuf = NULL;
static fbInfoModel_t *model = NULL;

/*
 * Initialize libfixbuf session and templates
 * 
 * TODO: Extract patterns from YAF's yafcore.c:yfInitExporterSession()
 */
static gboolean init_exporter_session(const char *connect_spec, GError **err)
{
    (void)connect_spec;  /* Unused for skeleton */
    (void)err;
    
    /* Get InfoModel and register SAV IEs */
    model = fbInfoModelAlloc();
    if (!sav_register_info_elements(model)) {
        fprintf(stderr, "Failed to register SAV Information Elements\n");
        return FALSE;
    }
    
    /* Create session */
    session = fbSessionAlloc(model);
    
    /* TODO: Create and register templates using YAF patterns */
    /* Will implement actual network connection after YAF study */
    
    printf("[sav-exporter] Initialized session (skeleton)\n");
    return TRUE;
}

/*
 * Export a single SAV record with SubTemplateList
 * 
 * TODO: Extract patterns from YAF's payload scanner or DPI code
 */
static gboolean export_sav_record(sav_main_record_t *record, GError **err)
{
    /* TODO: 
     * 1. Set internal template with fBufSetInternalTemplate()
     * 2. Initialize SubTemplateList with fbSubTemplateListInit()
     * 3. Populate SubTemplateList elements
     * 4. Write to buffer with fBufAppend()
     * 5. Clear SubTemplateList with fbSubTemplateListClear()
     */
    
    printf("[sav-exporter] Export record (skeleton): rule_type=%d, match_count=%d\n",
           record->rule_type, record->match_count);
    
    return TRUE;
}

/*
 * Cleanup and close exporter
 */
static void cleanup_exporter(void)
{
    if (fbuf) {
        fBufFree(fbuf);
        fbuf = NULL;
    }
    if (session) {
        fbSessionFree(session);
        session = NULL;
    }
    if (model) {
        /* InfoModel is freed with session */
        model = NULL;
    }
    printf("[sav-exporter] Cleaned up\n");
}

/*
 * Test mode: Generate dummy SAV records
 */
static void run_test_mode(void)
{
    sav_main_record_t record;
    
    printf("[sav-exporter] Running in test mode\n");
    
    /* Test record 1: IPv4 rule with 3 matches */
    sav_main_record_init(&record, SAV_RULE_TYPE_901, SAV_TARGET_TYPE_IPV4);
    record.match_count = 3;
    
    /* TODO: Populate SubTemplateList with actual rule_901 entries */
    
    GError *err = NULL;
    if (!export_sav_record(&record, &err)) {
        fprintf(stderr, "Export failed: %s\n", err->message);
        g_clear_error(&err);
    }
    
    sav_main_record_clear(&record);
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    GError *err = NULL;
    const char *connect_spec = "tcp://127.0.0.1:4739";
    gboolean test_mode = FALSE;
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--connect=", 10) == 0) {
            connect_spec = argv[i] + 10;
        } else if (strcmp(argv[i], "--test-mode") == 0) {
            test_mode = TRUE;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --connect=<spec>  Connection spec (default: tcp://127.0.0.1:4739)\n");
            printf("  --test-mode       Generate dummy test records\n");
            printf("  --help            Show this help\n");
            return 0;
        }
    }
    
    printf("=== SAV IPFIX Exporter (Skeleton) ===\n");
    printf("Connect: %s\n", connect_spec);
    
    /* Initialize exporter */
    if (!init_exporter_session(connect_spec, &err)) {
        fprintf(stderr, "Initialization failed: %s\n", err->message);
        g_clear_error(&err);
        return 1;
    }
    
    /* Run test mode or wait for real data */
    if (test_mode) {
        run_test_mode();
    } else {
        printf("[sav-exporter] Waiting for SAV data input...\n");
        printf("[sav-exporter] (Not implemented yet - need YAF patterns)\n");
        sleep(5);
    }
    
    /* Cleanup */
    cleanup_exporter();
    
    return 0;
}
