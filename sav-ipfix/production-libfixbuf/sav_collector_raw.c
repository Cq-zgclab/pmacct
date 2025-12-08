/**
 * SAV IPFIX Collector - Raw Socket Implementation
 * 
 * Bypasses libfixbuf complexity, directly parses IPFIX using raw TCP socket.
 * Based on working hackathon Python implementation.
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
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

/* Global state */
static volatile sig_atomic_t g_running = 1;
static uint64_t g_records_decoded = 0;
static FILE *g_output = NULL;

/* IPFIX constants */
#define IPFIX_VERSION           10
#define IPFIX_SET_ID_TEMPLATE   2

/* Template IDs */
#define TMPL_SAV_MAIN   256
#define TMPL_SAV_901    901
#define TMPL_SAV_902    902

/* Maximum sizes */
#define MAX_MSG_SIZE    65536
#define MAX_TEMPLATES   256

/* Template storage */
typedef struct {
    uint16_t template_id;
    uint16_t field_count;
    uint8_t  record_size;  /* For fixed-size templates */
    int      has_varlen;   /* Has variable-length fields */
} template_info_t;

static template_info_t g_templates[MAX_TEMPLATES];
static int g_template_count = 0;

/**
 * Signal handler
 */
static void signal_handler(int signum) {
    (void)signum;
    g_running = 0;
}

/**
 * Read exactly n bytes from socket
 */
static int read_exact(int sock, uint8_t *buf, size_t len) {
    size_t total = 0;
    while (total < len && g_running) {
        ssize_t n = recv(sock, buf + total, len - total, 0);
        if (n <= 0) {
            if (n == 0) {
                fprintf(stderr, "Connection closed by peer\n");
            } else {
                fprintf(stderr, "recv error: %s\n", strerror(errno));
            }
            return -1;
        }
        total += n;
    }
    return total == len ? 0 : -1;
}

/**
 * Parse IPFIX message header
 */
static int parse_ipfix_header(uint8_t *buf, uint16_t *msg_len) {
    uint16_t version = ntohs(*(uint16_t *)(buf + 0));
    *msg_len = ntohs(*(uint16_t *)(buf + 2));
    
    if (version != IPFIX_VERSION) {
        fprintf(stderr, "Invalid IPFIX version: %u\n", version);
        return -1;
    }
    
    fprintf(stderr, "IPFIX Message: version=%u, length=%u\n", version, *msg_len);
    return 0;
}

/**
 * Store template info (simplified - just track template ID)
 */
static void store_template(uint16_t tid, uint16_t field_count) {
    if (g_template_count < MAX_TEMPLATES) {
        g_templates[g_template_count].template_id = tid;
        g_templates[g_template_count].field_count = field_count;
        g_template_count++;
        fprintf(stderr, "Stored template %u with %u fields\n", tid, field_count);
    }
}

/**
 * Parse Template Set
 */
static int parse_template_set(uint8_t *data, uint16_t set_len) {
    size_t pos = 4;  /* Skip set header */
    
    while (pos + 4 <= set_len) {
        uint16_t tmpl_id = ntohs(*(uint16_t *)(data + pos));
        uint16_t field_count = ntohs(*(uint16_t *)(data + pos + 2));
        pos += 4;
        
        fprintf(stderr, "Template %u: %u fields\n", tmpl_id, field_count);
        
        /* Skip field specifiers */
        for (int i = 0; i < field_count && pos < set_len; i++) {
            uint16_t ie_num = ntohs(*(uint16_t *)(data + pos));
            uint16_t ie_len = ntohs(*(uint16_t *)(data + pos + 2));
            pos += 4;
            
            /* Check for enterprise bit */
            if (ie_num & 0x8000) {
                pos += 4;  /* Skip enterprise ID */
            }
            
            fprintf(stderr, "  Field %d: IE %u, len %u\n", i, ie_num & 0x7FFF, ie_len);
        }
        
        store_template(tmpl_id, field_count);
    }
    
    return 0;
}

/**
 * Output SAV rule 901 (IPv4 Interface-to-Prefix)
 */
