/**
 * sav_collector.c
 * 
 * RFC 7011-compliant IPFIX collector for SAV (Source Address Validation)
 * Using libfixbuf with SCTP support for receiving SubTemplateList-based SAV rules
 * 
 * Build:
 *   gcc -o sav_collector sav_collector.c \
 *       $(pkg-config --cflags --libs glib-2.0) \
 *       -I/usr/local/include -L/usr/local/lib -lfixbuf -lsctp -lpthread
 * 
 * Run:
 *   LD_LIBRARY_PATH=/usr/local/lib ./sav_collector --listen=sctp://0.0.0.0:4739
 */

#include <fixbuf/public.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <glib.h>

/* Global state */
static gboolean g_running = TRUE;
static FILE *g_output_file = NULL;
static uint64_t g_records_received = 0;

/* Data structures for SAV rules */
typedef struct {
    uint32_t interface_index;
    uint8_t prefix_v4[4];
    uint8_t prefix_length;
} sav_rule_ipv4_if2prefix_t;

typedef struct {
    uint32_t interface_index;
    uint8_t prefix_v6[16];
    uint8_t prefix_length;
} sav_rule_ipv6_if2prefix_t;

typedef struct {
    uint8_t prefix_v4[4];
    uint8_t prefix_length;
    uint32_t interface_index;
} sav_rule_ipv4_prefix2if_t;

typedef struct {
    uint8_t prefix_v6[16];
    uint8_t prefix_length;
    uint32_t interface_index;
} sav_rule_ipv6_prefix2if_t;

/* SAV Information Element IDs from draft-cao-opsawg-ipfix-sav-01
 * Using placeholder values until IANA assignment */
#define IE_SAV_RULE_TYPE                50000  /* TBD1 - placeholder */
#define IE_SAV_TARGET_TYPE              50001  /* TBD2 - placeholder */
#define IE_SAV_MATCHED_CONTENT_LIST     50002  /* TBD3 - subTemplateList */
#define IE_SAV_POLICY_ACTION            50003  /* TBD4 - placeholder */

/* Standard IPFIX IEs used in SAV sub-templates */
#define IE_INGRESS_INTERFACE            10     /* RFC 5102 */
#define IE_SOURCE_IPV4_PREFIX           44     /* RFC 5102 */
#define IE_SOURCE_IPV4_PREFIX_LENGTH    29     /* RFC 5102 */
#define IE_SOURCE_IPV6_PREFIX           170    /* RFC 5102 */
#define IE_SOURCE_IPV6_PREFIX_LENGTH    29     /* RFC 5102 (same as v4) */

/* SAV Template IDs from draft-cao-opsawg-ipfix-sav-01 Appendix A */
#define TMPL_SAV_IPV4_INTERFACE_TO_PREFIX   901
#define TMPL_SAV_IPV6_INTERFACE_TO_PREFIX   902
#define TMPL_SAV_IPV4_PREFIX_TO_INTERFACE   903
#define TMPL_SAV_IPV6_PREFIX_TO_INTERFACE   904

/* SAV rule types */
#define SAV_RULE_TYPE_ALLOWLIST         0
#define SAV_RULE_TYPE_BLOCKLIST         1

/* SAV target types */
#define SAV_TARGET_TYPE_INTERFACE       0
#define SAV_TARGET_TYPE_PREFIX          1

/* SAV policy actions */
#define SAV_POLICY_ACTION_PERMIT        0
#define SAV_POLICY_ACTION_DISCARD       1
#define SAV_POLICY_ACTION_RATE_LIMIT    2
#define SAV_POLICY_ACTION_REDIRECT      3

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int signum) {
    fprintf(stderr, "\nReceived signal %d, shutting down...\n", signum);
    g_running = FALSE;
}

/**
 * Setup signal handlers
 */
static void setup_signals(void) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
}

/**
 * Print usage information
 */
static void print_usage(const char *progname) {
    fprintf(stderr, "Usage: %s [OPTIONS]\n", progname);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  --listen=CONNSPEC    Listen specification (default: sctp://0.0.0.0:4739)\n");
    fprintf(stderr, "                       Formats: sctp://HOST:PORT, tcp://HOST:PORT, udp://HOST:PORT\n");
    fprintf(stderr, "  --output=FILE        Output file for SAV rules (default: stdout)\n");
    fprintf(stderr, "  --verbose            Enable verbose logging\n");
    fprintf(stderr, "  --help               Show this help message\n");
    fprintf(stderr, "\nExample:\n");
    fprintf(stderr, "  %s --listen=sctp://0.0.0.0:4739 --output=sav_rules.json\n", progname);
}

/**
 * Parse command-line arguments
 */
typedef struct {
    char *listen_spec;
    char *output_file;
    gboolean verbose;
} config_t;

static gboolean parse_args(int argc, char **argv, config_t *config) {
    static config_t default_config = {
        .listen_spec = "sctp://0.0.0.0:4739",
        .output_file = NULL,
        .verbose = FALSE
    };
    
    *config = default_config;
    
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--listen=", 9) == 0) {
            config->listen_spec = argv[i] + 9;
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            config->output_file = argv[i] + 9;
        } else if (strcmp(argv[i], "--verbose") == 0) {
            config->verbose = TRUE;
        } else if (strcmp(argv[i], "--help") == 0) {
            return FALSE;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            return FALSE;
        }
    }
    
    return TRUE;
}

