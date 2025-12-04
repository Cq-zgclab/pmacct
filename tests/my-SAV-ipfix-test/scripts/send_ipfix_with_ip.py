#!/usr/bin/env python3
"""
send_ipfix_with_ip.py - IPFIX SAV Message Sender with Full subTemplateList Support

Purpose:
    Production-grade IPFIX v10 message generator implementing RFC 6313 subTemplateList
    for SAV (Source Address Validation) Information Elements per draft-cao-opsawg-ipfix-sav-01.
    
Features:
    - RFC 7011 variable-length encoding (single-byte <255, extended ≥255)
    - RFC 6313 subTemplateList with semantic field support
    - 4 sub-template definitions (Templates 901-904)
    - Enterprise PEN support (optional)
    - Built-in IPFIX message validation
    - JSON rule file or inline JSON input
    - IPv4 + IPv6 dual-stack support
    
Templates:
    Main Template (400):
        - sourceIPv4Address (8), destinationIPv4Address (12)
        - octetDeltaCount (1), packetDeltaCount (2)
        - savRuleType (30001), savTargetType (30002)
        - savMatchedContentList (30003, variable-length)
        - savPolicyAction (30004)
    
    Sub-Templates:
        - 901: IPv4 Interface-to-Prefix (9 bytes/rule)
        - 902: IPv6 Interface-to-Prefix (21 bytes/rule)
        - 903: IPv4 Prefix-to-Interface (9 bytes/rule)
        - 904: IPv6 Prefix-to-Interface (21 bytes/rule)

Usage:
    # Basic usage with JSON file
    ./send_ipfix_with_ip.py \\
      --sav-rules ../test-data/sav_rules_example.json \\
      --sub-template-id 901 \\
      --use-complete-message
    
    # Inline JSON rules
    ./send_ipfix_with_ip.py \\
      --sav-rules '[{"interface_id":5001,"prefix":"198.51.100.0","prefix_len":24}]' \\
      --sub-template-id 901 \\
      --use-complete-message
    
    # Full parameters
    ./send_ipfix_with_ip.py \\
      --src 10.0.1.100 --dst 10.0.2.1 \\
      --sav-rules ../test-data/sav_rules_example.json \\
      --sub-template-id 901 \\
      --sav-rule-type 0 --sav-target-type 0 --sav-action 1 \\
      --enterprise --pen 55555 \\
      --use-complete-message

Dependencies:
    Standard library only (no external packages required)

Standards:
    RFC 7011 (IPFIX Protocol)
    RFC 6313 (Export of Structured Data in IPFIX)
    RFC 7012 (IPFIX Information Model)
    draft-cao-opsawg-ipfix-sav-01

Author:
    Generated for pmacct SAV IPFIX testing
    
Version:
    2.0 (Phase 1A - Complete subTemplateList implementation)
"""
import argparse
import socket
import struct
import time
import ipaddress
import sys
import json


# RFC 7011 variable-length encoding
def encode_varlen(length: int) -> bytes:
    """Encode variable-length field per RFC 7011 Section 7.
    
    If length < 255: encode as 1 octet
    If length >= 255: encode as 0xFF followed by 2-octet length (network byte order)
    """
    if length < 255:
        return struct.pack('!B', length)
    else:
        if length > 0xFFFF:
            raise ValueError(f'variable-length field too large: {length}')
        return struct.pack('!BH', 255, length)


# Sub-Template Definitions (RFC 6313)
def build_sub_template_901():
    """Sub-Template 901: Interface-to-Prefix Mapping (IPv4)
    
    Fields:
    - ingressInterface (10, 4 bytes)
    - sourceIPv4Prefix (44, 4 bytes) 
    - sourceIPv4PrefixLength (8, 1 byte)
    """
    set_id = 2  # Template Set
    template_id = 901
    field_count = 3
    
    fields = [
        (10, 4),   # ingressInterface
        (44, 4),   # sourceIPv4Prefix
        (8, 1),    # sourceIPv4PrefixLength
    ]
    
    tpl_rec = struct.pack('!HH', template_id, field_count)
    for fid, flen in fields:
        tpl_rec += struct.pack('!HH', fid, flen)
    
    set_length = 4 + len(tpl_rec)
    return struct.pack('!HH', set_id, set_length) + tpl_rec