static void output_rule_901(uint8_t *data, int *first) {
    uint32_t iface = ntohl(*(uint32_t *)(data + 0));
    uint32_t ipv4 = ntohl(*(uint32_t *)(data + 4));
    uint8_t  plen = data[8];
    
    char ip_str[INET_ADDRSTRLEN];
    struct in_addr addr = { .s_addr = htonl(ipv4) };
    inet_ntop(AF_INET, &addr, ip_str, sizeof(ip_str));
    
    if (!(*first)) fprintf(g_output, ",\n");
    *first = 0;
    
    fprintf(g_output, "  {\n");
    fprintf(g_output, "    \"template_id\": 901,\n");
    fprintf(g_output, "    \"type\": \"ipv4_interface_to_prefix\",\n");
    fprintf(g_output, "    \"interface\": %u,\n", iface);
    fprintf(g_output, "    \"prefix\": \"%s/%u\"\n", ip_str, plen);
    fprintf(g_output, "  }");
    
    g_records_decoded++;
}

/**
 * Parse SubTemplateList manually (RFC 6313)
 */
static int parse_subtemplatelist(uint8_t *stl_data, uint16_t stl_len, int *first) {
    if (stl_len < 3) {
        fprintf(stderr, "STL too short: %u bytes\n", stl_len);
        return -1;
    }
    
    uint8_t  semantic = stl_data[0];
    uint16_t tmpl_id = ntohs(*(uint16_t *)(stl_data + 1));
    
    fprintf(stderr, "SubTemplateList: semantic=0x%02x, template=%u\n", semantic, tmpl_id);
    
    /* Calculate record size based on template */
    int rec_size = 0;
    if (tmpl_id == TMPL_SAV_901) {
        rec_size = 9;  /* 4(iface) + 4(ipv4) + 1(plen) */
    } else if (tmpl_id == TMPL_SAV_902) {
        rec_size = 21; /* 4(iface) + 16(ipv6) + 1(plen) */
    } else {
        fprintf(stderr, "Unknown template %u in STL\n", tmpl_id);
        return -1;
    }
    
    /* Parse records */
    size_t pos = 3;
    int count = 0;
    
    while (pos + rec_size <= stl_len) {
        if (tmpl_id == TMPL_SAV_901) {
            output_rule_901(stl_data + pos, first);
        }
        /* TODO: Add 902, 903, 904 */
        
        pos += rec_size;
        count++;
    }
    
    fprintf(stderr, "Decoded %d records from STL\n", count);
    return count;
}

/**
 * Parse Data Set (Template 256 - Main SAV record)
 */
static int parse_data_set_256(uint8_t *data, uint16_t set_len, int *first) {
    size_t pos = 4;  /* Skip set header */
    
    while (pos + 4 <= set_len) {
        uint8_t rule_type = data[pos];
        uint8_t target_type = data[pos + 1];
        uint16_t stl_len = ntohs(*(uint16_t *)(data + pos + 2));
        
        fprintf(stderr, "SAV Record: rule_type=%u, target_type=%u, STL_len=%u\n",
                rule_type, target_type, stl_len);
        
        if (pos + 4 + stl_len + 1 > set_len) {
            fprintf(stderr, "Truncated data record\n");
            break;
        }
        
        /* Parse SubTemplateList */
        if (stl_len > 0) {
            parse_subtemplatelist(data + pos + 4, stl_len, first);
        }
        
        /* Skip policy action (1 byte) */
        pos += 4 + stl_len + 1;
    }
    
    return 0;
}

/**
 * Parse one IPFIX Set
 */
static int parse_set(uint8_t *data, size_t data_len, int *first) {
    if (data_len < 4) return -1;
    
    uint16_t set_id = ntohs(*(uint16_t *)(data + 0));
    uint16_t set_len = ntohs(*(uint16_t *)(data + 2));
    
    fprintf(stderr, "Set: ID=%u, Length=%u\n", set_id, set_len);
    
    if (set_len < 4 || set_len > data_len) {
        fprintf(stderr, "Invalid set length\n");
        return -1;
    }
    
    if (set_id == IPFIX_SET_ID_TEMPLATE) {
        parse_template_set(data, set_len);
    } else if (set_id == TMPL_SAV_MAIN) {
        parse_data_set_256(data, set_len, first);
    } else if (set_id >= 256) {
        fprintf(stderr, "Unknown data set ID: %u\n", set_id);
    }
    
    return set_len;
}

/**
 * Process one IPFIX message
 */
static int process_message(uint8_t *msg, int *first) {
    uint16_t msg_len;
    
    if (parse_ipfix_header(msg, &msg_len) < 0) {
        return -1;
    }
    
    /* Parse sets */
    size_t pos = 16;  /* Skip message header */
    while (pos + 4 <= msg_len) {
        int set_len = parse_set(msg + pos, msg_len - pos, first);
        if (set_len < 0) break;
        pos += set_len;
    }
    
    return 0;
}

