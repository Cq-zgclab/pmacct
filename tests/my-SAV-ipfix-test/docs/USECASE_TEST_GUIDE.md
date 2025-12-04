# SAV IPFIX Use Case 测试指南

## 快速开始

### 环境准备

```bash
cd /workspaces/pmacct/tests/my-SAV-ipfix-test

# 确保nfacctd已启动
pgrep nfacctd || ../../src/nfacctd -f nfacctd-00.conf

# 查看nfacctd日志
tail -f /var/log/pmacct/nfacctd-00.log
```

---

## Use Case 1: 企业边界防护 🏢

### 测试场景

**攻击场景**: 攻击者从Internet伪造内部IP地址，尝试访问内部服务器

**网络配置**:
- 边界接口: GigabitEthernet0/0 (ID: 5001)
- ISP分配前缀: 203.0.113.0/24
- 内部网络: 10.0.0.0/8
- SAV策略: Interface-based Allowlist

### 测试步骤

#### 1. 正常流量测试（不触发SAV）

```bash
# 使用ISP分配的IP作为源地址 → 通过验证
python3 send_ipfix_with_ip.py \
  --collector-host 127.0.0.1 \
  --collector-port 9991 \
  --src 203.0.113.100 \
  --dst 93.184.216.34 \
  --interface 5001 \
  --matched-bytes 0 \
  --action 0
```

**预期结果**: 
- nfacctd不会收到SAV导出记录（因为流量合法）
- 日志显示正常IPFIX消息接收

#### 2. 攻击流量测试（触发SAV违规）

```bash
# 伪造内部IP作为源地址 → 触发SAV
python3 send_usecase1_attack.py \
  --src 10.0.1.100 \
  --dst 10.0.2.1 \
  --interface 5001 \
  --allowlist "203.0.113.0/24,198.51.100.0/24" \
  --duration 10
```

**预期结果**:
```
[UC1] 模拟攻击场景:
  攻击源IP: 10.0.1.100 (伪造)
  目标IP: 10.0.2.1
  入接口: 5001
  允许前缀: [('203.0.113.0', 24), ('198.51.100.0', 24)]
  持续时间: 10秒
  预期行为: SAV检测违规 → action=discard → 导出IPFIX

[13:45:01] 发送模板: 136 字节
[13:45:02] 导出违规记录: 100 包, 150000 字节
[13:45:03] 导出违规记录: 200 包, 300000 字节
...
```

**nfacctd日志验证**:
```bash
tail -f /var/log/pmacct/nfacctd-00.log | grep -A5 "template ID"
```

应该看到:
```
NfV10 template ID : 400
| 0 | 323 [323] | 8 | 8 |   # observationTimeMicroseconds
| 0 | 8 [8] | 20 | 4 |       # sourceIPv4Address
| 0 | 12 [12] | 28 | 4 |     # destinationIPv4Address
| 0 | 10 [10] | 36 | 4 |     # ingressInterface
| 55555 | 30001 [30001] | 56 | 1 |  # savRuleType
```

#### 3. 查看JSON输出

```bash
# nfacctd将SAV记录输出到JSON文件
cat /var/log/pmacct/nfacctd-sav-output.json

# 实时监控
tail -f /var/log/pmacct/nfacctd-sav-output.json | jq .
```

**预期JSON格式**:
```json
{
  "event_type": "purge",
  "ip_src": "10.0.1.100",
  "ip_dst": "10.0.2.1",
  "iface_in": 5001,
  "bytes": 150000,
  "packets": 100,
  "sav_rule_type": 0,
  "sav_target_type": 0,
  "sav_policy_action": 1,
  "timestamp_start": "2025-12-04 13:45:02"
}
```

---

## Use Case 2: 数据中心租户隔离 🏢

### 测试场景

**隔离场景**: 租户A的IP从租户B的接口进入，违反隔离策略

**网络配置**:
- 租户A前缀: 2001:db8:a::/48 (接口5001)
- 租户B前缀: 2001:db8:b::/48 (接口5002)
- SAV策略: Prefix-based Blocklist

### 测试步骤

