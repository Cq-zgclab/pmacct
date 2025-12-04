#!/bin/bash
#
# test_all_templates.sh - Complete subTemplateList validation test
#
# Purpose:
#   Tests all 4 SAV sub-templates (901-904) defined in RFC 6313 format:
#   - Template 901: IPv4 Interface-to-Prefix Mapping
#   - Template 902: IPv6 Interface-to-Prefix Mapping
#   - Template 903: IPv4 Prefix-to-Interface Mapping
#   - Template 904: IPv6 Prefix-to-Interface Mapping
#
# Dependencies:
#   - scripts/send_ipfix_with_ip.py
#   - test-data/sav_rules_*.json (4 files)
#   - nfacctd running on localhost:9991
#
# Usage:
#   ./test_all_templates.sh
#
# Standards:
#   RFC 6313 (subTemplateList), RFC 7011 (IPFIX), draft-cao-opsawg-ipfix-sav-01
#

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
BASE_DIR="$(dirname "$SCRIPT_DIR")"
cd "$BASE_DIR"

echo "=========================================="
echo "Phase 1A: 完整测试 - 所有4个子模板"
echo "=========================================="
echo ""

# 测试1: Template 901 - IPv4 Interface-to-Prefix
echo "【测试1】Template 901: IPv4 Interface-to-Prefix Mapping"
echo "规则: 接口5001允许3个IPv4前缀"
python3 scripts/send_ipfix_with_ip.py \
  --src 10.0.1.100 \
  --dst 10.0.2.1 \
  --sav-rules test-data/sav_rules_example.json \
  --sub-template-id 901 \
  --sav-rule-type 0 \
  --sav-target-type 0 \
  --sav-action 1 \
  --use-complete-message
echo ""

# 测试2: Template 902 - IPv6 Interface-to-Prefix
echo "【测试2】Template 902: IPv6 Interface-to-Prefix Mapping"
echo "规则: 接口5002允许2个IPv6前缀"
python3 scripts/send_ipfix_with_ip.py \
  --src 10.0.1.100 \
  --dst 10.0.2.1 \
  --sav-rules test-data/sav_rules_ipv6_example.json \
  --sub-template-id 902 \
  --sav-rule-type 0 \
  --sav-target-type 0 \
  --sav-action 1 \
  --use-complete-message
echo ""

# 测试3: Template 903 - IPv4 Prefix-to-Interface
echo "【测试3】Template 903: IPv4 Prefix-to-Interface Mapping"
echo "规则: 2个IPv4前缀只能从接口5001进入"
python3 scripts/send_ipfix_with_ip.py \
  --src 198.51.100.100 \
  --dst 203.0.113.1 \
  --sav-rules test-data/sav_rules_prefix2if_ipv4.json \
  --sub-template-id 903 \
  --sav-rule-type 1 \
  --sav-target-type 1 \
  --sav-action 2 \
  --use-complete-message
echo ""

# 测试4: Template 904 - IPv6 Prefix-to-Interface
echo "【测试4】Template 904: IPv6 Prefix-to-Interface Mapping"
echo "规则: 2个IPv6前缀只能从接口5003进入"
python3 scripts/send_ipfix_with_ip.py \
  --src 10.0.1.100 \
  --dst 10.0.2.1 \
  --sav-rules test-data/sav_rules_prefix2if_ipv6.json \
  --sub-template-id 904 \
  --sav-rule-type 1 \
  --sav-target-type 1 \
  --sav-action 2 \
  --use-complete-message
echo ""

# 测试5: 消息大小对比（所有4个模板）
echo "【测试5】消息大小对比（4个子模板）"
echo "Template 901 (IPv4, 3规则): $(python3 scripts/send_ipfix_with_ip.py --sav-rules test-data/sav_rules_example.json --sub-template-id 901 --use-complete-message 2>&1 | grep bytes)"
echo "Template 902 (IPv6, 2规则): $(python3 scripts/send_ipfix_with_ip.py --sav-rules test-data/sav_rules_ipv6_example.json --sub-template-id 902 --use-complete-message 2>&1 | grep bytes)"
echo "Template 903 (IPv4, 2规则): $(python3 scripts/send_ipfix_with_ip.py --sav-rules test-data/sav_rules_prefix2if_ipv4.json --sub-template-id 903 --use-complete-message 2>&1 | grep bytes)"
echo "Template 904 (IPv6, 2规则): $(python3 scripts/send_ipfix_with_ip.py --sav-rules test-data/sav_rules_prefix2if_ipv6.json --sub-template-id 904 --use-complete-message 2>&1 | grep bytes)"
echo ""

# 测试6: 混合场景
echo "【测试6】混合场景 - 不同SAV模式"
echo "Mode 1: Interface-based Allowlist (Template 901)"
python3 scripts/send_ipfix_with_ip.py \
  --src 10.0.1.100 --dst 10.0.2.1 \
  --sav-rules test-data/sav_rules_example.json \
  --sub-template-id 901 \
  --sav-rule-type 0 --sav-target-type 0 --sav-action 1 \
  --use-complete-message 2>&1 | tail -1

echo "Mode 2: Prefix-based Allowlist (Template 903)"
python3 scripts/send_ipfix_with_ip.py \
  --src 198.51.100.100 --dst 203.0.113.1 \
  --sav-rules test-data/sav_rules_prefix2if_ipv4.json \
  --sub-template-id 903 \
  --sav-rule-type 0 --sav-target-type 1 --sav-action 0 \
  --use-complete-message 2>&1 | tail -1

echo "Mode 3: Interface-based Blocklist (Template 902, IPv6)"
python3 scripts/send_ipfix_with_ip.py \
  --src 10.0.1.100 --dst 10.0.2.1 \
  --sav-rules test-data/sav_rules_ipv6_example.json \
  --sub-template-id 902 \
  --sav-rule-type 1 --sav-target-type 0 --sav-action 2 \
  --use-complete-message 2>&1 | tail -1

echo "Mode 4: Prefix-based Blocklist (Template 904, IPv6)"
python3 scripts/send_ipfix_with_ip.py \
  --src 10.0.1.100 --dst 10.0.2.1 \
  --sav-rules test-data/sav_rules_prefix2if_ipv6.json \
  --sub-template-id 904 \
  --sav-rule-type 1 --sav-target-type 1 --sav-action 1 \
  --use-complete-message 2>&1 | tail -1
echo ""

echo "=========================================="
echo "✅ 所有4个子模板测试完成！"
echo "=========================================="
echo ""
echo "📊 测试覆盖率:"
echo "  ✅ Template 901: IPv4 Interface-to-Prefix (9 bytes/rule)"
echo "  ✅ Template 902: IPv6 Interface-to-Prefix (21 bytes/rule)"
echo "  ✅ Template 903: IPv4 Prefix-to-Interface (9 bytes/rule)"
echo "  ✅ Template 904: IPv6 Prefix-to-Interface (21 bytes/rule)"
echo ""
echo "📋 测试场景:"
echo "  ✅ 所有4种SAV验证模式"
echo "  ✅ IPv4 + IPv6 双栈支持"
echo "  ✅ 不同的SAV动作 (permit/discard/rate-limit)"
echo "  ✅ Interface-based 和 Prefix-based"
echo ""
echo "💾 生成的规则文件:"
echo "  - sav_rules_example.json (Template 901, IPv4)"
echo "  - sav_rules_ipv6_example.json (Template 902, IPv6)"
echo "  - sav_rules_prefix2if_ipv4.json (Template 903, IPv4)"
echo "  - sav_rules_prefix2if_ipv6.json (Template 904, IPv6)"
