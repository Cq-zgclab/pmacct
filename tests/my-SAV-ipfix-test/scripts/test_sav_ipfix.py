#!/usr/bin/env python3
"""
test_sav_ipfix.py

验证pmacct对SAV IPFIX Information Elements的支持
基于draft-cao-opsawg-ipfix-sav-01规范

测试场景：
1. 发送包含SAV IEs的IPFIX模板和数据记录
2. 验证nfacctd正确接收和解析这些字段
3. 检查输出是否包含预期的SAV信息
"""
import socket
import struct
import time
import json
import subprocess
import os
import signal

# SAV IPFIX Information Elements (使用临时ID进行测试)
# 实际IANA分配的ID将在draft批准后使用
SAV_IE_IDS = {
    'savRuleType': 30001,          # TBD1 in draft
    'savTargetType': 30002,        # TBD2 in draft
    'savMatchedContentList': 30003, # TBD3 in draft (subTemplateList)
    'savPolicyAction': 30004        # TBD4 in draft
}

# SAV规则类型 (draft Section 4.1)
SAV_RULE_TYPES = {
    'allowlist': 0,
    'blocklist': 1
}

# SAV目标类型 (draft Section 4.2)
SAV_TARGET_TYPES = {
    'interface_based': 0,  # Mode 1 & 2
    'prefix_based': 1       # Mode 3 & 4
}

# SAV策略动作 (draft Section 4.4)
SAV_POLICY_ACTIONS = {
    'permit': 0,
    'discard': 1,
    'rate_limit': 2,
    'redirect': 3
}


def build_ipfix_template(template_id=400):
    """
    构建IPFIX模板记录，包含SAV IEs
    对应draft中的Main Template Record示例
    """
    fields = [
        (324, 8),                              # observationTimeMicroseconds
        (SAV_IE_IDS['savRuleType'], 1),       # savRuleType (unsigned8)
        (SAV_IE_IDS['savTargetType'], 1),     # savTargetType (unsigned8)
        (SAV_IE_IDS['savMatchedContentList'], 0xFFFF),  # variable length
        (SAV_IE_IDS['savPolicyAction'], 1)    # savPolicyAction (unsigned8)
    ]
    
    tpl_rec = struct.pack('!HH', template_id, len(fields))
    for field_id, field_len in fields:
        tpl_rec += struct.pack('!HH', field_id & 0xFFFF, field_len & 0xFFFF)
    
    tpl_set_id = 2
    tpl_set_len = 4 + len(tpl_rec)
    tpl_set = struct.pack('!HH', tpl_set_id, tpl_set_len) + tpl_rec
    
    return tpl_set


def build_data_record_allowlist_ipv4():
    """
    构建数据记录：IPv4 allowlist非匹配场景
    对应draft Table 2中的Event 1
    
    场景：192.0.2.100在接口5001上未匹配任何allowlist规则
    """
    obs_time = int(time.time() * 1000000)  # 微秒
    sav_rule = SAV_RULE_TYPES['allowlist']
    sav_target = SAV_TARGET_TYPES['interface_based']
    
    # 简化版：暂时使用空的matched content list
    # 完整实现需要编码subTemplateList (RFC 6313)
    matched_list = b'\x00'  # 长度0
    
    sav_action = SAV_POLICY_ACTIONS['rate_limit']
    
    data_rec = struct.pack('!Q', obs_time)  # 8字节
    data_rec += struct.pack('!B', sav_rule)  # 1字节
    data_rec += struct.pack('!B', sav_target)  # 1字节
    data_rec += matched_list  # 可变长度
    data_rec += struct.pack('!B', sav_action)  # 1字节
    
    return data_rec


def build_data_record_blocklist_ipv6():
    """
    构建数据记录：IPv6 blocklist匹配场景
    对应draft Table 2中的Event 2
    
    场景：2001:db8::1在接口5001上匹配了prefix-based blocklist
    """
    obs_time = int(time.time() * 1000000) + 1
    sav_rule = SAV_RULE_TYPES['blocklist']
    sav_target = SAV_TARGET_TYPES['prefix_based']
    
    # 简化版：暂时使用空的matched content list
    matched_list = b'\x00'
    
    sav_action = SAV_POLICY_ACTIONS['discard']
    
    data_rec = struct.pack('!Q', obs_time)
    data_rec += struct.pack('!B', sav_rule)
    data_rec += struct.pack('!B', sav_target)
    data_rec += matched_list
    data_rec += struct.pack('!B', sav_action)
    
    return data_rec


