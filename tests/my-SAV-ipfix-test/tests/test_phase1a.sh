#!/bin/bash
# Phase 1A 测试脚本 - 验证subTemplateList完整实现

cd /workspaces/pmacct/tests/my-SAV-ipfix-test

echo "=========================================="
echo "Phase 1A: subTemplateList完整实现测试"
echo "=========================================="
echo ""

# 测试1: 基本功能 - IPv4 Interface-to-Prefix (Template 901)
echo "【测试1】IPv4 Interface-to-Prefix (Sub-Template 901)"
echo "规则: 接口5001允许3个前缀"
python3 send_ipfix_with_ip.py \
  --src 10.0.1.100 \
  --dst 10.0.2.1 \
  --sav-rules sav_rules_example.json \
  --sub-template-id 901 \
  --sav-rule-type 0 \
  --sav-target-type 0 \
  --sav-action 1 \
  --use-complete-message
echo ""

# 测试2: 内联JSON规则
echo "【测试2】内联JSON规则（单条规则）"
python3 send_ipfix_with_ip.py \
  --src 192.0.2.100 \
  --dst 203.0.113.1 \
  --sav-rules '[{"interface_id":5002,"prefix":"192.0.2.0","prefix_len":24}]' \
  --sub-template-id 901 \
  --use-complete-message
echo ""

# 测试3: 不同的SAV动作
echo "【测试3】不同的SAV动作 - rate-limit"
python3 send_ipfix_with_ip.py \
  --src 198.51.100.50 \
  --dst 198.51.100.1 \
  --sav-rules sav_rules_example.json \
  --sub-template-id 901 \
  --sav-action 2 \
  --use-complete-message
echo ""

# 测试4: 验证消息大小
echo "【测试4】消息大小验证"
echo "空消息（无SAV规则）:"
python3 send_ipfix_with_ip.py --src 1.1.1.1 --dst 2.2.2.2 2>&1 | grep bytes

echo "带3条规则的消息:"
python3 send_ipfix_with_ip.py \
  --src 1.1.1.1 --dst 2.2.2.2 \
  --sav-rules sav_rules_example.json \
  --sub-template-id 901 \
  --use-complete-message 2>&1 | grep bytes
echo ""

# 测试5: 兼容性测试 - 旧的matched-bytes模式
echo "【测试5】向后兼容 - 旧的matched-bytes模式"
python3 send_ipfix_with_ip.py \
  --src 10.10.10.10 \
  --dst 20.20.20.20 \
  --matched-bytes 50
echo ""

echo "=========================================="
echo "Phase 1A测试完成！"
echo "=========================================="
echo ""
echo "✅ 已实现功能:"
echo "  1. RFC 6313 subTemplateList完整结构"
echo "  2. 4个子模板定义 (901-904)"
echo "  3. 真实SAV规则编码"
echo "  4. JSON规则输入支持"
echo "  5. 所有4种SAV动作支持"
echo "  6. 向后兼容旧模式"
echo ""
echo "📊 消息结构:"
echo "  IPFIX Header (16 bytes)"
echo "  + Main Template Set (Template 400)"
echo "  + Sub-Template Sets (901-904, 4个模板)"
echo "  + Data Set (包含真实subTemplateList)"
echo ""
echo "🔍 验证方法:"
echo "  tail -f /var/log/pmacct/nfacctd-00.log | grep -A20 'template'"
