/**
 * SAV IPFIX Collector - libfixbuf Native API Implementation
 * 
 * Uses fbSessionAddTemplate() and fbRecordCopyToTemplate() for automatic
 * SubTemplateList decoding per RFC 6313.
 *
 * Author: SAV IPFIX Implementation Team
 * Date: 2025-12-08
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fixbuf/public.h>

/* Global flag for signal handling */
static volatile sig_atomic_t g_running = TRUE;
static uint64_t g_records_received = 0;
static FILE *g_output_file = NULL;

/* Template IDs */
#define TMPL_SAV_MAIN           256
#define TMPL_SAV_RULE_901       901
#define TMPL_SAV_RULE_902       902
#define TMPL_SAV_RULE_903       903
#define TMPL_SAV_RULE_904       904

/* Information Element IDs */
#define IE_SAV_RULE_TYPE        50000
#define IE_SAV_TARGET_TYPE      50001
#define IE_SAV_CONTENT_LIST     50002
#define IE_SAV_POLICY_ACTION    50003

/* Standard IPFIX IEs */
#define IE_INGRESS_INTERFACE    10
#define IE_SOURCE_IPV4_ADDRESS  8
#define IE_SOURCE_IPV6_ADDRESS  27

/* Configuration */
typedef struct {
    const char *listen_spec;
    const char *output_file;
    gboolean    verbose;
} config_t;

/* SAV Rule Structures */

/* Template 901: IPv4 Interface-to-Prefix (9 bytes) */
typedef struct {
    uint32_t interface_index;
    uint32_t ipv4_address;
    uint8_t  prefix_length;
} __attribute__((packed)) sav_rule_901_t;

/* Template 902: IPv6 Interface-to-Prefix (21 bytes) */
typedef struct {
    uint32_t interface_index;
    uint8_t  ipv6_address[16];
    uint8_t  prefix_length;
} __attribute__((packed)) sav_rule_902_t;

/* Template 903: IPv4 Prefix-to-Interface (9 bytes) */
typedef struct {
    uint32_t ipv4_address;
    uint8_t  prefix_length;
    uint32_t interface_index;
} __attribute__((packed)) sav_rule_903_t;

/* Template 904: IPv6 Prefix-to-Interface (21 bytes) */
typedef struct {
    uint8_t  ipv6_address[16];
    uint8_t  prefix_length;
    uint32_t interface_index;
} __attribute__((packed)) sav_rule_904_t;

/* Main SAV record with SubTemplateList */
typedef struct {
    uint8_t             rule_type;
    uint8_t             target_type;
    fbSubTemplateList_t content_list;
    uint8_t             policy_action;
} sav_main_record_t;

/**
 * Signal handler
 */
static void signal_handler(int signum) {
    (void)signum;
    fprintf(stderr, "\nShutting down...\n");
    g_running = FALSE;
}

/**
 * Register SAV Information Elements to model
 */
static void register_sav_ies(fbInfoModel_t *model) {
    /* Define SAV IEs using FB_IE_INIT_FULL macro */
    fbInfoElement_t sav_ies[] = {
        FB_IE_INIT_FULL("savRuleType", 0, IE_SAV_RULE_TYPE, 1, 0,
                        0, 0, FB_UINT_8, "SAV rule type (allowlist/blocklist)"),
        FB_IE_INIT_FULL("savTargetType", 0, IE_SAV_TARGET_TYPE, 1, 0,
                        0, 0, FB_UINT_8, "SAV target type (interface/prefix)"),
        FB_IE_INIT_FULL("savMatchedContentList", 0, IE_SAV_CONTENT_LIST, FB_IE_VARLEN, 0,
                        0, 0, FB_SUB_TMPL_LIST, "SAV matched content list (SubTemplateList)"),
        FB_IE_INIT_FULL("savPolicyAction", 0, IE_SAV_POLICY_ACTION, 1, 0,
                        0, 0, FB_UINT_8, "SAV policy action (permit/discard/etc)"),
        FB_IE_NULL
    };
    
    fbInfoModelAddElementArray(model, sav_ies);
    fprintf(stderr, "Registered 4 SAV IEs\n");
}

/**
 * Create internal template for Template 901
 */