def build_ipfix_message(template_id=400, seq=1, obs_domain=1234, include_data=True):
    """构建完整的IPFIX消息"""
    version = 10
    export_time = int(time.time())
    
    # 模板集
    tpl_set = build_ipfix_template(template_id)
    
    # 数据集（可选）
    data_set = b''
    if include_data:
        data_rec1 = build_data_record_allowlist_ipv4()
        data_rec2 = build_data_record_blocklist_ipv6()
        
        data_set_id = template_id
        data_set_len = 4 + len(data_rec1) + len(data_rec2)
        data_set = struct.pack('!HH', data_set_id, data_set_len) + data_rec1 + data_rec2
    
    # 消息头
    total_len = 16 + len(tpl_set) + len(data_set)
    header = struct.pack('!HHIII', version, total_len, export_time, seq, obs_domain)
    
    return header + tpl_set + data_set


def send_test_messages(host='127.0.0.1', port=9991):
    """发送测试IPFIX消息"""
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    
    print("=" * 60)
    print("SAV IPFIX 测试 - 基于draft-cao-opsawg-ipfix-sav-01")
    print("=" * 60)
    
    # 消息1: 只发送模板
    print("\n[1] 发送IPFIX模板 (Template ID 400)")
    msg1 = build_ipfix_message(seq=1, include_data=False)
    sock.sendto(msg1, (host, port))
    print(f"    已发送 {len(msg1)} 字节到 {host}:{port}")
    time.sleep(1)
    
    # 消息2: 发送模板+数据
    print("\n[2] 发送IPFIX数据记录")
    print("    - Event 1: IPv4 allowlist非匹配 (interface-based, rate-limit)")
    print("    - Event 2: IPv6 blocklist匹配 (prefix-based, discard)")
    msg2 = build_ipfix_message(seq=2, include_data=True)
    sock.sendto(msg2, (host, port))
    print(f"    已发送 {len(msg2)} 字节到 {host}:{port}")
    
    sock.close()
    print("\n✓ 测试消息发送完成")
    

def check_nfacctd_logs():
    """检查nfacctd日志输出"""
    print("\n" + "=" * 60)
    print("检查nfacctd处理结果")
    print("=" * 60)
    
    log_file = '/var/log/pmacct/nfacctd.log'
    if os.path.exists(log_file):
        print(f"\n查看日志: {log_file}")
        subprocess.run(['tail', '-30', log_file])
    else:
        print(f"日志文件不存在: {log_file}")


def verify_template_parsing():
    """验证模板解析结果"""
    print("\n" + "=" * 60)
    print("验证要点 (基于draft规范)")
    print("=" * 60)
    
    checks = [
        ("✓ Template ID", "应该识别Template ID 400"),
        ("✓ savRuleType (30001)", "Field type 30001, 长度1字节"),
        ("✓ savTargetType (30002)", "Field type 30002, 长度1字节"),
        ("✓ savMatchedContentList (30003)", "Field type 30003, 可变长度(65535)"),
        ("✓ savPolicyAction (30004)", "Field type 30004, 长度1字节"),
        ("", ""),
        ("数据记录应包含", ""),
        ("  - observationTimeMicrosec", "8字节时间戳"),
        ("  - savRuleType=0或1", "allowlist(0)或blocklist(1)"),
        ("  - savTargetType=0或1", "interface(0)或prefix(1)"),
        ("  - savPolicyAction=0-3", "permit/discard/rate-limit/redirect")
    ]
    
    for check, desc in checks:
        print(f"{check:30} {desc}")


def main():
    """主测试流程"""
    print("\n" + "=" * 60)
    print("pmacct SAV IPFIX 功能验证")
    print("draft-cao-opsawg-ipfix-sav-01 实现测试")
    print("=" * 60)
    
    # 发送测试消息
    send_test_messages()
    
    # 等待处理
    time.sleep(3)
    
    # 检查结果
    check_nfacctd_logs()
    
    # 验证要点
    verify_template_parsing()
    
    print("\n" + "=" * 60)
    print("测试完成")
    print("=" * 60)
    print("\n下一步:")
    print("1. 检查上面的日志，确认template被正确解析")
    print("2. 实现primitives.lst映射 (将IE 30001-30004映射到可读名称)")
    print("3. 实现subTemplateList解析 (savMatchedContentList的完整支持)")
    print("4. 添加JSON输出验证")
    print()


if __name__ == '__main__':
    main()
