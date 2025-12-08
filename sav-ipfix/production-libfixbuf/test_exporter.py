#!/usr/bin/env python3
"""
Test IPFIX Exporter for SAV Rules with SubTemplateList
Sends SAV rules using SubTemplateList (RFC 6313) to test the collector

Usage:
    python3 test_exporter.py --host 127.0.0.1 --port 4739 --transport tcp
"""

import socket
import struct
import argparse
import time
from typing import List, Tuple

class IPFIXExporter:
    """Simple IPFIX exporter for testing SAV collector"""
    
    # IPFIX version
    VERSION = 10
    
    # Template IDs
    TEMPLATE_ID_MAIN = 256  # Main SAV record template
    TEMPLATE_ID_901 = 901   # IPv4 Interface-to-Prefix
    
    # Set IDs
    SET_ID_TEMPLATE = 2
    SET_ID_DATA = TEMPLATE_ID_MAIN  # Data Set ID must match Template ID
    
    def __init__(self, host: str, port: int, transport: str = 'tcp'):
        self.host = host
        self.port = port
        self.transport = transport.lower()
        self.sequence = 0
        self.observation_domain = 1
        
    def _pack_message_header(self, length: int) -> bytes:
        """Pack IPFIX Message Header (16 bytes)
        RFC 7011: Version(2) + Length(2) + ExportTime(4) + SeqNum(4) + ObsDomainID(4)
        """
        export_time = int(time.time())
        self.sequence += 1
        
        return struct.pack('!HHIII',
            self.VERSION,              # Version (2 bytes)
            length,                    # Length (2 bytes)
            export_time,               # Export Time (4 bytes)
            self.sequence,             # Sequence Number (4 bytes)
            self.observation_domain    # Observation Domain ID (4 bytes)
        )
    
    def _pack_set_header(self, set_id: int, length: int) -> bytes:
        """Pack Set Header (4 bytes)"""
        return struct.pack('!HH', set_id, length)
    
    def create_template_set(self) -> bytes:
        """Create Template Set for SAV main record
        
        Template contains:
        - savRuleType (1 byte)
        - savTargetType (1 byte)
        - savMatchedContentList (SubTemplateList, variable)
        - savPolicyAction (1 byte)
        
        Note: IE numbers >32767 need enterprise bit (0x8000) set in IPFIX.
        For private IEs 50000-50003, we set enterprise bit + enterprise ID.
        """
        # Template Record Header
        template_header = struct.pack('!HH',
            self.TEMPLATE_ID_MAIN,  # Template ID
            4                        # Field Count
        )
        
        # Field Specifiers with enterprise bit
        # Format: IE number (2 bytes, high bit=1 for enterprise) + length (2 bytes) + enterprise ID (4 bytes)
        fields = b''
        
        # savRuleType (IE 50000, 1 byte, enterprise 0)
        fields += struct.pack('!HHI', 0x8000 | (50000 & 0x7FFF), 1, 0)
        
        # savTargetType (IE 50001, 1 byte, enterprise 0)
        fields += struct.pack('!HHI', 0x8000 | (50001 & 0x7FFF), 1, 0)
        
        # savMatchedContentList (IE 50002, variable, enterprise 0)
        fields += struct.pack('!HHI', 0x8000 | (50002 & 0x7FFF), 0xFFFF, 0)
        
        # savPolicyAction (IE 50003, 1 byte, enterprise 0)
        fields += struct.pack('!HHI', 0x8000 | (50003 & 0x7FFF), 1, 0)
        
        template_record = template_header + fields
        
        # Set Header + Template Record
        set_length = 4 + len(template_record)
        set_header = self._pack_set_header(self.SET_ID_TEMPLATE, set_length)
        
        return set_header + template_record
    
    def create_subtemplatelist_901(self, rules: List[Tuple[int, str, int]]) -> bytes:
        """Create SubTemplateList with Template 901 (IPv4 Interface-to-Prefix)
        
        Args:
            rules: List of (interface_index, ipv4_prefix, prefix_length)
        
        Returns:
            SubTemplateList bytes
        """
        # SubTemplateList Header (3 bytes)
        # Semantic: 0xFF (allOf), Template ID: 901
        stl_header = struct.pack('!BH', 0xFF, 901)
        
        # Encode all rules
        stl_data = b''
        for interface, prefix, prefix_len in rules:
            # Parse IPv4 address
            ip_bytes = socket.inet_aton(prefix)
            
            # Encode: interface(4) + ipv4(4) + prefix_length(1) = 9 bytes
            stl_data += struct.pack('!I4sB',
                interface,
                ip_bytes,
                prefix_len
            )
        
        return stl_header + stl_data
    
    def create_data_set(self, sav_rules: List[Tuple[int, str, int]]) -> bytes:
        """Create Data Set with SAV records
        
        Args:
            sav_rules: List of (interface_index, ipv4_prefix, prefix_length)
        """
        # Create SubTemplateList
        stl = self.create_subtemplatelist_901(sav_rules)
        
        # Encode main SAV record
        data_record = b''
        data_record += struct.pack('!B', 0)       # savRuleType: 0 (allowlist)
        data_record += struct.pack('!B', 0)       # savTargetType: 0 (interface-based)
        data_record += struct.pack('!H', len(stl))  # Variable length field
        data_record += stl                         # SubTemplateList
        data_record += struct.pack('!B', 0)       # savPolicyAction: 0 (permit)
        
        # Set Header + Data Record
        set_length = 4 + len(data_record)
        set_header = self._pack_set_header(self.SET_ID_DATA, set_length)
        
        return set_header + data_record
    
    def send_sav_rules(self, rules: List[Tuple[int, str, int]]):
        """Send SAV rules to collector
        
        Args:
            rules: List of (interface_index, ipv4_prefix, prefix_length)
        """
        # Build IPFIX Message
        template_set = self.create_template_set()
        data_set = self.create_data_set(rules)
        
        message_body = template_set + data_set
        message_length = 16 + len(message_body)  # Header + Body
        
        message_header = self._pack_message_header(message_length)
        ipfix_message = message_header + message_body
        
        # Send via TCP or UDP
        if self.transport == 'tcp':
            with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
                sock.connect((self.host, self.port))
                sock.sendall(ipfix_message)
                print(f"Sent {len(ipfix_message)} bytes via TCP to {self.host}:{self.port}")
        elif self.transport == 'udp':
            with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
                sock.sendto(ipfix_message, (self.host, self.port))
                print(f"Sent {len(ipfix_message)} bytes via UDP to {self.host}:{self.port}")
        else:
            raise ValueError(f"Unsupported transport: {self.transport}")
        
        print(f"Exported {len(rules)} SAV rules in SubTemplateList")

