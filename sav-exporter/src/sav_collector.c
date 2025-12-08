/*
 * sav_collector.c
 * 
 * SAV IPFIX Collector using libfixbuf
 * 
 * This is a minimal skeleton - will be populated with YAF patterns
 * after studying YAF source code (yaf tools)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fixbuf/public.h>
#include "../include/sav_info_elements.h"
#include "../include/sav_records.h"

/* Global state */
static fbSession_t *session = NULL;
static fBuf_t *fbuf = NULL;
static fbInfoModel_t *model = NULL;
static gboolean keep_running = TRUE;
static FILE *output_file = NULL;

/*
 * Signal handler for clean shutdown
 */
static void signal_handler(int signum)
{
    printf("\n[sav-collector] Received signal %d, shutting down...\n", signum);
    keep_running = FALSE;
}

/*
 * Initialize libfixbuf collector session
 * 
 * TODO: Extract patterns from YAF's collector tools
 */
static gboolean init_collector_session(const char *listen_spec, GError **err)
{
    (void)listen_spec;  /* Unused for skeleton */
    (void)err;
    
    /* Get InfoModel and register SAV IEs */
    model = fbInfoModelAlloc();
    if (!sav_register_info_elements(model)) {
        fprintf(stderr, "Failed to register SAV Information Elements\n");
        return FALSE;
    }
    
    /* Create session */
    session = fbSessionAlloc(model);
    
    /* TODO: Register internal templates for reading */
    /* Will implement actual network listener after YAF study */
    
    printf("[sav-collector] Initialized session (skeleton)\n");
    printf("[sav-collector] (Network listener not yet implemented)\n");
    return TRUE;
}

/*
 * Process one SAV record from IPFIX stream
 * 
 * TODO: Extract SubTemplateList iteration patterns from YAF
 */
static gboolean process_sav_record(sav_main_record_t *record)
{
    void *data = NULL;
    int count = 0;
    
    printf("[sav-collector] Received record: rule_type=%d, match_count=%d\n",
           record->rule_type, record->match_count);
    
    /* TODO: Iterate SubTemplateList using fbSubTemplateListGetNextPtr() */
    /* Example from YAF:
     * while ((data = fbSubTemplateListGetNextPtr(&record->content_list, data)) != NULL) {
     *     if (record->rule_type == SAV_RULE_TYPE_901) {
     *         sav_rule_901_t *rule = (sav_rule_901_t *)data;
     *         // Process rule...
     *     }
     * }
     */
    
    /* Output to JSON (simplified) */
    if (output_file) {
        fprintf(output_file, "{\"rule_type\": %d, \"match_count\": %d}\n",
                record->rule_type, record->match_count);
        fflush(output_file);
    }
    
    return TRUE;
}

/*
 * Main collection loop
 */
static gboolean run_collector_loop(GError **err)
{
    sav_main_record_t record;
    size_t rec_size;
    
    printf("[sav-collector] Starting collection loop\n");
    
    /* TODO: Set internal template for reading */
    /* fBufSetInternalTemplate(fbuf, TMPL_SAV_MAIN, err); */
    
    while (keep_running) {
        /* Initialize record for reading */
        memset(&record, 0, sizeof(record));
        fbSubTemplateListCollectorInit(&record.content_list);
        
        /* Read next record (TODO: Implement with proper error handling) */
        /* if (!fBufNext(fbuf, (uint8_t *)&record, &rec_size, err)) {
         *     // Handle EOF or error
         *     break;
         * }
         */
        
        /* Process record */
        /* process_sav_record(&record); */
        
        /* Clear SubTemplateList */
        /* fbSubTemplateListClear(&record.content_list); */
        
        /* For skeleton, just sleep */
        printf("[sav-collector] (Skeleton mode - no actual reading yet)\n");
        sleep(2);
    }
    
    return TRUE;
}

/*
 * Cleanup and close collector
 */
static void cleanup_collector(void)
{
    if (fbuf) {
        fBufFree(fbuf);
        fbuf = NULL;
    }
    if (session) {
        fbSessionFree(session);
        session = NULL;
    }
    if (output_file && output_file != stdout) {
        fclose(output_file);
        output_file = NULL;
    }
    printf("[sav-collector] Cleaned up\n");
}

/*
 * Main entry point
 */
int main(int argc, char **argv)
{
    GError *err = NULL;
    const char *listen_spec = "tcp://127.0.0.1:4739";
    const char *output_path = NULL;
    
    /* Setup signal handlers */
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    /* Parse command line arguments */
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--listen=", 9) == 0) {
            listen_spec = argv[i] + 9;
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            output_path = argv[i] + 9;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --listen=<spec>   Listen spec (default: tcp://127.0.0.1:4739)\n");
            printf("  --output=<file>   Output file (default: stdout)\n");
            printf("  --help            Show this help\n");
            return 0;
        }
    }
    
    printf("=== SAV IPFIX Collector (Skeleton) ===\n");
    printf("Listen: %s\n", listen_spec);
    
    /* Open output file */
    if (output_path) {
        output_file = fopen(output_path, "w");
        if (!output_file) {
            fprintf(stderr, "Failed to open output file: %s\n", output_path);
            return 1;
        }
        printf("Output: %s\n", output_path);
    } else {
        output_file = stdout;
        printf("Output: stdout\n");
    }
    
    /* Initialize collector */
    if (!init_collector_session(listen_spec, &err)) {
        fprintf(stderr, "Initialization failed: %s\n", err->message);
        g_clear_error(&err);
        return 1;
    }
    
    /* Run collection loop */
    if (!run_collector_loop(&err)) {
        fprintf(stderr, "Collection failed: %s\n", err->message);
        g_clear_error(&err);
        cleanup_collector();
        return 1;
    }
    
    /* Cleanup */
    cleanup_collector();
    
    return 0;
}
