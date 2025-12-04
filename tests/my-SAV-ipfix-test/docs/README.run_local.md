# SAV IPFIX 本地测试指南

## 概述

这个测试验证pmacct对SAV (Source Address Validation) IPFIX Information Elements的支持，基于`draft-cao-opsawg-ipfix-sav-01`规范。

**无需Docker**: 所有测试都在本地直接运行，不需要Docker环境。

## 前提条件

1. pmacct已编译完成（特别是nfacctd）
2. Python 3环境
3. 具有创建日志目录的权限

## 快速开始

### 1. 准备环境

```bash
# 创建必要的目录
sudo mkdir -p /var/log/pmacct /var/run
sudo chmod 777 /var/log/pmacct /var/run

# 进入测试目录
cd /workspaces/pmacct/tests/my-SAV-ipfix-test
```

### 2. 启动nfacctd

```bash
# 启动nfacctd收集器（后台运行）
/workspaces/pmacct/src/nfacctd -f nfacctd-00.conf &

# 检查进程状态
ps aux | grep nfacctd

# 检查端口监听
netstat -tuln | grep 9991
# 或使用 ss -tuln | grep 9991
```

### 3. 运行测试

```bash
# 运行SAV IPFIX完整测试
python3 test_sav_ipfix.py

# 或运行简单测试
python3 send_ipfix.py --count 3
```

### 4. 查看结果

```bash
# 查看nfacctd日志（实时）
tail -f /var/log/pmacct/nfacctd.log

# 查看最近的日志条目
tail -50 /var/log/pmacct/nfacctd.log | grep -E "template|30001|30002|30003|30004"
```

### 5. 停止测试

```bash
# 停止nfacctd
killall nfacctd
```

## 测试脚本说明

### test_sav_ipfix.py
完整的SAV IPFIX测试脚本，包含：
- 符合draft规范的IPFIX消息构建
- SAV IEs的正确编码
- 两个测试场景（allowlist和blocklist）
- 自动化验证和结果输出

**测试场景**:
1. IPv4 interface-based allowlist非匹配（rate-limit）
2. IPv6 prefix-based blocklist匹配（discard）

### send_ipfix.py
简单的IPFIX发送器，用于快速测试：
```bash
python3 send_ipfix.py --host 127.0.0.1 --port 9991 --count 5 --interval 1.0
```

参数：
- `--host`: 收集器地址（默认127.0.0.1）
- `--port`: 收集器端口（默认9991）
- `--count`: 发送消息数量
- `--interval`: 消息间隔（秒）

## 验证要点

### ✅ 模板识别
日志应显示：
```
NfV10 template ID   : 400
| 0          | 324                [324  ] |      0 |      8 |  # observationTimeMicrosec
| 0          | 30001              [30001] |      8 |      1 |  # savRuleType
| 0          | 30002              [30002] |      9 |      1 |  # savTargetType
| 0          | 30003              [30003] |     10 |  65535 |  # savMatchedContentList
| 0          | 30004              [30004] |    tbd |      1 |  # savPolicyAction
```

### ✅ 数据处理
日志应显示：
```
Processing NetFlow/IPFIX flowset [400] from [127.0.0.1:xxxxx]
```

## SAV Information Elements (基于draft)

| IE ID | 名称 | 类型 | 值域 | 描述 |
|-------|------|------|------|------|
| 30001 | savRuleType | unsigned8 | 0-1 | 0=allowlist, 1=blocklist |
| 30002 | savTargetType | unsigned8 | 0-1 | 0=interface-based, 1=prefix-based |
| 30003 | savMatchedContentList | subTemplateList | 可变 | SAV规则内容列表 |
| 30004 | savPolicyAction | unsigned8 | 0-3 | 0=permit, 1=discard, 2=rate-limit, 3=redirect |

## 故障排除

### 问题：端口已被占用
```bash
# 查找占用端口的进程
lsof -i :9991
# 或
ss -tulpn | grep 9991

# 终止进程
kill <PID>
```

### 问题：权限错误
```bash
# 确保日志目录权限
sudo chmod 777 /var/log/pmacct /var/run

# 或以sudo运行nfacctd
sudo /workspaces/pmacct/src/nfacctd -f nfacctd-00.conf
```

### 问题：没有输出
```bash
# 检查nfacctd是否正在运行
ps aux | grep nfacctd

# 检查配置文件
cat nfacctd-00.conf

# 增加调试级别（已在配置中启用debug: true）
```

## 配置文件说明

`nfacctd-00.conf`关键配置：
```
nfacctd_ip: 0.0.0.0          # 监听所有接口
nfacctd_port: 9991           # IPFIX监听端口
debug: true                   # 启用详细日志
plugins: print                # 使用print插件
print_output: json            # JSON格式输出
print_refresh_time: 1         # 1秒刷新间隔
```

## 下一步

1. **查看详细报告**: `SAV_IPFIX_VALIDATION_REPORT.md`
2. **实现primitives映射**: 创建`sav_primitives.lst`
3. **实现字段提取**: 修改nfacctd源代码以提取SAV字段值
4. **验证JSON输出**: 确认SAV字段出现在print插件输出中

## 参考资料

- Draft规范: `draft-cao-opsawg-ipfix-sav-01`
- IPFIX RFC: RFC 7011, RFC 7012
- subTemplateList: RFC 6313
- pmacct文档: http://www.pmacct.net/

---
**创建日期**: 2025-12-04  
**版本**: 1.0