def main():
    parser = argparse.ArgumentParser(description='Test IPFIX exporter for SAV rules')
    parser.add_argument('--host', default='127.0.0.1', help='Collector host')
    parser.add_argument('--port', type=int, default=4739, help='Collector port')
    parser.add_argument('--transport', choices=['tcp', 'udp'], default='tcp',
                        help='Transport protocol')
    
    args = parser.parse_args()
    
    # Create test SAV rules
    test_rules = [
        (1, '192.0.2.0', 24),      # Interface 1 -> 192.0.2.0/24
        (2, '198.51.100.0', 24),   # Interface 2 -> 198.51.100.0/24
        (3, '203.0.113.0', 24),    # Interface 3 -> 203.0.113.0/24
    ]
    
    print("=== SAV IPFIX Test Exporter ===")
    print(f"Target: {args.host}:{args.port} ({args.transport.upper()})")
    print(f"Rules to export: {len(test_rules)}")
    for iface, prefix, plen in test_rules:
        print(f"  - Interface {iface} -> {prefix}/{plen}")
    print()
    
    # Create exporter and send
    exporter = IPFIXExporter(args.host, args.port, args.transport)
    exporter.send_sav_rules(test_rules)
    
    print("\nDone! Check collector output for decoded rules.")

if __name__ == '__main__':
    main()
