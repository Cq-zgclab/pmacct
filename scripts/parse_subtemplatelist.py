#!/usr/bin/env python3
"""
SubTemplateList Parser for ipfixcol2 hex output
Decodes SAV rules from basicList/subTemplateList encoding (RFC 6313)

This parser handles the hex string output from ipfixcol2 when it receives
SubTemplateList IEs that are not automatically decoded.

Author: SAV IPFIX Implementation Team
Date: 2025-12-08
"""

import struct
import json
from typing import List, Dict, Any, Tuple
import sys


class SubTemplateListParser:
    """Parser for IPFIX SubTemplateList (RFC 6313) encoded as hex strings"""
    
    # IPFIX Template IDs for SAV
    TEMPLATE_SAV_RULE = 901      # Main SAV rule template
    TEMPLATE_PREFIX = 902        # Prefix-based rule
    TEMPLATE_AS = 903           # AS-based rule
    TEMPLATE_INTERFACE = 904    # Interface-based rule
    
    # SAV IE IDs (enterprise 0, private space)
    IE_SAV_VALIDATION_METHOD = 30001
    IE_SAV_VALIDATION_STATUS = 30002
    IE_SAV_RULE_LIST = 30003
    IE_SAV_RULE_COUNT = 30004
    
    def __init__(self, debug: bool = False):
        self.debug = debug
        self.offset = 0
        self.data = b''
    
    def _log(self, msg: str):
        """Debug logging"""
        if self.debug:
            print(f"[DEBUG] Offset {self.offset}: {msg}", file=sys.stderr)
    
    def _read_byte(self) -> int:
        """Read one byte and advance offset"""
        if self.offset >= len(self.data):
            raise ValueError(f"Unexpected end of data at offset {self.offset}")
        val = self.data[self.offset]
        self.offset += 1
        return val
    
    def _read_bytes(self, n: int) -> bytes:
        """Read n bytes and advance offset"""
        if self.offset + n > len(self.data):
            raise ValueError(f"Cannot read {n} bytes at offset {self.offset}")
        val = self.data[self.offset:self.offset+n]
        self.offset += n
        return val
    
    def _read_uint16(self) -> int:
        """Read 2-byte unsigned integer (network byte order)"""
        return struct.unpack("!H", self._read_bytes(2))[0]
    
    def _read_uint32(self) -> int:
        """Read 4-byte unsigned integer (network byte order)"""
        return struct.unpack("!I", self._read_bytes(4))[0]
    
    def _read_variable_length(self) -> int:
        """
        Read IPFIX variable-length encoding (RFC 7011 Section 7)
        - If first byte < 255: length is that byte
        - If first byte == 255: next 2 bytes are length
        """
        length_byte = self._read_byte()
        if length_byte < 255:
            self._log(f"Variable length: {length_byte}")
            return length_byte
        else:
            length = self._read_uint16()
            self._log(f"Variable length (extended): {length}")
            return length
    
    def _read_ipv4_address(self) -> str:
        """Read 4-byte IPv4 address"""
        octets = self._read_bytes(4)
        return ".".join(str(b) for b in octets)
    
    def _read_ipv6_address(self) -> str:
        """Read 16-byte IPv6 address"""
        octets = self._read_bytes(16)
        # Format as IPv6
        parts = []
        for i in range(0, 16, 2):
            parts.append(f"{octets[i]:02x}{octets[i+1]:02x}")
        return ":".join(parts)
    
    def parse_subtemplatelist_header(self) -> Tuple[int, int]:
        """
        Parse subTemplateList header (RFC 6313 Section 4.5.3)
        Returns: (semantic, template_id)
        
        Format (per our sender's build_sub_template_list):
        - Semantic (1 byte): describes meaning of list
        - Template ID (2 bytes): template being used (901-904)
        
        Note: Our sender uses variable-length encoding for the entire
        subTemplateList, so the length is parsed separately.
        """
        semantic = self._read_byte()
        self._log(f"SubTemplateList semantic: 0x{semantic:02x}")
        
        # Template ID (2 bytes, network byte order)
        template_id = self._read_uint16()
        self._log(f"Template ID: {template_id}")
        
        return semantic, template_id
    
    def parse_sav_rule_901(self) -> Dict[str, Any]:
        """
        Parse a single SAV rule from template 901 (IPv4 Interface-to-Prefix)
        
        Template 901 structure (9 bytes):
        - interface_id (uint32): Interface identifier
        - ipv4_prefix (uint32): IPv4 address
        - prefix_len (uint8): Prefix length (0-32)
        """
        # Read interface ID (4 bytes)
        interface_id = self._read_uint32()
        self._log(f"Interface ID: {interface_id}")
        
        # Read IPv4 prefix (4 bytes)
        ipv4_addr = self._read_ipv4_address()
        self._log(f"IPv4 address: {ipv4_addr}")
        
        # Read prefix length (1 byte)
        prefix_len = self._read_byte()
        self._log(f"Prefix length: {prefix_len}")
        
        return {
            "type": "ipv4_interface_to_prefix",
            "templateId": 901,
            "interfaceId": interface_id,
            "prefix": f"{ipv4_addr}/{prefix_len}",
            "ipv4Address": ipv4_addr,
            "prefixLength": prefix_len
        }
    
    def parse_sav_rule_902(self) -> Dict[str, Any]:
        """
        Parse a single SAV rule from template 902 (IPv6 Interface-to-Prefix)
        
        Template 902 structure (21 bytes):
        - interface_id (uint32): Interface identifier
        - ipv6_prefix (16 bytes): IPv6 address
        - prefix_len (uint8): Prefix length (0-128)
        """
        interface_id = self._read_uint32()
        self._log(f"Interface ID: {interface_id}")
        
        ipv6_addr = self._read_ipv6_address()
        self._log(f"IPv6 address: {ipv6_addr}")
        
        prefix_len = self._read_byte()
        self._log(f"Prefix length: {prefix_len}")
        
        return {
            "type": "ipv6_interface_to_prefix",
            "templateId": 902,
            "interfaceId": interface_id,
            "prefix": f"{ipv6_addr}/{prefix_len}",
            "ipv6Address": ipv6_addr,
            "prefixLength": prefix_len
        }
    
    def parse_sav_rule_903(self) -> Dict[str, Any]:
        """
        Parse a single SAV rule from template 903 (IPv4 Prefix-to-Interface)
        
        Template 903 structure (9 bytes):
        - ipv4_prefix (uint32): IPv4 address
        - prefix_len (uint8): Prefix length
        - interface_id (uint32): Interface identifier
        """
        ipv4_addr = self._read_ipv4_address()
        self._log(f"IPv4 address: {ipv4_addr}")
        
        prefix_len = self._read_byte()
        self._log(f"Prefix length: {prefix_len}")
        
        interface_id = self._read_uint32()
        self._log(f"Interface ID: {interface_id}")
        
        return {
            "type": "ipv4_prefix_to_interface",
            "templateId": 903,
            "prefix": f"{ipv4_addr}/{prefix_len}",
            "ipv4Address": ipv4_addr,
            "prefixLength": prefix_len,
            "interfaceId": interface_id
        }
    
    def parse_sav_rule_904(self) -> Dict[str, Any]:
        """
        Parse a single SAV rule from template 904 (IPv6 Prefix-to-Interface)
        
        Template 904 structure (21 bytes):
        - ipv6_prefix (16 bytes): IPv6 address
        - prefix_len (uint8): Prefix length
        - interface_id (uint32): Interface identifier
        """
        ipv6_addr = self._read_ipv6_address()
        self._log(f"IPv6 address: {ipv6_addr}")
        
        prefix_len = self._read_byte()
        self._log(f"Prefix length: {prefix_len}")
        
        interface_id = self._read_uint32()
        self._log(f"Interface ID: {interface_id}")
        
        return {
            "type": "ipv6_prefix_to_interface",
            "templateId": 904,
            "prefix": f"{ipv6_addr}/{prefix_len}",
            "ipv6Address": ipv6_addr,
            "prefixLength": prefix_len,
            "interfaceId": interface_id
        }
    
    def parse(self, hex_string: str) -> List[Dict[str, Any]]:
        """
        Parse ipfixcol2 hex string to structured SAV rules
        
        Args:
            hex_string: Hex-encoded subTemplateList (e.g., "0x030385...")
        
        Returns:
            List of SAV rule dictionaries
        
        Example:
            parser = SubTemplateListParser(debug=True)
            rules = parser.parse("0x03038500000001C0...")
            # Returns: [{"interfaceId": 1, "prefix": "192.0.2.0/24", ...}, ...]
        """
        # Remove 0x prefix if present
        if hex_string.startswith("0x") or hex_string.startswith("0X"):
            hex_string = hex_string[2:]
        
        # Convert to bytes
        try:
            self.data = bytes.fromhex(hex_string)
        except ValueError as e:
            raise ValueError(f"Invalid hex string: {e}")
        
        self.offset = 0
        self._log(f"Total data length: {len(self.data)} bytes")
        
        # Parse subTemplateList header (semantic + template ID)
        semantic, template_id = self.parse_subtemplatelist_header()
        
        self._log(f"Semantic: {semantic}, Template: {template_id}")
        
        # Calculate remaining data length (total - header)
        remaining_bytes = len(self.data) - self.offset
        self._log(f"Data records: {remaining_bytes} bytes")
        
        # Parse individual rules based on template
        rules = []
        
        if template_id == 901:  # IPv4 Interface-to-Prefix (9 bytes per rule)
            record_size = 9
            num_records = remaining_bytes // record_size
            self._log(f"Expected {num_records} records of {record_size} bytes each")
            
            for i in range(num_records):
                try:
                    rule = self.parse_sav_rule_901()
                    rule["ruleIndex"] = i + 1
                    rules.append(rule)
                    self._log(f"Parsed rule {i+1}: {rule}")
                except Exception as e:
                    self._log(f"Error parsing rule {i+1}: {e}")
                    rules.append({
                        "error": str(e),
                        "ruleIndex": i + 1,
                        "offset": self.offset
                    })
                    break
        
        elif template_id == 902:  # IPv6 Interface-to-Prefix (21 bytes per rule)
            record_size = 21
            num_records = remaining_bytes // record_size
            for i in range(num_records):
                try:
                    rule = self.parse_sav_rule_902()
                    rule["ruleIndex"] = i + 1
                    rules.append(rule)
                except Exception as e:
                    self._log(f"Error parsing rule {i+1}: {e}")
                    break
        
        elif template_id == 903:  # IPv4 Prefix-to-Interface (9 bytes per rule)
            record_size = 9
            num_records = remaining_bytes // record_size
            for i in range(num_records):
                try:
                    rule = self.parse_sav_rule_903()
                    rule["ruleIndex"] = i + 1
                    rules.append(rule)
                except Exception as e:
                    self._log(f"Error parsing rule {i+1}: {e}")
                    break
        
        elif template_id == 904:  # IPv6 Prefix-to-Interface (21 bytes per rule)
            record_size = 21
            num_records = remaining_bytes // record_size
            for i in range(num_records):
                try:
                    rule = self.parse_sav_rule_904()
                    rule["ruleIndex"] = i + 1
                    rules.append(rule)
                except Exception as e:
                    self._log(f"Error parsing rule {i+1}: {e}")
                    break
        
        else:
            self._log(f"Unknown template ID: {template_id}")
            rules.append({
                "error": f"Unknown template ID: {template_id}",
                "rawData": self.data[self.offset:].hex()
            })
        
        return rules