/**
 * Handle one client connection
 */
static void handle_client(int client_sock, struct sockaddr_in *client_addr) {
    char addr_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &client_addr->sin_addr, addr_str, sizeof(addr_str));
    fprintf(stderr, "Connection from %s:%u\n", addr_str, ntohs(client_addr->sin_port));
    
    uint8_t msg_buf[MAX_MSG_SIZE];
    int first_output = 1;
    
    fprintf(g_output, "[\n");
    
    /* Read IPFIX messages */
    while (g_running) {
        /* Read message header (16 bytes) */
        if (read_exact(client_sock, msg_buf, 16) < 0) {
            break;
        }
        
        /* Get message length */
        uint16_t msg_len = ntohs(*(uint16_t *)(msg_buf + 2));
        
        if (msg_len < 16 || msg_len > MAX_MSG_SIZE) {
            fprintf(stderr, "Invalid message length: %u\n", msg_len);
            break;
        }
        
        /* Read rest of message */
        if (msg_len > 16) {
            if (read_exact(client_sock, msg_buf + 16, msg_len - 16) < 0) {
                break;
            }
        }
        
        /* Process message */
        process_message(msg_buf, &first_output);
    }
    
    fprintf(g_output, "\n]\n");
    fflush(g_output);
    
    close(client_sock);
    fprintf(stderr, "Connection closed. Decoded %lu SAV rules\n", 
            (unsigned long)g_records_decoded);
}

/**
 * Main server loop
 */
static int run_server(const char *host, uint16_t port, const char *output_file) {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    /* Open output file */
    if (output_file) {
        g_output = fopen(output_file, "w");
        if (!g_output) {
            fprintf(stderr, "Failed to open %s: %s\n", output_file, strerror(errno));
            return EXIT_FAILURE;
        }
    } else {
        g_output = stdout;
    }
    
    /* Create socket */
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        fprintf(stderr, "socket() failed: %s\n", strerror(errno));
        return EXIT_FAILURE;
    }
    
    /* Set socket options */
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    /* Bind */
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host, &server_addr.sin_addr);
    
    if (bind(server_sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fprintf(stderr, "bind() failed: %s\n", strerror(errno));
        close(server_sock);
        return EXIT_FAILURE;
    }
    
    /* Listen */
    if (listen(server_sock, 5) < 0) {
        fprintf(stderr, "listen() failed: %s\n", strerror(errno));
        close(server_sock);
        return EXIT_FAILURE;
    }
    
    fprintf(stderr, "Listening on %s:%u\n", host, port);
    
    /* Accept connections */
    while (g_running) {
        client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_len);
        if (client_sock < 0) {
            if (errno == EINTR) continue;
            fprintf(stderr, "accept() failed: %s\n", strerror(errno));
            break;
        }
        
        handle_client(client_sock, &client_addr);
        
        /* Reset for next connection */
        g_records_decoded = 0;
        g_template_count = 0;
    }
    
    close(server_sock);
    if (g_output && g_output != stdout) fclose(g_output);
    
    return EXIT_SUCCESS;
}

/**
 * Parse command line arguments
 */
static int parse_args(int argc, char **argv, char **host, uint16_t *port, char **output) {
    *host = "127.0.0.1";
    *port = 4739;
    *output = NULL;
    
    for (int i = 1; i < argc; i++) {
        if (strncmp(argv[i], "--listen=", 9) == 0) {
            char *spec = argv[i] + 9;
            if (strncmp(spec, "tcp://", 6) == 0) spec += 6;
            
            char *colon = strrchr(spec, ':');
            if (colon) {
                *colon = '\0';
                *host = spec;
                *port = atoi(colon + 1);
            }
        } else if (strncmp(argv[i], "--output=", 9) == 0) {
            *output = argv[i] + 9;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [--listen=tcp://HOST:PORT] [--output=FILE]\n", argv[0]);
            return 0;
        }
    }
    
    return 1;
}

/**
 * Main entry point
 */
int main(int argc, char **argv) {
    char *host, *output;
    uint16_t port;
    
    if (!parse_args(argc, argv, &host, &port, &output)) {
        return EXIT_SUCCESS;
    }
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    
    return run_server(host, port, output);
}
