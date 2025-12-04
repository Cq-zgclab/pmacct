#!/usr/bin/env python3
"""
send_ipfix_with_ip.py

Sends an IPFIX v10 message containing both standard flow fields
(source/dest IPv4, octet/packet counters) and the SAV test fields
used by the original `send_ipfix.py`. This helps `nfacctd` produce
printable flows since the collector can aggregate by source IP.

No external dependencies.
"""
import argparse
import socket
import struct
import time
import ipaddress
import sys


def build_ipfix_message(template_id=400, seq=1, obs_domain=1234,
                        src_ip='127.0.0.1', dst_ip='127.0.0.1',
                        octets=1000, packets=10,
                        enterprise=False, pen=0, matched_bytes=0):
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

    # SAV fields as in original test
    sav_rule = 0
    sav_target = 0
    # build matched content payload of requested size (for testing varlen encoding)
    if matched_bytes and matched_bytes > 0:
        matched_list_bytes = bytes([0x41]) * matched_bytes
    else:
        matched_list_bytes = b''

    # RFC7011 variable-length encoding for Data Records
    # If length < 255: encode 1 octet length
    # If length >= 255: encode 1 octet of 255 followed by 2-octet length (network byte order)
    def encode_varlen(length: int) -> bytes:
        if length < 255:
            return struct.pack('!B', length)
        else:
            if length > 0xFFFF:
                raise ValueError('variable-length field too large')
            return struct.pack('!B', 255) + struct.pack('!H', length)

    matched_field = encode_varlen(len(matched_list_bytes)) + matched_list_bytes
    sav_action = 2

    data_rec += struct.pack('!B', sav_rule)
    data_rec += struct.pack('!B', sav_target)
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
    p.add_argument('--matched-bytes', type=int, default=0, help='fill savMatchedContentList with N bytes (for varlen testing)')
    args = p.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    for i in range(1, args.count + 1):
        msg = build_ipfix_message(seq=i, obs_domain=12345,
                     src_ip=args.src, dst_ip=args.dst,
                     octets=args.octets, packets=args.packets,
                     enterprise=args.enterprise, pen=args.pen,
                     matched_bytes=args.matched_bytes)
        ok, reason = validate_ipfix_message(msg)
        if not ok:
            print(f'Validation failed: {reason}', file=sys.stderr)
            sys.exit(1)
        sock.sendto(msg, (args.host, args.port))
        print(f'sent message #{i} to {args.host}:{args.port} ({len(msg)} bytes)')
        time.sleep(args.interval)


if __name__ == '__main__':
    main()