/**
 * Initialize Information Model with SAV IEs
 * Uses standard info model with RFC 5102 IEs for now
 * TODO: Add custom SAV IEs once draft is finalized
 */
static fbInfoModel_t *init_sav_info_model(void) {
    fbInfoModel_t *model = fbInfoModelAlloc();
    
    if (model == NULL) {
        fprintf(stderr, "Failed to allocate information model\n");
        return NULL;
    }
    
    /* For now, using standard IPFIX IEs (ingressInterface, sourceIPv4/v6Prefix, etc.)
     * Custom SAV IEs will be added when draft-cao-opsawg-ipfix-sav is finalized */
    
    fprintf(stderr, "Information model initialized with %u standard IPFIX elements\n",
            fbInfoModelCountElements(model));
    
    return model;
}

/**
 * Listener application context callback
 * Called when a new connection is established
 */
static gboolean listener_app_init(
    fbListener_t *listener,
    void **ctx,
    int fd,
    struct sockaddr *peer,
    size_t peerlen,
    GError **err)
{
    char addrbuf[256];
    
    (void)listener;  /* Unused */
    (void)fd;        /* Unused */
    (void)peerlen;   /* Unused */
    (void)err;       /* Unused */
    
    /* Format peer address */
    if (peer->sa_family == AF_INET) {
        struct sockaddr_in *sin = (struct sockaddr_in *)peer;
        inet_ntop(AF_INET, &sin->sin_addr, addrbuf, sizeof(addrbuf));
        fprintf(stderr, "New connection from %s:%d\n", addrbuf, ntohs(sin->sin_port));
    } else if (peer->sa_family == AF_INET6) {
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)peer;
        inet_ntop(AF_INET6, &sin6->sin6_addr, addrbuf, sizeof(addrbuf));
        fprintf(stderr, "New connection from [%s]:%d\n", addrbuf, ntohs(sin6->sin6_port));
    }
    
    /* Allocate session context (none needed for now) */
    *ctx = NULL;
    
    return TRUE;
}

/**
 * Listener application context free callback
 * Called when a connection is closed
 */
static void listener_app_free(void *ctx) {
    (void)ctx;  /* Unused */
    fprintf(stderr, "Connection closed\n");
}

/**
 * Process received IPFIX messages from buffer
 * Simplified version - just logs received data for now
 * TODO: Implement proper template management and SubTemplateList decoding
 */
static gboolean process_buffer(fBuf_t *fbuf, FILE *output) {
    GError *err = NULL;
    uint8_t buf[65536];
    size_t rec_len;
    size_t bufsize = sizeof(buf);
    gboolean ret = TRUE;
    
    (void)output;  /* Unused for now */
    
    fprintf(stderr, "Processing IPFIX stream...\n");
    
    /* Simple message reading loop
     * In a real implementation, we would:
     * 1. Set internal template matching the exporter's external template
     * 2. Use fBufNext() to read data records
     * 3. Decode SubTemplateLists using fbSubTemplateList* functions
     * 4. Output SAV rules as JSON
     */
    
    while (ret) {
        rec_len = fBufNext(fbuf, buf, &bufsize, &err);
        
        if (rec_len > 0) {
            g_records_received++;
            
            if (g_records_received % 10 == 0) {
                fprintf(stderr, "Received %lu records (%zu bytes each)\n",
                        (unsigned long)g_records_received, rec_len);
            }
            
            /* TODO: Decode the record based on template */
            /* For now, just acknowledge receipt */
            bufsize = sizeof(buf);  /* Reset buffer size for next read */
            
        } else {
            /* No more data or error */
            if (err) {
                if (g_error_matches(err, FB_ERROR_DOMAIN, FB_ERROR_EOF)) {
                    fprintf(stderr, "Connection closed by peer\n");
                } else if (g_error_matches(err, FB_ERROR_DOMAIN, FB_ERROR_EOM)) {
                    /* End of message, continue reading */
                    g_clear_error(&err);
                    bufsize = sizeof(buf);
                    continue;
                } else {
                    fprintf(stderr, "Error reading from buffer: %s\n", err->message);
                    ret = FALSE;
                }
                g_clear_error(&err);
            }
            break;
        }
    }
    
    fprintf(stderr, "Finished processing buffer (%lu records total)\n",
            (unsigned long)g_records_received);
    
    return ret;
}