static fbTemplate_t *create_template_901(fbInfoModel_t *model, GError **err) {
    const fbInfoElement_t *ie = NULL;
    fbTemplate_t *tmpl = fbTemplateAlloc(model);
    
    /* ingressInterface (4 bytes) */
    ie = fbInfoModelGetElementByName(model, "ingressInterface");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    /* sourceIPv4Address (4 bytes) */
    ie = fbInfoModelGetElementByName(model, "sourceIPv4Address");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    /* sourceIPv4PrefixLength (1 byte) */
    ie = fbInfoModelGetElementByName(model, "sourceIPv4PrefixLength");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    return tmpl;
}

/**
 * Create internal template for Template 902
 */
static fbTemplate_t *create_template_902(fbInfoModel_t *model, GError **err) {
    const fbInfoElement_t *ie = NULL;
    fbTemplate_t *tmpl = fbTemplateAlloc(model);
    
    /* ingressInterface (4 bytes) */
    ie = fbInfoModelGetElementByName(model, "ingressInterface");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    /* sourceIPv6Address (16 bytes) */
    ie = fbInfoModelGetElementByName(model, "sourceIPv6Address");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    /* sourceIPv6PrefixLength (1 byte) */
    ie = fbInfoModelGetElementByName(model, "sourceIPv6PrefixLength");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    return tmpl;
}

/**
 * Create internal template for main SAV record
 */
static fbTemplate_t *create_template_main(fbInfoModel_t *model, GError **err) {
    const fbInfoElement_t *ie = NULL;
    fbTemplate_t *tmpl = fbTemplateAlloc(model);
    
    /* savRuleType */
    ie = fbInfoModelGetElementByName(model, "savRuleType");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    /* savTargetType */
    ie = fbInfoModelGetElementByName(model, "savTargetType");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    /* savMatchedContentList (SubTemplateList) */
    ie = fbInfoModelGetElementByName(model, "savMatchedContentList");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    /* savPolicyAction */
    ie = fbInfoModelGetElementByName(model, "savPolicyAction");
    if (!ie || !fbTemplateAppend(tmpl, ie, err)) return NULL;
    
    return tmpl;
}

/**
 * Output SAV rule 901 as JSON
 */
static void output_rule_901(sav_rule_901_t *rule, gboolean *first) {
    char ip_str[INET_ADDRSTRLEN];
    
    if (!(*first)) fprintf(g_output_file, ",\n");
    *first = FALSE;
    
    inet_ntop(AF_INET, &rule->ipv4_address, ip_str, sizeof(ip_str));
    
    fprintf(g_output_file, "  {\n");
    fprintf(g_output_file, "    \"template_id\": 901,\n");
    fprintf(g_output_file, "    \"type\": \"ipv4_interface_to_prefix\",\n");
    fprintf(g_output_file, "    \"interface\": %u,\n", rule->interface_index);
    fprintf(g_output_file, "    \"prefix\": \"%s/%u\"\n", ip_str, rule->prefix_length);
    fprintf(g_output_file, "  }");
    
    g_records_received++;
}

/**
 * Output SAV rule 902 as JSON
 */
static void output_rule_902(sav_rule_902_t *rule, gboolean *first) {
    char ip_str[INET6_ADDRSTRLEN];
    
    if (!(*first)) fprintf(g_output_file, ",\n");
    *first = FALSE;
    
    inet_ntop(AF_INET6, rule->ipv6_address, ip_str, sizeof(ip_str));
    
    fprintf(g_output_file, "  {\n");
    fprintf(g_output_file, "    \"template_id\": 902,\n");
    fprintf(g_output_file, "    \"type\": \"ipv6_interface_to_prefix\",\n");
    fprintf(g_output_file, "    \"interface\": %u,\n", rule->interface_index);
    fprintf(g_output_file, "    \"prefix\": \"%s/%u\"\n", ip_str, rule->prefix_length);
    fprintf(g_output_file, "  }");
    
    g_records_received++;
}

/**
 * Process SubTemplateList from main record
 */
static void process_subtmpl_list(fbSubTemplateList_t *stl, gboolean *first_output) {
    uint16_t tmpl_id = fbSubTemplateListGetTemplateID(stl);
    void *record = NULL;
    
    fprintf(stderr, "Processing SubTemplateList: template ID %u\n", tmpl_id);
    
    /* Iterate through all records in the SubTemplateList */
    while ((record = fbSubTemplateListGetNextPtr(stl, record)) != NULL) {
        switch (tmpl_id) {
            case TMPL_SAV_RULE_901:
                output_rule_901((sav_rule_901_t *)record, first_output);
                break;
            case TMPL_SAV_RULE_902:
                output_rule_902((sav_rule_902_t *)record, first_output);
                break;
            case TMPL_SAV_RULE_903:
            case TMPL_SAV_RULE_904:
                fprintf(stderr, "  Template %u not yet implemented\n", tmpl_id);
                break;
            default:
                fprintf(stderr, "  Unknown template ID: %u\n", tmpl_id);
        }
    }
}

