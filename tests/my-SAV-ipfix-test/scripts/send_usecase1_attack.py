#!/usr/bin/env python3
"""
Use Case 1: 企业边界防护 - 攻击流量模拟器

场景: 攻击者从Internet伪造内部IP地址，尝试绕过边界防护
触发条件: 源IP不在ISP分配的前缀allowlist中
预期行为: SAV导出IPFIX记录，action=discard
"""

import socket
import struct
import time
import argparse
import ipaddress

def build_ipfix_header(seq_num, obs_domain_id):
    """构建IPFIX消息头 (RFC 7011 Section 3.1)"""
    version = 10
    length = 0  # 稍后填充
    export_time = int(time.time())
    sequence_number = seq_num
    observation_domain_id = obs_domain_id
    
    return struct.pack('!HHIII', version, length, export_time, 
                       sequence_number, observation_domain_id)

def build_template_set():
    """构建Template 400 (主模板)"""
    set_id = 2  # Template Set
    template_id = 400
    field_count = 8
    
    # 8个字段定义
    fields = [
        (323, 8),    # observationTimeMicroseconds
        (8, 4),      # sourceIPv4Address
        (12, 4),     # destinationIPv4Address  
        (10, 4),     # ingressInterface
        (1, 8),      # octetDeltaCount
        (2, 8),      # packetDeltaCount
        (30001 | 0x8000, 1),  # savRuleType (enterprise)
        (30002 | 0x8000, 1),  # savTargetType (enterprise)
    ]
    
    tpl_rec = struct.pack('!HH', template_id, field_count)
    
    for field_id, field_len in fields:
        if field_id & 0x8000:  # Enterprise bit
            tpl_rec += struct.pack('!HHI', field_id, field_len, 55555)
        else:
            tpl_rec += struct.pack('!HH', field_id, field_len)
    
    set_length = 4 + len(tpl_rec)
    tpl_set = struct.pack('!HH', set_id, set_length) + tpl_rec
    
    return tpl_set

def build_sub_template_901():
    """构建Sub-Template 901 (Interface-to-Prefix Mapping)"""
    set_id = 2
    template_id = 901
    field_count = 3
    
    fields = [
        (10, 4),   # ingressInterface
        (44, 5),   # sourceIPv4Prefix (4+1 bytes for /xx)
    ]
    
    tpl_rec = struct.pack('!HH', template_id, field_count)
    tpl_rec += struct.pack('!HH', 10, 4)   # ingressInterface
    tpl_rec += struct.pack('!HH', 44, 4)   # sourceIPv4Prefix
    tpl_rec += struct.pack('!HH', 8, 1)    # sourceIPv4PrefixLength
    
    set_length = 4 + len(tpl_rec)
    return struct.pack('!HH', set_id, set_length) + tpl_rec

def encode_varlen(length):
    """RFC 7011可变长度编码"""
    if length < 255:
        return struct.pack('!B', length)
    else:
        return struct.pack('!BH', 255, length)

def build_sub_template_list(interface_id, allowed_prefixes):
    """
    构建subTemplateList (RFC 6313)
    
    参数:
    - interface_id: 入接口ID
    - allowed_prefixes: [(prefix, prefix_len), ...] 允许的前缀列表
    """
    semantic = 0x03  # allOf - 必须匹配所有规则之一
    template_id = 901
    
    # 构建子模板数据记录
    records = b''
    for prefix, prefix_len in allowed_prefixes:
        rec = struct.pack('!I', interface_id)
        rec += struct.pack('!I', int(ipaddress.IPv4Address(prefix)))
        rec += struct.pack('!B', prefix_len)
        records += rec
    
    # subTemplateList结构: semantic(1B) + template_id(2B) + records
    stl_header = struct.pack('!BH', semantic, template_id)
    stl_data = stl_header + records
    
    # 可变长度编码
    return encode_varlen(len(stl_data)) + stl_data

def build_data_set_attack(src_ip, dst_ip, interface_id, allowed_prefixes,
                          bytes_count, packets_count):
    """
    构建攻击流量数据集
    
    场景: 源IP不在allowlist中，触发SAV违规检测
    """
    set_id = 400  # Data Set (使用Template 400)
    
    # 数据记录
    timestamp = int(time.time() * 1000000)  # 微秒
    data_rec = struct.pack('!Q', timestamp)
    data_rec += struct.pack('!I', int(ipaddress.IPv4Address(src_ip)))
    data_rec += struct.pack('!I', int(ipaddress.IPv4Address(dst_ip)))
    data_rec += struct.pack('!I', interface_id)
    data_rec += struct.pack('!Q', bytes_count)
    data_rec += struct.pack('!Q', packets_count)
    data_rec += struct.pack('!B', 0)  # savRuleType: 0=allowlist
    data_rec += struct.pack('!B', 0)  # savTargetType: 0=interface-based
    
    # 注意: 简化版，不包含savMatchedContentList（需要完整subTemplateList实现）
    
    set_length = 4 + len(data_rec)
    return struct.pack('!HH', set_id, set_length) + data_rec