/**
 * Main collection loop
 */
static int run_collector(config_t *config) {
    GError *err = NULL;
    fbInfoModel_t *model = NULL;
    fbSession_t *session = NULL;
    fbListener_t *listener = NULL;
    fBuf_t *fbuf = NULL;
    fbConnSpec_t connspec;
    int ret = EXIT_FAILURE;
    
    /* Open output file */
    if (config->output_file) {
        g_output_file = fopen(config->output_file, "w");
        if (!g_output_file) {
            fprintf(stderr, "Failed to open output file: %s\n", config->output_file);
            goto cleanup;
        }
        fprintf(g_output_file, "[\n");
    } else {
        g_output_file = stdout;
    }
    
    /* Initialize information model */
    model = init_sav_info_model();
    if (!model) {
        goto cleanup;
    }
    
    /* Create session */
    session = fbSessionAlloc(model);
    if (!session) {
        fprintf(stderr, "Failed to create session\n");
        goto cleanup;
    }
    
    /* Parse connection specification */
    memset(&connspec, 0, sizeof(connspec));
    connspec.transport = FB_SCTP;  /* Default to SCTP */
    connspec.host = "0.0.0.0";
    connspec.svc = "4739";
    
    /* Parse listen spec if provided */
    if (strncmp(config->listen_spec, "sctp://", 7) == 0) {
        char *spec_copy = strdup(config->listen_spec + 7);
        char *colon = strchr(spec_copy, ':');
        if (colon) {
            *colon = '\0';
            connspec.host = strdup(spec_copy);
            connspec.svc = strdup(colon + 1);
        }
        free(spec_copy);
        connspec.transport = FB_SCTP;
    } else if (strncmp(config->listen_spec, "tcp://", 6) == 0) {
        char *spec_copy = strdup(config->listen_spec + 6);
        char *colon = strchr(spec_copy, ':');
        if (colon) {
            *colon = '\0';
            connspec.host = strdup(spec_copy);
            connspec.svc = strdup(colon + 1);
        }
        free(spec_copy);
        connspec.transport = FB_TCP;
    } else if (strncmp(config->listen_spec, "udp://", 6) == 0) {
        char *spec_copy = strdup(config->listen_spec + 6);
        char *colon = strchr(spec_copy, ':');
        if (colon) {
            *colon = '\0';
            connspec.host = strdup(spec_copy);
            connspec.svc = strdup(colon + 1);
        }
        free(spec_copy);
        connspec.transport = FB_UDP;
    }
    
    /* Create listener */
    listener = fbListenerAlloc(&connspec, session, listener_app_init, 
                               listener_app_free, &err);
    if (!listener) {
        fprintf(stderr, "Failed to create listener: %s\n", err->message);
        g_clear_error(&err);
        goto cleanup;
    }
    
    fprintf(stderr, "Listening on %s (transport=%d)\n", config->listen_spec, connspec.transport);
    fprintf(stderr, "Press Ctrl+C to stop\n");
    
    /* Main collection loop */
    while (g_running) {
        /* Wait for incoming connection - returns fBuf_t */
        fbuf = fbListenerWait(listener, &err);
        if (!fbuf) {
            if (err) {
                fprintf(stderr, "Listener error: %s\n", err->message);
                g_clear_error(&err);
            }
            if (!g_running) break;
            continue;
        }
        
        fprintf(stderr, "Connection established, processing messages...\n");
        
        /* Process IPFIX messages from this connection */
        if (!process_buffer(fbuf, g_output_file)) {
            fprintf(stderr, "Error processing buffer\n");
        }
        
        /* Buffer will be freed by listener */
    }
    
    fprintf(stderr, "\nTotal records received: %lu\n", (unsigned long)g_records_received);
    ret = EXIT_SUCCESS;
    
cleanup:
    if (listener) fbListenerFree(listener);
    if (session) fbSessionFree(session);
    if (model) fbInfoModelFree(model);
    
    if (g_output_file && g_output_file != stdout) {
        fprintf(g_output_file, "]\n");
        fclose(g_output_file);
    }
    
    return ret;
}

/**
 * Main entry point
 */
int main(int argc, char **argv) {
    config_t config;
    int ret;
    
    /* Parse arguments */
    if (!parse_args(argc, argv, &config)) {
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }
    
    /* Setup signal handlers */
    setup_signals();
    
    /* Run collector */
    ret = run_collector(&config);
    
    return ret;
}