/**
 * Process one IPFIX buffer
 */
static gboolean process_buffer(fBuf_t *fbuf) {
    GError *err = NULL;
    sav_main_record_t record;
    gboolean first_output = TRUE;
    
    fprintf(stderr, "Processing IPFIX messages...\n");
    fprintf(g_output_file, "[\n");
    
    /* Read records until EOF */
    while (g_running) {
        size_t rec_size = sizeof(record);
        
        /* Clear and initialize the record (CRITICAL for SubTemplateList!) */
        memset(&record, 0, sizeof(record));
        
        /* Initialize SubTemplateList for reading */
        fbSubTemplateListCollectorInit(&record.content_list);
        
        /* Read next record using libfixbuf API */
        if (!fBufNext(fbuf, (uint8_t *)&record, &rec_size, &err)) {
            if (g_error_matches(err, FB_ERROR_DOMAIN, FB_ERROR_EOF)) {
                fprintf(stderr, "DEBUG: Got EOF from fBufNext()\n");
                g_clear_error(&err);
                break;
            } else if (g_error_matches(err, FB_ERROR_DOMAIN, FB_ERROR_EOM)) {
                /* End of message, continue */
                fprintf(stderr, "DEBUG: Got EOM from fBufNext()\n");
                g_clear_error(&err);
                continue;
            } else {
                fprintf(stderr, "Error reading record: %s (code: %d, domain: %s)\n", 
                        err ? err->message : "unknown",
                        err ? err->code : 0,
                        err && err->domain ? g_quark_to_string(err->domain) : "unknown");
                g_clear_error(&err);
                break;
            }
        }
        
        fprintf(stderr, "DEBUG: Successfully read record, size=%zu bytes\n", rec_size);
        
        fprintf(stderr, "Received SAV record: type=%u, target=%u, action=%u\n",
                record.rule_type, record.target_type, record.policy_action);
        
        /* Process SubTemplateList */
        if (fbSubTemplateListCountElements(&record.content_list) > 0) {
            process_subtmpl_list(&record.content_list, &first_output);
        }
        
        /* Clear the SubTemplateList */
        fbSubTemplateListClear(&record.content_list);
    }
    
    fprintf(g_output_file, "\n]\n");
    fprintf(stderr, "Total SAV rules decoded: %lu\n", (unsigned long)g_records_received);
    
    return TRUE;
}

/**
 * Listener callback for new connections
 */
static gboolean listener_callback(
    fbListener_t *listener,
    void **ctx,
    int fd,
    struct sockaddr *peer,
    size_t peerlen,
    GError **err)
{
    (void)listener; (void)ctx; (void)fd; (void)peerlen; (void)err;
    
    char addr_str[256];
    if (peer->sa_family == AF_INET) {
        struct sockaddr_in *sa = (struct sockaddr_in *)peer;
        inet_ntop(AF_INET, &sa->sin_addr, addr_str, sizeof(addr_str));
        fprintf(stderr, "New connection from %s:%u\n", addr_str, ntohs(sa->sin_port));
    }
    
    return TRUE;
}

/**
 * Main collector loop
 */