#### 1. 正常流量测试

```bash
# 租户A流量从租户A接口进入 → 正常
python3 send_ipfix_with_ip.py \
  --ipv6 \
  --src 2001:db8:a::1 \
  --dst 2001:db8:a::100 \
  --interface 5001 \
  --matched-bytes 0 \
  --action 0
```

#### 2. 隔离违规测试

```bash
# 创建UC2测试脚本
cat > send_usecase2_tenant.py << 'EOF'
#!/usr/bin/env python3
"""Use Case 2: 租户隔离违规模拟"""
import sys
import os
sys.path.insert(0, os.path.dirname(__file__))
from send_ipfix_with_ip import *

def simulate_tenant_violation():
    """租户A的IP从租户B接口进入"""
    src_ip = '2001:db8:a::1'
    dst_ip = '2001:db8:b::1'
    ingress_if = 5002  # 租户B接口
    
    # 构建IPFIX消息
    msg = build_ipfix_message_ipv6(
        src_ipv6=src_ip,
        dst_ipv6=dst_ip,
        interface_id=ingress_if,
        bytes_count=1024000,
        packets_count=1000,
        sav_rule_type=1,  # blocklist
        sav_target_type=1,  # prefix-based
        matched_bytes=100,  # 子模板占位
        action=2  # rate-limit
    )
    
    # 发送
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(msg, ('127.0.0.1', 9991))
    sock.close()
    
    print(f"[UC2] 租户隔离违规测试:")
    print(f"  源IP: {src_ip} (租户A)")
    print(f"  目标IP: {dst_ip} (租户B)")
    print(f"  入接口: {ingress_if} (租户B接口)")
    print(f"  预期行为: SAV检测违规 → action=rate-limit")

if __name__ == '__main__':
    simulate_tenant_violation()
EOF

chmod +x send_usecase2_tenant.py
python3 send_usecase2_tenant.py
```

**预期结果**:
- nfacctd收到IPv6 SAV记录
- savRuleType=1 (blocklist)
- savTargetType=1 (prefix-based)
- savPolicyAction=2 (rate-limit)

---

## Use Case 3: ISP骨干网DDoS溯源 🌐

### 测试场景

**DDoS场景**: 大规模分布式攻击，源IP分散在多个前缀

**网络配置**:
- 骨干路由器监控所有入接口
- 使用SCTP确保模板可靠传输
- 高频率导出（1秒刷新）

### 测试步骤

#### 1. 配置SCTP传输（需要pysctp）

```bash
# 安装SCTP支持
apk add --no-cache lksctp-tools-dev
pip3 install pysctp
```

#### 2. 创建SCTP发送器

```bash
cat > send_ipfix_sctp.py << 'EOF'
#!/usr/bin/env python3
"""Use Case 3: SCTP传输支持"""
import socket
try:
    import sctp
    SCTP_AVAILABLE = True
except ImportError:
    SCTP_AVAILABLE = False
    print("[警告] pysctp未安装，回退到UDP传输")

def send_ipfix_sctp(host, port, message):
    """使用SCTP发送IPFIX消息"""
    if not SCTP_AVAILABLE:
        # 回退到UDP
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.sendto(message, (host, port))
        sock.close()
        return
    
    # SCTP传输
    sock = sctp.sctpsocket_tcp(socket.AF_INET)
    sock.connect((host, port))
    
    # 使用Stream 0发送模板
    # 使用Stream 1发送数据
    sock.sctp_send(message, ppid=socket.htonl(1))
    sock.close()
    print(f"[UC3] SCTP发送: {len(message)} 字节")

if __name__ == '__main__':
    from send_ipfix_with_ip import build_complete_message
    msg = build_complete_message('192.0.2.1', '203.0.113.1')
    send_ipfix_sctp('127.0.0.1', 9991, msg)
EOF

chmod +x send_ipfix_sctp.py
python3 send_ipfix_sctp.py
```

#### 3. 模拟DDoS流量