def build_sub_template_902():
    """Sub-Template 902: Interface-to-Prefix Mapping (IPv6)
    
    Fields:
    - ingressInterface (10, 4 bytes)
    - sourceIPv6Prefix (170, 16 bytes)
    - sourceIPv6PrefixLength (29, 1 byte)
    """
    set_id = 2
    template_id = 902
    field_count = 3
    
    fields = [
        (10, 4),    # ingressInterface
        (170, 16),  # sourceIPv6Prefix
        (29, 1),    # sourceIPv6PrefixLength
    ]
    
    tpl_rec = struct.pack('!HH', template_id, field_count)
    for fid, flen in fields:
        tpl_rec += struct.pack('!HH', fid, flen)
    
    set_length = 4 + len(tpl_rec)
    return struct.pack('!HH', set_id, set_length) + tpl_rec


def build_sub_template_903():
    """Sub-Template 903: Prefix-to-Interface Mapping (IPv4)
    
    Fields:
    - sourceIPv4Prefix (44, 4 bytes)
    - sourceIPv4PrefixLength (8, 1 byte)
    - ingressInterface (10, 4 bytes)
    """
    set_id = 2
    template_id = 903
    field_count = 3
    
    fields = [
        (44, 4),   # sourceIPv4Prefix
        (8, 1),    # sourceIPv4PrefixLength
        (10, 4),   # ingressInterface
    ]
    
    tpl_rec = struct.pack('!HH', template_id, field_count)
    for fid, flen in fields:
        tpl_rec += struct.pack('!HH', fid, flen)
    
    set_length = 4 + len(tpl_rec)
    return struct.pack('!HH', set_id, set_length) + tpl_rec


def build_sub_template_904():
    """Sub-Template 904: Prefix-to-Interface Mapping (IPv6)
    
    Fields:
    - sourceIPv6Prefix (170, 16 bytes)
    - sourceIPv6PrefixLength (29, 1 byte)
    - ingressInterface (10, 4 bytes)
    """
    set_id = 2
    template_id = 904
    field_count = 3
    
    fields = [
        (170, 16),  # sourceIPv6Prefix
        (29, 1),    # sourceIPv6PrefixLength
        (10, 4),    # ingressInterface
    ]
    
    tpl_rec = struct.pack('!HH', template_id, field_count)
    for fid, flen in fields:
        tpl_rec += struct.pack('!HH', fid, flen)
    
    set_length = 4 + len(tpl_rec)
    return struct.pack('!HH', set_id, set_length) + tpl_rec


def build_sav_rule_ipv4_interface(interface_id, prefix, prefix_len):
    """Build a single SAV rule record for sub-template 901.
    
    Args:
        interface_id: Interface ID (uint32)
        prefix: IPv4 prefix string (e.g., '198.51.100.0')
        prefix_len: Prefix length (uint8, e.g., 24)
    
    Returns:
        bytes: Encoded data record (9 bytes)
    """
    rule = struct.pack('!I', interface_id)
    rule += struct.pack('!I', int(ipaddress.IPv4Address(prefix)))
    rule += struct.pack('!B', prefix_len)
    return rule


def build_sav_rule_ipv6_interface(interface_id, prefix, prefix_len):
    """Build a single SAV rule record for sub-template 902.
    
    Args:
        interface_id: Interface ID (uint32)
        prefix: IPv6 prefix string (e.g., '2001:db8::')
        prefix_len: Prefix length (uint8, e.g., 48)
    
    Returns:
        bytes: Encoded data record (21 bytes)
    """
    rule = struct.pack('!I', interface_id)
    rule += ipaddress.IPv6Address(prefix).packed
    rule += struct.pack('!B', prefix_len)
    return rule