static int run_collector(config_t *config) {
    GError *err = NULL;
    fbInfoModel_t *model = NULL;
    fbSession_t *session = NULL;
    fbListener_t *listener = NULL;
    fBuf_t *fbuf = NULL;
    fbConnSpec_t connspec;
    fbTemplate_t *tmpl_main = NULL;
    fbTemplate_t *tmpl_901 = NULL;
    fbTemplate_t *tmpl_902 = NULL;
    int ret = EXIT_FAILURE;
    
    /* Initialize info model */
    model = fbInfoModelAlloc();
    register_sav_ies(model);
    
    /* Create session */
    session = fbSessionAlloc(model);
    
    /* Create and add internal templates */
    tmpl_main = create_template_main(model, &err);
    if (!tmpl_main) {
        fprintf(stderr, "Failed to create main template: %s\n", err->message);
        goto cleanup;
    }
    if (!fbSessionAddTemplate(session, TRUE, TMPL_SAV_MAIN, tmpl_main, &err)) {
        fprintf(stderr, "Failed to add main template: %s\n", err->message);
        goto cleanup;
    }
    
    tmpl_901 = create_template_901(model, &err);
    if (!tmpl_901) {
        fprintf(stderr, "Failed to create template 901: %s\n", err->message);
        goto cleanup;
    }
    if (!fbSessionAddTemplate(session, TRUE, TMPL_SAV_RULE_901, tmpl_901, &err)) {
        fprintf(stderr, "Failed to add template 901: %s\n", err->message);
        goto cleanup;
    }
    
    tmpl_902 = create_template_902(model, &err);
    if (!tmpl_902) {
        fprintf(stderr, "Failed to create template 902: %s\n", err->message);
        goto cleanup;
    }
    if (!fbSessionAddTemplate(session, TRUE, TMPL_SAV_RULE_902, tmpl_902, &err)) {
        fprintf(stderr, "Failed to add template 902: %s\n", err->message);
        goto cleanup;
    }
    
    fprintf(stderr, "Registered templates: 256 (main), 901, 902\n");
    
    /* Parse connection spec */
    memset(&connspec, 0, sizeof(connspec));
    
    if (strncmp(config->listen_spec, "tcp://", 6) == 0) {
        char *spec_copy = strdup(config->listen_spec + 6);
        char *colon = strrchr(spec_copy, ':');
        if (colon) {
            *colon = '\0';
            connspec.host = strdup(spec_copy);
            connspec.svc = strdup(colon + 1);
        }
        free(spec_copy);
        connspec.transport = FB_TCP;
    } else if (strncmp(config->listen_spec, "sctp://", 7) == 0) {
        char *spec_copy = strdup(config->listen_spec + 7);
        char *colon = strrchr(spec_copy, ':');
        if (colon) {
            *colon = '\0';
            connspec.host = strdup(spec_copy);
            connspec.svc = strdup(colon + 1);
        }
        free(spec_copy);
        connspec.transport = FB_SCTP;
    } else {
        fprintf(stderr, "Unsupported transport in: %s\n", config->listen_spec);
        goto cleanup;
    }
    
    /* Create listener */
    listener = fbListenerAlloc(&connspec, session, listener_callback, NULL, &err);
    if (!listener) {
        fprintf(stderr, "Failed to create listener: %s\n", err->message);
        goto cleanup;
    }
    
    fprintf(stderr, "Listening on %s (transport=%d)\n", config->listen_spec, connspec.transport);
    
    /* Open output file */
    if (config->output_file) {
        g_output_file = fopen(config->output_file, "w");
        if (!g_output_file) {
            fprintf(stderr, "Failed to open output file: %s\n", config->output_file);
            goto cleanup;
        }
    } else {
        g_output_file = stdout;
    }
    
    /* Main loop */
    while (g_running) {
        fbuf = fbListenerWait(listener, &err);
        if (!fbuf) {
            if (err) {
                fprintf(stderr, "Listener error: %s\n", err->message);
                g_clear_error(&err);
            }
            if (!g_running) break;
            continue;
        }
        
        fprintf(stderr, "Connection established\n");
        
        /* Set internal template for reading */
        if (!fBufSetInternalTemplate(fbuf, TMPL_SAV_MAIN, &err)) {
            fprintf(stderr, "Failed to set internal template: %s\n", err->message);
            g_clear_error(&err);
            continue;
        }
        
        /* Process the buffer */
        process_buffer(fbuf);
        
        /* Buffer freed by listener */
    }
    
    ret = EXIT_SUCCESS;
    
cleanup:
    if (listener) fbListenerFree(listener);
    if (session) fbSessionFree(session);
    if (model) fbInfoModelFree(model);
    if (g_output_file && g_output_file != stdout) fclose(g_output_file);
    
    return ret;
}

/**
 * Parse command line arguments
 */
static gboolean parse_args(int argc, char **argv, config_t *config) {
    config->listen_spec = "tcp://127.0.0.1:4739";
    config->output_file = NULL;
    config->verbose = FALSE;
    
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--listen=", 9) == 0) {
            config->listen_spec = argv[i] + 9;
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            config->output_file = argv[i] + 9;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = TRUE;
        } else if (strcmp(argv[i], "--help") == 0) {
            return FALSE;
        }
    }
    
    return TRUE;
}

/**
 * Main entry point
 */
int main(int argc, char **argv) {
    config_t config;
    
    if (!parse_args(argc, argv, &config)) {
        fprintf(stderr, "Usage: %s [--listen=tcp://HOST:PORT] [--output=FILE]\n", argv[0]);
        return EXIT_FAILURE;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    return run_collector(&config);
}