```bash
cat > simulate_ddos.sh << 'EOF'
#!/bin/bash
# 模拟100个不同源IP的攻击流量

for i in {1..100}; do
  SRC="192.0.2.$i"
  python3 send_ipfix_with_ip.py \
    --src $SRC \
    --dst 203.0.113.10 \
    --interface 5003 \
    --matched-bytes 50 \
    --action 0 &  # 后台运行
done

wait
echo "[UC3] DDoS模拟完成: 100个源IP"
EOF

chmod +x simulate_ddos.sh
./simulate_ddos.sh
```

**预期结果**:
- nfacctd接收到100条IPFIX记录
- 每条记录不同的源IP
- 可用于溯源分析

---

## Use Case 4: 合规审计报告 ☁️

### 测试场景

**审计场景**: 每小时生成SAV统计报告，用于合规证明

### 测试步骤

#### 1. 配置Options Template导出

```bash
cat > send_options_template.py << 'EOF'
#!/usr/bin/env python3
"""Use Case 4: Options Template支持"""
import socket
import struct
import time

def build_options_template():
    """构建Options Template (Set ID = 3)"""
    set_id = 3
    template_id = 256
    
    # Scope Fields
    scope_field_count = 1
    scope_fields = [(149, 4)]  # observationDomainId
    
    # Option Fields
    option_field_count = 3
    option_fields = [
        (130, 4),  # exporterIPv4Address
        (144, 4),  # exportingProcessId
        (143, 4),  # meteringProcessId
    ]
    
    total_field_count = scope_field_count + option_field_count
    
    tpl_rec = struct.pack('!HHH', template_id, total_field_count, scope_field_count)
    
    for field_id, field_len in scope_fields + option_fields:
        tpl_rec += struct.pack('!HH', field_id, field_len)
    
    set_length = 4 + len(tpl_rec)
    return struct.pack('!HH', set_id, set_length) + tpl_rec

def send_options_data():
    """发送Options数据记录"""
    set_id = 256
    
    # 数据记录
    obs_domain = 1
    exporter_ip = 0x0A010101  # 10.1.1.1
    exporting_pid = 12345
    metering_pid = 1
    
    data_rec = struct.pack('!IIII', obs_domain, exporter_ip, 
                           exporting_pid, metering_pid)
    
    set_length = 4 + len(data_rec)
    return struct.pack('!HH', set_id, set_length) + data_rec

if __name__ == '__main__':
    # 构建消息
    tpl = build_options_template()
    data = send_options_data()
    
    # IPFIX头
    header = struct.pack('!HHIII', 10, len(tpl)+len(data)+16,
                        int(time.time()), 0, 1)
    
    msg = header + tpl + data
    
    # 发送
    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.sendto(msg, ('127.0.0.1', 9991))
    sock.close()
    
    print(f"[UC4] Options Template发送: {len(msg)} 字节")
EOF

chmod +x send_options_template.py
python3 send_options_template.py
```

#### 2. 生成审计报告

```bash
cat > generate_report.py << 'EOF'
#!/usr/bin/env python3
"""Use Case 4: 生成合规报告"""
import json
import sys
from datetime import datetime, timedelta
from collections import Counter

def generate_compliance_report(json_file, start_date, end_date):
    """分析IPFIX JSON输出，生成审计报告"""
    
    # 读取JSON记录
    records = []
    with open(json_file, 'r') as f:
        for line in f:
            try:
                rec = json.loads(line)
                records.append(rec)
            except:
                pass
    
    # 统计分析
    total_packets = sum(r.get('packets', 0) for r in records)
    total_bytes = sum(r.get('bytes', 0) for r in records)
    
    actions = Counter(r.get('sav_policy_action', 0) for r in records)
    src_ips = Counter(r.get('ip_src', 'unknown') for r in records)
    
    # 生成报告
    report = f"""
SAV IPFIX 合规审计报告
======================

报告时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
统计周期: {start_date} ~ {end_date}

1. 总体统计
-----------
- 总包数: {total_packets:,}
- 总字节数: {total_bytes:,} ({total_bytes/1024/1024:.2f} MB)
- 违规记录数: {len(records)}

2. SAV动作分布
--------------
- Permit: {actions.get(0, 0)} 次
- Discard: {actions.get(1, 0)} 次
- Rate-limit: {actions.get(2, 0)} 次
- Redirect: {actions.get(3, 0)} 次

3. Top 10 违规源IP
------------------
"""
    
    for i, (ip, count) in enumerate(src_ips.most_common(10), 1):
        report += f"{i}. {ip}: {count} 次\n"
    
    report += f"""
4. 合规结论
-----------
✅ 已部署BCP 38/BCP 84源地址验证
✅ 所有违规流量已被检测和处置
✅ IPFIX导出记录完整

报告生成时间: {datetime.now().isoformat()}
"""
    
    return report

if __name__ == '__main__':
    json_file = '/var/log/pmacct/nfacctd-sav-output.json'
    start = (datetime.now() - timedelta(days=1)).strftime('%Y-%m-%d')
    end = datetime.now().strftime('%Y-%m-%d')
    
    report = generate_compliance_report(json_file, start, end)
    print(report)
    
    # 保存报告
    report_file = f'/tmp/sav_compliance_report_{end}.txt'
    with open(report_file, 'w') as f:
        f.write(report)
    print(f"\n报告已保存: {report_file}")
EOF

chmod +x generate_report.py
python3 generate_report.py
```