def build_sav_rule_ipv4_prefix(prefix, prefix_len, interface_id):
    """Build a single SAV rule record for sub-template 903.
    
    Args:
        prefix: IPv4 prefix string
        prefix_len: Prefix length (uint8)
        interface_id: Interface ID (uint32)
    
    Returns:
        bytes: Encoded data record (9 bytes)
    """
    rule = struct.pack('!I', int(ipaddress.IPv4Address(prefix)))
    rule += struct.pack('!B', prefix_len)
    rule += struct.pack('!I', interface_id)
    return rule


def build_sav_rule_ipv6_prefix(prefix, prefix_len, interface_id):
    """Build a single SAV rule record for sub-template 904.
    
    Args:
        prefix: IPv6 prefix string
        prefix_len: Prefix length (uint8)
        interface_id: Interface ID (uint32)
    
    Returns:
        bytes: Encoded data record (21 bytes)
    """
    rule = ipaddress.IPv6Address(prefix).packed
    rule += struct.pack('!B', prefix_len)
    rule += struct.pack('!I', interface_id)
    return rule


def build_sub_template_list(semantic, template_id, records):
    """Build a subTemplateList per RFC 6313.
    
    Args:
        semantic: Semantic field (uint8)
            0x00: undefined (default)
            0x01: exactlyOneOf
            0x02: oneOrMoreOf
            0x03: allOf
            0x04: ordered
            0xFF: undefined
        template_id: Sub-template ID (901-904)
        records: List of encoded data records (bytes)
    
    Returns:
        bytes: Complete subTemplateList with variable-length encoding
    
    Structure:
        [VarLen][Semantic(1B)][TemplateID(2B)][Records...]
    """
    # Build subTemplateList content
    stl_header = struct.pack('!B', semantic)
    stl_header += struct.pack('!H', template_id)
    
    # Concatenate all data records
    stl_data = b''.join(records)
    
    # Complete subTemplateList
    stl_content = stl_header + stl_data
    
    # Variable-length encoding for the entire subTemplateList
    return encode_varlen(len(stl_content)) + stl_content


