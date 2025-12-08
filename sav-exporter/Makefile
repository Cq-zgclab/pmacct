# Makefile for SAV-Exporter
#
# Build IPFIX exporter and collector using libfixbuf

CC = gcc
CFLAGS = -Wall -Wextra -O2 -g -I./include
LDFLAGS = -L/usr/local/lib
LIBS = -lfixbuf $(shell pkg-config --libs glib-2.0) -lsctp -lpthread

# Source directories
SRC_DIR = src
BIN_DIR = bin

# Source files
COMMON_SRCS = $(SRC_DIR)/sav_info_elements.c $(SRC_DIR)/sav_records.c
EXPORTER_SRCS = $(SRC_DIR)/sav_exporter.c $(COMMON_SRCS)
COLLECTOR_SRCS = $(SRC_DIR)/sav_collector.c $(COMMON_SRCS)

# Output binaries
EXPORTER_BIN = $(BIN_DIR)/sav_exporter
COLLECTOR_BIN = $(BIN_DIR)/sav_collector

# Targets
.PHONY: all clean test

all: $(BIN_DIR) $(EXPORTER_BIN) $(COLLECTOR_BIN)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

$(EXPORTER_BIN): $(EXPORTER_SRCS)
	$(CC) $(CFLAGS) $(shell pkg-config --cflags glib-2.0) \
		$(EXPORTER_SRCS) \
		$(LDFLAGS) $(LIBS) \
		-o $(EXPORTER_BIN)
	@echo "Built: $(EXPORTER_BIN)"

$(COLLECTOR_BIN): $(COLLECTOR_SRCS)
	$(CC) $(CFLAGS) $(shell pkg-config --cflags glib-2.0) \
		$(COLLECTOR_SRCS) \
		$(LDFLAGS) $(LIBS) \
		-o $(COLLECTOR_BIN)
	@echo "Built: $(COLLECTOR_BIN)"

clean:
	rm -rf $(BIN_DIR)
	rm -f *.o
	@echo "Cleaned build artifacts"

test: all
	@echo "=== Testing SAV-Exporter ==="
	@echo "Starting collector in background..."
	LD_LIBRARY_PATH=/usr/local/lib $(COLLECTOR_BIN) --listen tcp://127.0.0.1:4739 &
	@sleep 2
	@echo "Running exporter in test mode..."
	LD_LIBRARY_PATH=/usr/local/lib $(EXPORTER_BIN) --connect tcp://127.0.0.1:4739 --test-mode
	@sleep 1
	@pkill -INT sav_collector || true
	@echo "=== Test completed ==="

help:
	@echo "SAV-Exporter Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  all        Build exporter and collector (default)"
	@echo "  clean      Remove build artifacts"
	@echo "  test       Run basic connectivity test"
	@echo "  help       Show this help message"
	@echo ""
	@echo "Binaries:"
	@echo "  $(EXPORTER_BIN) - IPFIX exporter"
	@echo "  $(COLLECTOR_BIN) - IPFIX collector"