def parse_ipfixcol2_json(json_file: str, debug: bool = False) -> List[Dict[str, Any]]:
    """
    Parse ipfixcol2 JSON output file and extract SAV rules
    
    Args:
        json_file: Path to ipfixcol2 JSON output file
        debug: Enable debug logging
    
    Returns:
        List of all SAV rules from all IPFIX records
    """
    parser = SubTemplateListParser(debug=debug)
    all_rules = []
    
    with open(json_file, 'r') as f:
        for line in f:
            try:
                record = json.loads(line.strip())
                
                # Look for SAV rule list IE (en0:id30003)
                if "en0:id30003" in record:
                    hex_string = record["en0:id30003"]
                    
                    # Parse the subTemplateList
                    rules = parser.parse(hex_string)
                    
                    # Add context from the IPFIX record
                    for rule in rules:
                        rule["ipfixRecord"] = {
                            "sourceIP": record.get("iana:sourceIPv4Address"),
                            "destIP": record.get("iana:destinationIPv4Address"),
                            "validationMethod": record.get("en0:id30001"),
                            "validationStatus": record.get("en0:id30002"),
                            "ruleCount": record.get("en0:id30004")
                        }
                    
                    all_rules.extend(rules)
            
            except json.JSONDecodeError:
                continue
            except Exception as e:
                if debug:
                    print(f"Error processing record: {e}", file=sys.stderr)
                continue
    
    return all_rules