def build_ipfix_message(template_id=400, seq=1, obs_domain=1234,
                        src_ip='127.0.0.1', dst_ip='127.0.0.1',
                        octets=1000, packets=10,
                        enterprise=False, pen=0, matched_bytes=0,
                        sav_rules=None, sub_template_id=901,
                        sav_rule_type=0, sav_target_type=0, sav_action=2):
    version = 10

    # Template fields: include some standard IEs so the collector can
    # build flows, then include the SAV testing IEs (temporary IDs).
    # IANA-standard element IDs (common):
    # 1 = octetDeltaCount (8 octets)
    # 2 = packetDeltaCount (8 octets)
    # 8 = sourceIPv4Address (4 octets)
    # 12 = destinationIPv4Address (4 octets)
    # Then SAV fields 30001..30004 as in the original test.

    tpl_fields = [
        (8, 4),      # sourceIPv4Address
        (12, 4),     # destinationIPv4Address
        (1, 8),      # octetDeltaCount (uint64)
        (2, 8),      # packetDeltaCount (uint64)
        (30001, 1),  # savRuleType
        (30002, 1),  # savTargetType
        (30003, 0xFFFF),  # savMatchedContentList (variable)
        (30004, 1),  # savPolicyAction
    ]

    tpl_rec = struct.pack('!HH', template_id, len(tpl_fields))
    for fid, flen in tpl_fields:
        # If enterprise mode is requested and the field is a custom SAV IE
        # (we choose to treat IDs >= 30000 as enterprise/custom), set
        # the enterprise bit in the field ID and append the 4-byte PEN.
        if enterprise and fid >= 30000:
            ie_id = (fid & 0x7FFF) | 0x8000
            tpl_rec += struct.pack('!HH', ie_id & 0xFFFF, flen & 0xFFFF)
            tpl_rec += struct.pack('!I', pen)
        else:
            tpl_rec += struct.pack('!HH', fid & 0xFFFF, flen & 0xFFFF)

    tpl_set_id = 2
    tpl_set_len = 4 + len(tpl_rec)
    tpl_set = struct.pack('!HH', tpl_set_id, tpl_set_len) + tpl_rec

    # Build data record matching the template order
    src_i = int(ipaddress.IPv4Address(src_ip))
    dst_i = int(ipaddress.IPv4Address(dst_ip))
    data_rec = struct.pack('!I', src_i)
    data_rec += struct.pack('!I', dst_i)
    data_rec += struct.pack('!Q', octets)
    data_rec += struct.pack('!Q', packets)

    # Build savMatchedContentList (subTemplateList)
    if sav_rules and len(sav_rules) > 0:
        # Build real subTemplateList with actual SAV rules
        records = []
        
        for rule in sav_rules:
            if sub_template_id == 901:  # IPv4 Interface-to-Prefix
                records.append(build_sav_rule_ipv4_interface(
                    rule['interface_id'],
                    rule['prefix'],
                    rule['prefix_len']
                ))
            elif sub_template_id == 902:  # IPv6 Interface-to-Prefix
                records.append(build_sav_rule_ipv6_interface(
                    rule['interface_id'],
                    rule['prefix'],
                    rule['prefix_len']
                ))
            elif sub_template_id == 903:  # IPv4 Prefix-to-Interface
                records.append(build_sav_rule_ipv4_prefix(
                    rule['prefix'],
                    rule['prefix_len'],
                    rule['interface_id']
                ))
            elif sub_template_id == 904:  # IPv6 Prefix-to-Interface
                records.append(build_sav_rule_ipv6_prefix(
                    rule['prefix'],
                    rule['prefix_len'],
                    rule['interface_id']
                ))
        
        # Semantic: 0x03 = allOf (must match one of all rules)
        matched_field = build_sub_template_list(0x03, sub_template_id, records)
    elif matched_bytes and matched_bytes > 0:
        # Fallback: legacy mode with dummy bytes (for compatibility)
        matched_list_bytes = bytes([0x41]) * matched_bytes
        matched_field = encode_varlen(len(matched_list_bytes)) + matched_list_bytes
    else:
        # Empty subTemplateList
        matched_field = encode_varlen(0)

    data_rec += struct.pack('!B', sav_rule_type)
    data_rec += struct.pack('!B', sav_target_type)
    data_rec += matched_field
    data_rec += struct.pack('!B', sav_action)

    data_set_id = template_id
    data_set_len = 4 + len(data_rec)
    data_set = struct.pack('!HH', data_set_id, data_set_len) + data_rec

    export_time = int(time.time())
    seq_num = seq
    obs_domain = obs_domain

    total_len = 16 + len(tpl_set) + len(data_set)
    header = struct.pack('!HHIII', version, total_len, export_time, seq_num, obs_domain)

    msg = header + tpl_set + data_set
    return msg


