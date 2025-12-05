# SAV IPFIX Test Suite

## 概述

基于 pmacct 的 SAV (Source Address Validation) IPFIX 测试套件,**完整实现** RFC 6313 subTemplateList 解析,用于验证 draft-cao-opsawg-ipfix-sav-01。

**核心功能**:
- ✅ RFC 6313 subTemplateList 完整解析
- ✅ SAV规则提取 (interface + prefix + prefix_len)
- ✅ 双编码模式支持 (标准IANA + 企业模式)
- ✅ 日志级别规则展示

## 快速开始

```bash
# 1. 启动 nfacctd
cd /workspaces/pmacct
./src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd.log 2>&1 &

# 2. 运行演示
cd tests/my-SAV-ipfix-test
./demo.sh
```

## 目录结构

```
tests/my-SAV-ipfix-test/
├── README.md                          # 本文档
├── demo.sh                            # 快速演示脚本
├── scripts/
│   └── send_ipfix_with_ip.py         # IPFIX消息发送器 (751行)
├── data/
│   └── sav_example.json              # 简单测试数据 (3条规则)
└── test-data/                        # 高级测试数据集
    ├── sav_rules_example.json        # 完整示例
    ├── sav_rules_ipv6_example.json   # IPv6规则
    ├── sav_rules_prefix2if_ipv4.json # 前缀→接口
    └── sav_rules_prefix2if_ipv6.json # IPv6前缀→接口
```

## 两种编码模式

### 标准 IANA 模式 (默认)
- **IE编号**: 30001-30004
- **命令**: `python3 scripts/send_ipfix_with_ip.py --sav-rules data/sav_example.json`

### 企业模式 (RFC 7013)
- **PEN**: 0, **IE编号**: 1-4
- **命令**: `python3 scripts/send_ipfix_with_ip.py --sav-rules data/sav_example.json --enterprise --pen 0`

## IE编号参考 (固定,不可修改)

| 名称 | 标准IANA | 企业(PEN=0) |
|------|---------|------------|
| savRuleType | 30001 | 1 |
| savTargetType | 30002 | 2 |
| savMatchedContentList | 30003 | 3 |
| savPolicyAction | 30004 | 4 |

## 常用命令

```bash
# 标准IANA模式
python3 scripts/send_ipfix_with_ip.py --host 127.0.0.1 --port 9995 --sav-rules data/sav_example.json

# 企业模式
python3 scripts/send_ipfix_with_ip.py --host 127.0.0.1 --port 9995 --sav-rules data/sav_example.json --enterprise --pen 0

# 压力测试
python3 scripts/send_ipfix_with_ip.py --sav-rules data/sav_example.json --count 100 --interval 0.1

# IPv6规则
python3 scripts/send_ipfix_with_ip.py --sav-rules test-data/sav_rules_ipv6_example.json --sub-template-id 902

# 查看结果
grep "template ID   : 400" /tmp/nfacctd.log | wc -l
tail -f /tmp/nfacctd.log
```

## 子模板

| ID | 名称 | 字段 | 大小 |
|----|------|------|-----|
| 901 | IPv4 Interface→Prefix | interface_id, ipv4_prefix, prefix_len | 9 bytes |
| 902 | IPv6 Interface→Prefix | interface_id, ipv6_prefix, prefix_len | 21 bytes |
| 903 | IPv4 Prefix→Interface | ipv4_prefix, prefix_len, interface_id | 9 bytes |
| 904 | IPv6 Prefix→Interface | ipv6_prefix, prefix_len, interface_id | 21 bytes |

## 参考文档

- [HACKATHON_DEMO.md](../../HACKATHON_DEMO.md) - 完整演示指南
- [QUICKREF.md](../../QUICKREF.md) - 快速参考卡
- [WORKSTATE.md](../../WORKSTATE.md) - 项目状态

---
版本: 1.0 | 更新: 2025-12-05