def main():
    """Command-line interface"""
    import argparse
    
    parser = argparse.ArgumentParser(
        description="Parse SubTemplateList from ipfixcol2 hex output",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Parse hex string directly
  %(prog)s --hex "0x03038500000001C00002001800000002C63364001800000003CB00710018"
  
  # Parse from ipfixcol2 JSON file
  %(prog)s --file /tmp/ipfixcol/sav_202512080332
  
  # Enable debug output
  %(prog)s --file /tmp/ipfixcol/sav_* --debug
        """
    )
    
    parser.add_argument('--hex', type=str,
                        help='Hex string to parse (e.g., "0x030385...")')
    parser.add_argument('--file', type=str,
                        help='ipfixcol2 JSON output file to parse')
    parser.add_argument('--debug', action='store_true',
                        help='Enable debug logging')
    parser.add_argument('--pretty', action='store_true',
                        help='Pretty-print JSON output')
    
    args = parser.parse_args()
    
    if not args.hex and not args.file:
        parser.print_help()
        sys.exit(1)
    
    try:
        if args.hex:
            # Parse hex string directly
            stl_parser = SubTemplateListParser(debug=args.debug)
            rules = stl_parser.parse(args.hex)
        else:
            # Parse from file
            rules = parse_ipfixcol2_json(args.file, debug=args.debug)
        
        # Output results
        if args.pretty:
            print(json.dumps(rules, indent=2))
        else:
            print(json.dumps(rules))
    
    except Exception as e:
        print(f"Error: {e}", file=sys.stderr)
        if args.debug:
            import traceback
            traceback.print_exc()
        sys.exit(1)


if __name__ == "__main__":
    main()