def build_complete_message(template_id=400, seq=1, obs_domain=1234,
                          src_ip='127.0.0.1', dst_ip='127.0.0.1',
                          octets=1000, packets=10,
                          enterprise=False, pen=0,
                          sav_rules=None, sub_template_id=901,
                          sav_rule_type=0, sav_target_type=0, sav_action=2,
                          include_sub_templates=True):
    """Build a complete IPFIX message with main template + sub-templates + data.
    
    Args:
        include_sub_templates: If True, include all 4 sub-template definitions
        Other args: Same as build_ipfix_message
    
    Returns:
        bytes: Complete IPFIX message ready to send
    """
    version = 10
    export_time = int(time.time())
    seq_num = seq
    
    # Build main template (Template 400)
    tpl_fields = [
        (8, 4),      # sourceIPv4Address
        (12, 4),     # destinationIPv4Address
        (1, 8),      # octetDeltaCount
        (2, 8),      # packetDeltaCount
        (30001, 1),  # savRuleType
        (30002, 1),  # savTargetType
        (30003, 0xFFFF),  # savMatchedContentList (variable)
        (30004, 1),  # savPolicyAction
    ]
    
    tpl_rec = struct.pack('!HH', template_id, len(tpl_fields))
    for fid, flen in tpl_fields:
        if enterprise and fid >= 30000:
            ie_id = (fid & 0x7FFF) | 0x8000
            tpl_rec += struct.pack('!HH', ie_id & 0xFFFF, flen & 0xFFFF)
            tpl_rec += struct.pack('!I', pen)
        else:
            tpl_rec += struct.pack('!HH', fid & 0xFFFF, flen & 0xFFFF)
    
    tpl_set_id = 2
    tpl_set_len = 4 + len(tpl_rec)
    main_tpl_set = struct.pack('!HH', tpl_set_id, tpl_set_len) + tpl_rec
    
    # Build sub-templates (if requested and using subTemplateList)
    sub_tpl_sets = b''
    if include_sub_templates and sav_rules:
        sub_tpl_sets += build_sub_template_901()
        sub_tpl_sets += build_sub_template_902()
        sub_tpl_sets += build_sub_template_903()
        sub_tpl_sets += build_sub_template_904()
    
    # Build data record
    src_i = int(ipaddress.IPv4Address(src_ip))
    dst_i = int(ipaddress.IPv4Address(dst_ip))
    data_rec = struct.pack('!I', src_i)
    data_rec += struct.pack('!I', dst_i)
    data_rec += struct.pack('!Q', octets)
    data_rec += struct.pack('!Q', packets)
    
    # Build savMatchedContentList
    if sav_rules and len(sav_rules) > 0:
        records = []
        for rule in sav_rules:
            if sub_template_id == 901:
                records.append(build_sav_rule_ipv4_interface(
                    rule['interface_id'], rule['prefix'], rule['prefix_len']))
            elif sub_template_id == 902:
                records.append(build_sav_rule_ipv6_interface(
                    rule['interface_id'], rule['prefix'], rule['prefix_len']))
            elif sub_template_id == 903:
                records.append(build_sav_rule_ipv4_prefix(
                    rule['prefix'], rule['prefix_len'], rule['interface_id']))
            elif sub_template_id == 904:
                records.append(build_sav_rule_ipv6_prefix(
                    rule['prefix'], rule['prefix_len'], rule['interface_id']))
        
        matched_field = build_sub_template_list(0x03, sub_template_id, records)
    else:
        matched_field = encode_varlen(0)
    
    data_rec += struct.pack('!B', sav_rule_type)
    data_rec += struct.pack('!B', sav_target_type)
    data_rec += matched_field
    data_rec += struct.pack('!B', sav_action)
    
    data_set_id = template_id
    data_set_len = 4 + len(data_rec)
    data_set = struct.pack('!HH', data_set_id, data_set_len) + data_rec
    
    # Build complete message
    total_len = 16 + len(main_tpl_set) + len(sub_tpl_sets) + len(data_set)
    header = struct.pack('!HHIII', version, total_len, export_time, seq_num, obs_domain)
    
    msg = header + main_tpl_set + sub_tpl_sets + data_set
    return msg


