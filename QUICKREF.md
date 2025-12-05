# SAV IPFIX 快速参考

## IE编号 (固定,不可修改!)

| 模式 | savRuleType | savTargetType | savMatchedContent | savPolicyAction |
|------|------------|--------------|------------------|----------------|
| **标准IANA** | 30001 | 30002 | 30003 | 30004 |
| **企业(PEN=0)** | 1 | 2 | 3 | 4 |

## 快速命令

### 启动nfacctd
```bash
cd /workspaces/pmacct
./src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd.log 2>&1 &
```

### 标准IANA模式发送
```bash
cd tests/my-SAV-ipfix-test
python3 scripts/send_ipfix_with_ip.py \
  --host 127.0.0.1 --port 9995 \
  --sav-rules data/sav_example.json
```

### 企业模式发送
```bash
python3 scripts/send_ipfix_with_ip.py \
  --host 127.0.0.1 --port 9995 \
  --sav-rules data/sav_example.json \
  --enterprise --pen 0
```

### 运行完整演示
```bash
cd tests/my-SAV-ipfix-test && ./demo.sh
```

### 查看结果
```bash
# 接收模板数
grep "template ID   : 400" /tmp/nfacctd.log | wc -l

# 调试日志
tail -f /tmp/nfacctd.log | grep -E "(template|SAV|400)"

# 流输出
tail -f /tmp/nfacct.log
```

## 子模板ID

| ID | 名称 | 字段 | 大小 |
|----|------|------|-----|
| 901 | IPv4 Interface→Prefix | interface_id, ipv4_prefix, prefix_len | 9 bytes |
| 902 | IPv6 Interface→Prefix | interface_id, ipv6_prefix, prefix_len | 21 bytes |
| 903 | IPv4 Prefix→Interface | ipv4_prefix, prefix_len, interface_id | 9 bytes |
| 904 | IPv6 Prefix→Interface | ipv6_prefix, prefix_len, interface_id | 21 bytes |

## 文件位置

| 文件 | 路径 |
|------|------|
| 发送器 | `tests/my-SAV-ipfix-test/scripts/send_ipfix_with_ip.py` |
| 演示脚本 | `tests/my-SAV-ipfix-test/demo.sh` |
| 测试数据 | `tests/my-SAV-ipfix-test/data/sav_example.json` |
| C解析器 | `src/sav_parser.c` |
| IE定义 | `include/sav_parser.h` |
| 配置 | `/tmp/nfacctd_test.conf` |
| 日志 | `/tmp/nfacctd.log`, `/tmp/nfacct.log` |

## 故障排除

### nfacctd未运行
```bash
ps aux | grep nfacctd
# 如果没有输出,启动它:
./src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd.log 2>&1 &
```

### 端口已占用
```bash
netstat -tulpn | grep 9995
# 杀死占用进程:
pkill nfacctd
```

### 查看编译错误
```bash
cd /workspaces/pmacct
make 2>&1 | grep -E "(error|Error)"
```

### 清理重建
```bash
make clean
./configure --enable-jansson --enable-l2
make
```

## 性能参数

| 参数 | 值 | 说明 |
|------|----|----|
| Template 400 | 主模板 | 包含4个SAV IEs |
| 消息大小(标准) | 118 bytes | 3条规则,子模板901 |
| 消息大小(企业) | 142 bytes | 带PEN字段,+24 bytes |
| 规则大小 | 9-21 bytes | 取决于IPv4/IPv6 |
| 最大规则数 | 无限制 | 受变长编码限制 |

## 下一步

1. 查看 `HACKATHON_DEMO.md` 了解完整功能
2. 查看 `WORKSTATE.md` 了解项目状态
3. 阅读 `docs/draft-01-20251102.md` 了解SAV规范
