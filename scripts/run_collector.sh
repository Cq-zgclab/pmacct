#!/bin/bash
# Build and run go-ipfix collector; log to ~/pmacct/collector.log
set -e
WORKDIR="$HOME/pmacct/third_party/go-ipfix"
LOGFILE="$HOME/pmacct/collector.log"
PIDFILE="$HOME/pmacct/collector.pid"

echo "Building go-ipfix in $WORKDIR"
cd "$WORKDIR"
if [ -f go.mod ]; then
  echo "go.mod present, running go mod tidy"
  go mod tidy
fi

# build collector binary (path may vary)
if [ -d ./cmd/collector ]; then
  echo "Building cmd/collector"
  go build -o collector ./cmd/collector
else
  echo "Attempting generic build"
  go build -o collector ./...
fi

# start collector in background
echo "Starting collector (udp:4739) -> $LOGFILE"
nohup ./collector --ipfix.addr 0.0.0.0 --ipfix.port 4739 --ipfix.transport udp > "$LOGFILE" 2>&1 &
echo $! > "$PIDFILE"
echo "Collector started with PID $(cat $PIDFILE)"
