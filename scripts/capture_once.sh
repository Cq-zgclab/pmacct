#!/bin/bash
# Capture 4 UDP packets on lo:4739 and export human-readable text
PCAP="$HOME/pmacct/ipfix_capture.pcap"
TXT="$HOME/pmacct/ipfix_capture.txt"

echo "Capturing 4 UDP packets on lo:4739 -> $PCAP"
sudo tcpdump -i lo -nn -s 0 -w "$PCAP" 'udp port 4739' -c 4

if [ -f "$PCAP" ]; then
  echo "Exporting human-readable text -> $TXT"
  sudo tcpdump -r "$PCAP" -nn -vvv -s 0 'udp port 4739' > "$TXT" || true
  echo "Capture and export complete"
else
  echo "No pcap produced"
fi