def build_data_set_with_stl(src_ip, dst_ip, interface_id, allowed_prefixes,
                            bytes_count, packets_count, action):
    """构建包含subTemplateList的数据集（完整版）"""
    set_id = 400
    
    timestamp = int(time.time() * 1000000)
    data_rec = struct.pack('!Q', timestamp)
    data_rec += struct.pack('!I', int(ipaddress.IPv4Address(src_ip)))
    data_rec += struct.pack('!I', int(ipaddress.IPv4Address(dst_ip)))
    data_rec += struct.pack('!I', interface_id)
    data_rec += struct.pack('!Q', bytes_count)
    data_rec += struct.pack('!Q', packets_count)
    data_rec += struct.pack('!B', 0)  # savRuleType: 0=allowlist
    data_rec += struct.pack('!B', 0)  # savTargetType: 0=interface-based
    
    # 添加subTemplateList
    stl = build_sub_template_list(interface_id, allowed_prefixes)
    data_rec += stl
    
    # savPolicyAction (需要先定义在模板中)
    data_rec += struct.pack('!B', action)
    
    set_length = 4 + len(data_rec)
    return struct.pack('!HH', set_id, set_length) + data_rec

def simulate_attack(collector_host, collector_port, src_ip, dst_ip,
                   interface_id, allowed_prefixes, duration=10):
    """
    模拟持续攻击流量
    
    参数:
    - duration: 持续时间(秒)
    """
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    seq_num = 0
    
    print(f"[UC1] 模拟攻击场景:")
    print(f"  攻击源IP: {src_ip} (伪造)")
    print(f"  目标IP: {dst_ip}")
    print(f"  入接口: {interface_id}")
    print(f"  允许前缀: {allowed_prefixes}")
    print(f"  持续时间: {duration}秒")
    print(f"  预期行为: SAV检测违规 → action=discard → 导出IPFIX")
    print()
    
    # 首先发送模板（只发一次）
    tpl_set = build_template_set()
    sub_tpl_set = build_sub_template_901()
    
    header = build_ipfix_header(seq_num, 1)
    msg_len = len(header) + len(tpl_set) + len(sub_tpl_set)
    
    # 更新消息长度
    header_with_len = header[:2] + struct.pack('!H', msg_len) + header[4:]
    template_msg = header_with_len + tpl_set + sub_tpl_set
    
    sock.sendto(template_msg, (collector_host, collector_port))
    print(f"[{time.strftime('%H:%M:%S')}] 发送模板: {len(template_msg)} 字节")
    seq_num += 1
    
    time.sleep(1)
    
    # 发送攻击流量数据
    start_time = time.time()
    packet_count = 0
    byte_count = 0
    
    while time.time() - start_time < duration:
        # 模拟每秒100个包，每包1500字节
        packets_per_sec = 100
        bytes_per_packet = 1500
        
        byte_count += packets_per_sec * bytes_per_packet
        packet_count += packets_per_sec
        
        # 构建数据消息
        data_set = build_data_set_attack(src_ip, dst_ip, interface_id,
                                        allowed_prefixes, byte_count, packet_count)
        
        header = build_ipfix_header(seq_num, 1)
        msg_len = len(header) + len(data_set)
        header_with_len = header[:2] + struct.pack('!H', msg_len) + header[4:]
        
        data_msg = header_with_len + data_set
        sock.sendto(data_msg, (collector_host, collector_port))
        
        print(f"[{time.strftime('%H:%M:%S')}] 导出违规记录: "
              f"{packet_count} 包, {byte_count} 字节", end='\r')
        
        seq_num += 1
        time.sleep(1)
    
    print()
    print(f"\n[UC1] 攻击模拟完成:")
    print(f"  总共发送: {seq_num} 条IPFIX消息")
    print(f"  违规流量: {packet_count} 包, {byte_count} 字节")
    
    sock.close()

def main():
    parser = argparse.ArgumentParser(
        description='Use Case 1: 企业边界防护攻击模拟器'
    )
    parser.add_argument('--collector-host', default='127.0.0.1',
                       help='IPFIX收集器地址')
    parser.add_argument('--collector-port', type=int, default=9991,
                       help='IPFIX收集器端口')
    parser.add_argument('--src', required=True,
                       help='伪造的源IP地址 (攻击源)')
    parser.add_argument('--dst', required=True,
                       help='目标IP地址 (内部服务器)')
    parser.add_argument('--interface', type=int, default=5001,
                       help='入接口ID')
    parser.add_argument('--allowlist', required=True,
                       help='允许的前缀 (用逗号分隔), 例如: 203.0.113.0/24,198.51.100.0/24')
    parser.add_argument('--duration', type=int, default=10,
                       help='攻击持续时间(秒)')
    
    args = parser.parse_args()
    
    # 解析allowlist
    allowed_prefixes = []
    for prefix_str in args.allowlist.split(','):
        prefix_str = prefix_str.strip()
        if '/' in prefix_str:
            prefix, prefix_len = prefix_str.split('/')
            allowed_prefixes.append((prefix, int(prefix_len)))
        else:
            allowed_prefixes.append((prefix_str, 32))
    
    simulate_attack(args.collector_host, args.collector_port,
                   args.src, args.dst, args.interface,
                   allowed_prefixes, args.duration)

if __name__ == '__main__':
    main()