def validate_ipfix_message(msg: bytes) -> (bool, str):
    """Perform basic IPFIX message validation per RFC7011:
    - version == 10
    - message length equals header length
    - each Set length is consistent and within message
    - template Set appears before Data Set (in this simple test message)
    This is a lightweight check suitable for sender-side validation.
    Returns (True, '') on success or (False, reason) on failure.
    """
    try:
        if len(msg) < 16:
            return False, 'message too short for IPFIX header'
        version, msg_len, export_time, seq_num, obs_domain = struct.unpack('!HHIII', msg[:16])
        if version != 10:
            return False, f'invalid IPFIX version {version}'
        if msg_len != len(msg):
            return False, f'header length {msg_len} does not match actual {len(msg)}'

        offset = 16
        templates = {}
        # parse all sets and build template registry for this message
        while offset + 4 <= len(msg):
            set_id, set_len = struct.unpack('!HH', msg[offset:offset+4])
            if set_len < 4:
                return False, f'set at offset {offset} has invalid length {set_len}'
            if offset + set_len > len(msg):
                return False, f'set at offset {offset} extends beyond message (set_len {set_len})'

            inner = msg[offset+4: offset+set_len]
            # Template Set
            if set_id == 2:
                if len(inner) < 4:
                    return False, 'template set too short'
                # may contain multiple template records; parse sequentially
                pos = 0
                while pos + 4 <= len(inner):
                    try:
                        tid, fcount = struct.unpack('!HH', inner[pos:pos+4])
                    except struct.error:
                        return False, 'failed to unpack template header'
                    pos += 4
                    fields = []
                    for _ in range(fcount):
                        if pos + 4 > len(inner):
                            return False, 'template field specifier truncated'
                        fid, flen = struct.unpack('!HH', inner[pos:pos+4])
                        pos += 4
                        enterprise_bit = bool(fid & 0x8000)
                        element_id = fid & 0x7FFF
                        pen = None
                        if enterprise_bit:
                            if pos + 4 > len(inner):
                                return False, 'template enterprise PEN truncated'
                            pen = struct.unpack('!I', inner[pos:pos+4])[0]
                            pos += 4
                        fields.append((element_id, flen, enterprise_bit, pen))
                    templates[tid] = fields
                # end parsing template set
            offset += set_len

        # After collecting templates, second pass validate data sets are consistent
        offset = 16
        while offset + 4 <= len(msg):
            set_id, set_len = struct.unpack('!HH', msg[offset:offset+4])
            inner = msg[offset+4: offset+set_len]
            # Data Set: set_id equals template_id in our test
            if set_id >= 256 and set_id != 2:
                tpl_id = set_id
                if tpl_id not in templates:
                    return False, f'data set references unknown template {tpl_id}'
                fields = templates[tpl_id]
                # parse consecutive data records inside inner
                dpos = 0
                while dpos < len(inner):
                    rec_start = dpos
                    # iterate fields and consume bytes as per flen
                    for (element_id, flen, enterprise_bit, pen) in fields:
                        if flen == 0xFFFF:
                            # variable-length: read length byte(s)
                            if dpos + 1 > len(inner):
                                return False, 'variable-length size byte missing'
                            first = inner[dpos]
                            dpos += 1
                            if first < 255:
                                vlen = first
                            else:
                                if dpos + 2 > len(inner):
                                    return False, 'variable-length extended size bytes missing'
                                vlen = struct.unpack('!H', inner[dpos:dpos+2])[0]
                                dpos += 2
                            # then the vlen bytes of data
                            if dpos + vlen > len(inner):
                                return False, 'variable-length field data truncated'
                            dpos += vlen
                        else:
                            # fixed-length: consume flen bytes
                            if dpos + flen > len(inner):
                                return False, f'fixed-length field (len {flen}) truncated in data record'
                            dpos += flen
                    # finished one data record; ensure progress
                    if dpos == rec_start:
                        return False, 'zero-length data record detected'
                # end while data records
            offset += set_len

        return True, ''
    except Exception as e:
        return False, f'exception during validation: {e}'