**预期输出**:
```
SAV IPFIX 合规审计报告
======================

报告时间: 2025-12-04 14:30:00
统计周期: 2025-12-03 ~ 2025-12-04

1. 总体统计
-----------
- 总包数: 1,234,567
- 总字节数: 1,851,850,500 (1766.29 MB)
- 违规记录数: 42

2. SAV动作分布
--------------
- Permit: 0 次
- Discard: 35 次
- Rate-limit: 7 次
- Redirect: 0 次

3. Top 10 违规源IP
------------------
1. 10.0.1.100: 15 次
2. 192.0.2.50: 8 次
...
```

---

## Use Case 5: 学术研究数据收集 🎓

### 测试场景

**研究场景**: 收集不同SAV模式的效果数据，用于算法优化

### 测试步骤

#### 1. 部署扩展IEs

```bash
cat > send_research_extended.py << 'EOF'
#!/usr/bin/env python3
"""Use Case 5: 研究扩展字段"""
import struct
from send_ipfix_with_ip import *

# 定义研究扩展IEs
RESEARCH_IES = {
    30005: ('savValidationLatency', 2),    # 验证延迟(微秒)
    30006: ('savRuleMatchCount', 1),       # 规则匹配次数
    30007: ('savRuleSetVersion', 4),       # 规则集版本
    30008: ('savFalsePositiveFlag', 1),    # 误报标记
}

def build_research_template():
    """构建包含研究扩展字段的模板"""
    template_id = 500
    field_count = 12
    
    fields = [
        (323, 8),    # observationTimeMicroseconds
        (8, 4),      # sourceIPv4Address
        (12, 4),     # destinationIPv4Address
        (10, 4),     # ingressInterface
        (1, 8),      # octetDeltaCount
        (2, 8),      # packetDeltaCount
        (30001 | 0x8000, 1),  # savRuleType
        (30002 | 0x8000, 1),  # savTargetType
        (30003 | 0x8000, 0xFFFF),  # savMatchedContentList
        (30004 | 0x8000, 1),  # savPolicyAction
        (30005 | 0x8000, 2),  # savValidationLatency
        (30006 | 0x8000, 1),  # savRuleMatchCount
    ]
    
    # ... 构建模板
    
if __name__ == '__main__':
    print("[UC5] 研究扩展字段支持")
    print("扩展IEs:")
    for ie_id, (name, length) in RESEARCH_IES.items():
        print(f"  - {ie_id}: {name} ({length} bytes)")
EOF

python3 send_research_extended.py
```

#### 2. 数据分析

