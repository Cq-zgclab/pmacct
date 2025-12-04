#!/usr/bin/env python3
"""
send_ipfix.py

Simple pure-Python IPFIX v10 sender used for SAV IE testing.

It builds a Template Record (Template ID 400) using temporary Element IDs
in the 30001-30004 range (all > 30000 as requested) and a single Data Record
that matches that template. The savMatchedContentList field is encoded as a
variable-length field with zero-length payload in this simple test.

No external Python packages are required.
"""
import argparse
import socket
import struct
import time


def build_ipfix_message(template_id=400, seq=1, obs_domain=1234):
    # IPFIX Header: version(2), length(2), exportTime(4), seq(4), obsDomain(4)
    version = 10

    # Build Template Set (Set ID = 2)
    # Template: Template ID = template_id, Field Count = 4
    # Fields: 30001 (1), 30002 (1), 30003 (variable=0xFFFF), 30004 (1)
    tpl_fields = [ (30001, 1), (30002, 1), (30003, 0xFFFF), (30004, 1) ]

    tpl_rec = struct.pack('!HH', template_id, len(tpl_fields))
    for fid, flen in tpl_fields:
        tpl_rec += struct.pack('!HH', fid & 0xFFFF, flen & 0xFFFF)

    tpl_set_id = 2
    tpl_set_len = 4 + len(tpl_rec)  # set header (4) + template record
    tpl_set = struct.pack('!HH', tpl_set_id, tpl_set_len) + tpl_rec

    # Build Data Set (Set ID = template_id)
    # Data record fields in same order: savRuleType(1), savTargetType(1),
    # savMatchedContentList(variable) encoded as one-byte length (0),
    # savPolicyAction(1)
    sav_rule = 0
    sav_target = 0
    matched_list_bytes = b''  # empty content for the subTemplateList in this simple test
    # variable length fields use a 1-octet length if < 255
    matched_field = struct.pack('!B', len(matched_list_bytes)) + matched_list_bytes
    sav_action = 2

    data_rec = struct.pack('!B', sav_rule) + struct.pack('!B', sav_target) + matched_field + struct.pack('!B', sav_action)

    data_set_id = template_id
    data_set_len = 4 + len(data_rec)
    data_set = struct.pack('!HH', data_set_id, data_set_len) + data_rec

    # Now assemble full message and compute length
    export_time = int(time.time())
    seq_num = seq
    obs_domain = obs_domain

    total_len = 16 + len(tpl_set) + len(data_set)
    header = struct.pack('!HHIII', version, total_len, export_time, seq_num, obs_domain)

    msg = header + tpl_set + data_set
    return msg


def main():
    p = argparse.ArgumentParser(description='Send a simple IPFIX v10 message with temporary SAV IEs')
    p.add_argument('--host', default='127.0.0.1', help='collector host')
    p.add_argument('--port', type=int, default=9991, help='collector UDP port')
    p.add_argument('--count', type=int, default=1, help='number of messages to send')
    p.add_argument('--interval', type=float, default=1.0, help='seconds between messages')
    args = p.parse_args()

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

    for i in range(1, args.count + 1):
        msg = build_ipfix_message(seq=i, obs_domain=12345)
        sock.sendto(msg, (args.host, args.port))
        print(f'sent message #{i} to {args.host}:{args.port} ({len(msg)} bytes)')
        time.sleep(args.interval)


if __name__ == '__main__':
    main()