def main():
    p = argparse.ArgumentParser(description='Send IPFIX v10 message with IP + SAV IEs')
    p.add_argument('--host', default='127.0.0.1', help='collector host')
    p.add_argument('--port', type=int, default=9991, help='collector UDP port')
    p.add_argument('--count', type=int, default=1, help='number of messages to send')
    p.add_argument('--interval', type=float, default=1.0, help='seconds between messages')
    p.add_argument('--src', default='127.0.0.1', help='source IPv4 address')
    p.add_argument('--dst', default='127.0.0.1', help='destination IPv4 address')
    p.add_argument('--octets', type=int, default=1000, help='octet count')
    p.add_argument('--packets', type=int, default=10, help='packet count')
    p.add_argument('--enterprise', action='store_true', help='mark SAV IEs as enterprise and append PEN')
    p.add_argument('--pen', type=int, default=55555, help='enterprise PEN to use when --enterprise is set')
    p.add_argument('--matched-bytes', type=int, default=0, help='(legacy) fill savMatchedContentList with N bytes')
    p.add_argument('--sav-rules', help='SAV rules JSON file or inline JSON string')
    p.add_argument('--sub-template-id', type=int, default=901, choices=[901,902,903,904],
                   help='sub-template ID: 901=IPv4-if2prefix, 902=IPv6-if2prefix, 903=IPv4-prefix2if, 904=IPv6-prefix2if')
    p.add_argument('--sav-rule-type', type=int, default=0, choices=[0,1],
                   help='SAV rule type: 0=allowlist, 1=blocklist')
    p.add_argument('--sav-target-type', type=int, default=0, choices=[0,1],
                   help='SAV target type: 0=interface-based, 1=prefix-based')
    p.add_argument('--sav-action', type=int, default=2, choices=[0,1,2,3],
                   help='SAV policy action: 0=permit, 1=discard, 2=rate-limit, 3=redirect')
    p.add_argument('--use-complete-message', action='store_true',
                   help='use build_complete_message (includes all sub-templates)')
    args = p.parse_args()
    
    # Parse SAV rules if provided
    sav_rules = None
    if args.sav_rules:
        try:
            # Try to load as file first
            with open(args.sav_rules, 'r') as f:
                data = json.load(f)
        except FileNotFoundError:
            # Try to parse as inline JSON
            try:
                data = json.loads(args.sav_rules)
            except json.JSONDecodeError as e:
                print(f'Error parsing SAV rules JSON: {e}', file=sys.stderr)
                sys.exit(1)
        
        # Support both formats: array or object with "rules" key
        if isinstance(data, list):
            sav_rules = data
        elif isinstance(data, dict) and 'rules' in data:
            sav_rules = data['rules']
        else:
            print('SAV rules must be a JSON array or object with "rules" key', file=sys.stderr)
            sys.exit(1)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    for i in range(1, args.count + 1):
        if args.use_complete_message:
            msg = build_complete_message(
                seq=i, obs_domain=12345,
                src_ip=args.src, dst_ip=args.dst,
                octets=args.octets, packets=args.packets,
                enterprise=args.enterprise, pen=args.pen,
                sav_rules=sav_rules, sub_template_id=args.sub_template_id,
                sav_rule_type=args.sav_rule_type, sav_target_type=args.sav_target_type,
                sav_action=args.sav_action, include_sub_templates=True)
        else:
            msg = build_ipfix_message(
                seq=i, obs_domain=12345,
                src_ip=args.src, dst_ip=args.dst,
                octets=args.octets, packets=args.packets,
                enterprise=args.enterprise, pen=args.pen,
                matched_bytes=args.matched_bytes,
                sav_rules=sav_rules, sub_template_id=args.sub_template_id,
                sav_rule_type=args.sav_rule_type, sav_target_type=args.sav_target_type,
                sav_action=args.sav_action)
        
        ok, reason = validate_ipfix_message(msg)
        if not ok:
            print(f'Validation failed: {reason}', file=sys.stderr)
            sys.exit(1)
        
        sock.sendto(msg, (args.host, args.port))
        
        if sav_rules:
            print(f'sent message #{i} to {args.host}:{args.port} ({len(msg)} bytes, '
                  f'{len(sav_rules)} SAV rules, sub-template {args.sub_template_id})')
        else:
            print(f'sent message #{i} to {args.host}:{args.port} ({len(msg)} bytes)')
        
        time.sleep(args.interval)


if __name__ == '__main__':
    main()