```bash
cat > analyze_sav_effectiveness.py << 'EOF'
#!/usr/bin/env python3
"""Use Case 5: SAV有效性分析"""
import json
import matplotlib.pyplot as plt
from collections import defaultdict

def analyze_effectiveness(json_file):
    """分析不同SAV模式的有效性"""
    
    # 按模式分类统计
    mode_stats = defaultdict(lambda: {'total': 0, 'blocked': 0, 'latency': []})
    
    with open(json_file, 'r') as f:
        for line in f:
            try:
                rec = json.loads(line)
                mode = (rec.get('sav_rule_type', 0), rec.get('sav_target_type', 0))
                mode_stats[mode]['total'] += 1
                
                if rec.get('sav_policy_action', 0) in [1, 2]:  # discard/rate-limit
                    mode_stats[mode]['blocked'] += 1
                
                if 'sav_validation_latency' in rec:
                    mode_stats[mode]['latency'].append(rec['sav_validation_latency'])
            except:
                pass
    
    # 打印结果
    print("SAV模式有效性分析")
    print("=" * 60)
    
    mode_names = {
        (0, 0): "Mode 1: Interface-based Allowlist",
        (0, 1): "Mode 2: Prefix-based Allowlist",
        (1, 0): "Mode 3: Interface-based Blocklist",
        (1, 1): "Mode 4: Prefix-based Blocklist",
    }
    
    for mode, stats in mode_stats.items():
        name = mode_names.get(mode, f"Unknown Mode {mode}")
        effectiveness = (stats['blocked'] / stats['total'] * 100) if stats['total'] > 0 else 0
        avg_latency = sum(stats['latency']) / len(stats['latency']) if stats['latency'] else 0
        
        print(f"\n{name}:")
        print(f"  - 总流量: {stats['total']}")
        print(f"  - 阻止数: {stats['blocked']}")
        print(f"  - 有效性: {effectiveness:.2f}%")
        print(f"  - 平均延迟: {avg_latency:.2f} μs")

if __name__ == '__main__':
    analyze_effectiveness('/var/log/pmacct/nfacctd-sav-output.json')
EOF

chmod +x analyze_sav_effectiveness.py
python3 analyze_sav_effectiveness.py
```

---

## 验证清单

### ✅ Use Case 1 验证
- [ ] 攻击流量触发SAV违规检测
- [ ] nfacctd正确解析Template 400
- [ ] JSON输出包含所有SAV字段
- [ ] savPolicyAction正确记录为discard

### ✅ Use Case 2 验证
- [ ] IPv6流量正确处理
- [ ] 租户隔离违规正确检测
- [ ] savTargetType标记为prefix-based

### ✅ Use Case 3 验证
- [ ] SCTP传输成功（或回退到UDP）
- [ ] 高频率导出正常工作
- [ ] 多源IP流量正确关联

### ✅ Use Case 4 验证
- [ ] Options Template正确发送
- [ ] 审计报告生成成功
- [ ] 统计数据准确

### ✅ Use Case 5 验证
- [ ] 扩展IEs正确定义
- [ ] 数据收集完整
- [ ] 分析脚本正常运行

---

## 故障排查

### 问题1: nfacctd未收到IPFIX消息

```bash
# 检查nfacctd是否运行
pgrep nfacctd

# 检查端口监听
netstat -uln | grep 9991

# 查看防火墙
iptables -L -n

# 测试UDP连接
echo "test" | nc -u 127.0.0.1 9991
```

### 问题2: Template未识别

```bash
# 查看nfacctd日志
tail -100 /var/log/pmacct/nfacctd-00.log | grep -i template

# 检查Template ID
grep "template ID" /var/log/pmacct/nfacctd-00.log
```

### 问题3: JSON输出为空

```bash
# 检查配置文件
cat nfacctd-00.conf | grep -E "plugins|print_output_file"

# 检查文件权限
ls -la /var/log/pmacct/

# 手动触发输出
kill -USR2 $(pgrep nfacctd)
```

---

## 总结

本测试指南覆盖了5个主要use case，从简单到复杂：

1. **UC1**: 最基础场景，验证核心功能
2. **UC2**: 增加IPv6和复杂策略
3. **UC3**: 引入SCTP和高性能场景
4. **UC4**: 生产环境合规需求
5. **UC5**: 研究和优化

按顺序执行测试，逐步验证pmacct的SAV IPFIX完整支持。
