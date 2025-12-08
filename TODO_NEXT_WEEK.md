# SAV IPFIX - 下周工作计划 [LEGACY - PoC版本]

> ⚠️ **注意**: 此文档为PoC (Proof of Concept) 实现的计划，已被新的RFC 7011规范实现替代。
> 
> **新实现请参考**: `TODO_RFC7011_COMPLIANT.md`
> 
> **PoC价值**: 本实现作为概念验证，解析逻辑和数据结构可复用到新实现中。

**创建日期**: 2025-12-05  
**状态**: PoC完成，架构决策：切换到RFC-compliant库实现  
**原目标**: 协议完善 + 性能优化 + 标准化推进  
**新方向**: 使用libfixbuf重新实现，完全符合RFC 7011

---

## 🎯 本周已完成 (2025-12-05)

- ✅ SAV字段完整解析 (process_sav_fields)
- ✅ 4个子模板验证 (901-904)
- ✅ IPv4/IPv6地址解析
- ✅ 双编码模式支持
- ✅ ext_db IE查找集成
- ✅ 完整日志输出
- ✅ Demo脚本 + 文档

---

## 📋 下周待办 (优先级排序)

### ✅ 已完成

#### 1. TCP传输支持 (~1-2小时) ✅ **2025-12-08完成**
**目标**: 满足RFC 7011 Section 10.2要求

**实现**:
- ✅ send_via_tcp() with 2-byte length prefix
- ✅ send_via_sctp() with pysctp fallback
- ✅ --transport {udp|tcp|sctp} CLI parameter
- ✅ Unified send_message() dispatcher
- ✅ RFC 7011 Section 10.2.1 framing compliance

**测试结果**:
- ✅ UDP: 3 SAV rules parsed (template 901)
- ✅ TCP: Message framing implemented correctly
- ✅ SCTP: Graceful fallback to UDP
- ⚠️ Note: nfacctd UDP-only (standard behavior)

**Commit**: 0a5dcad

---

### 🔴 高优先级 (待做)

**验证命令**:
```bash
python3 send_ipfix_with_ip.py --host 127.0.0.1 --port 9995 \
  --transport tcp --sav-rules data/sav_example.json
```

---

#### 2. JSON输出增强 (~4-6小时) - ✅ **2025-12-08完成 (MVP)**
**目标**: 将SAV规则输出到JSON格式

**实现** (2025-12-08):
- ✅ Direct JSON output to `/tmp/sav_output.json`
- ✅ Format: `{"timestamp":..., "sav_validation_mode":"...", "sav_matched_rules":[...]}`
- ✅ Outputs interface_id and prefix/prefixlen for each rule
- ✅ Supports template 901 (IPv4 Interface-to-Prefix)
- ✅ Supports template 903 (IPv4 Prefix-to-Interface)

**技术挑战与解决**:
- 🔴 **Challenge**: pmacct multi-process architecture (Core + Plugin processes)
  * Core process: Parses IPFIX and SAV data
  * Print Plugin process: Separate PID, cannot access Core's memory
  * IPC via ring buffer requires serialization
- 🟡 **Root Cause**: SAV data not in existing primitive types
  * compose_json_sav_fields() exists but pptrs=NULL in plugin process
  * Global variables don't work across processes
- ✅ **MVP Solution**: Direct file output in Core process (bypasses IPC)
  * Added JSON output in process_sav_fields() after parsing
  * Format matches compose_json_sav_fields() output structure
- 📋 **Future Work**: Implement SAV as primitive for proper IPC integration

**Testing**:
```bash
python3 send_ipfix_with_ip.py --sav-rules data/sav_example.json
cat /tmp/sav_output.json | python3 -m json.tool
```

**Output Example**:
```json
{
  "timestamp": 1765161538,
  "sav_validation_mode": "interface-to-prefix",
  "sav_matched_rules": [
    {"interface_id": 1, "prefix": "192.0.2.0/24"},
    {"interface_id": 2, "prefix": "198.51.100.0/24"},
    {"interface_id": 3, "prefix": "203.0.113.0/24"}
  ]
}
```

**Commit**: 2dc6367

def send_via_sctp(host, port, message):
    sock = sctp.sctpsocket_tcp(socket.AF_INET)
    sock.connect((host, port))
    sock.sctp_send(message)
    sock.close()
```

**依赖安装**:
```bash
pip3 install pysctp
# 或
apt-get install python3-pysctp
```

**测试点**:
- [ ] SCTP库安装验证
- [ ] SCTP连接建立
- [ ] 单流/多流测试
- [ ] nfacctd SCTP监听配置

**nfacctd配置**:
```
nfacctd_ip: 0.0.0.0
nfacctd_port: 9995
nfacctd_allow_file: /tmp/allow.lst
! SCTP配置 (如需要)
```

---

### 🟡 中优先级 (推荐)

#### 3. JSON输出集成 (~4小时)
**目标**: SAV规则在print plugin JSON中显示

**技术路线**:
1. 研究pmacct vlen字段机制
2. 序列化SAV规则到IPC buffer
3. 在print plugin反序列化
4. 集成到JSON输出

**参考代码位置**:
- `src/plugin_hooks.c` - IPC机制
- `src/print_plugin.c` - JSON输出
- `src/network.h` - pkt_vlen_hdr_primitives

**测试验证**:
```bash
tail -f /tmp/nfacct.log | jq '.sav_rules'
```

---

#### 4. 性能测试 (~2小时)
**目标**: 验证高负载场景

**测试场景**:
```bash
# 1000 pps for 60 seconds
python3 send_ipfix_with_ip.py --count 60000 --interval 0.001

# 峰值测试
python3 send_ipfix_with_ip.py --count 10000 --interval 0
```

**监控指标**:
- [ ] CPU使用率 (`top`)
- [ ] 内存占用 (`ps aux`)
- [ ] 消息丢失率 (日志计数)
- [ ] 解析延迟 (timestamp对比)

**优化点**:
- 规则缓存 (避免重复解析)
- 批量处理
- 内存池

---

### 🟢 低优先级 (可选)

#### 5. IPv6传输支持 (~30分钟)
**目标**: 发送器支持IPv6 socket

```python
sock = socket.socket(socket.AF_INET6, socket.SOCK_DGRAM)
sock.sendto(message, (ipv6_host, port))
```

**测试**:
```bash
python3 send_ipfix_with_ip.py --host ::1 --port 9995 --ipv6
```

---

#### 6. Web可视化界面 (~1天)
**目标**: 实时展示SAV规则

**技术栈**:
- Backend: Flask/FastAPI
- Frontend: React/Vue
- WebSocket实时更新

**功能**:
- [ ] 规则列表展示
- [ ] 实时统计
- [ ] 告警展示
- [ ] 规则搜索/过滤

---

## 📚 标准化工作 (长期)

### IETF Implementation Report
**目标**: 向IETF提交实现报告

**内容大纲**:
1. **实现概述**
   - pmacct v1.7.10集成
   - 完整RFC 6313支持
   - 4个子模板验证

2. **测试结果**
   - IPv4/IPv6地址解析
   - 双编码模式验证
   - 传输协议支持

3. **互操作性**
   - 与其他collector对比
   - 边界情况处理

4. **改进建议**
   - draft-cao可能的修改
   - IE定义优化

**提交渠道**:
- IETF邮件列表
- GitHub Issue
- Working Group会议

---

### IANA IE编号申请
**当前**: 30001-30004 (临时占位)  
**目标**: 正式分配编号

**流程**:
1. 准备IE定义文档
2. 提交IANA申请表
3. Expert Review
4. 正式发布

**时间**: 通常3-6个月

---

## 🔧 技术债务

1. **错误处理增强**
   - [ ] 更详细的错误码
   - [ ] 异常恢复机制
   - [ ] 日志级别细化

2. **代码重构**
   - [ ] 函数拆分 (process_sav_fields过长)
   - [ ] 常量提取
   - [ ] 单元测试

3. **文档完善**
   - [ ] API文档生成
   - [ ] 架构图
   - [ ] 故障排查指南

---

## 📝 每日工作流程建议

### Day 1 (周一): TCP支持
- [ ] 09:00-10:00: 设计TCP发送逻辑
- [ ] 10:00-11:30: 实现send_via_tcp
- [ ] 11:30-12:00: 单元测试
- [ ] 14:00-15:00: nfacctd集成测试
- [ ] 15:00-16:00: 文档更新

### Day 2 (周二): SCTP支持
- [ ] 09:00-10:00: pysctp库调研
- [ ] 10:00-12:00: 实现send_via_sctp
- [ ] 14:00-15:30: SCTP测试
- [ ] 15:30-17:00: 问题修复

### Day 3 (周三): JSON集成(上)
- [ ] 09:00-11:00: pmacct vlen机制研究
- [ ] 11:00-12:00: 设计序列化方案
- [ ] 14:00-16:00: IPC buffer修改
- [ ] 16:00-17:00: 编译测试

### Day 4 (周四): JSON集成(下)
- [ ] 09:00-11:00: print plugin修改
- [ ] 11:00-12:00: JSON格式设计
- [ ] 14:00-16:00: 集成测试
- [ ] 16:00-17:00: 调试修复

### Day 5 (周五): 性能测试 + 总结
- [ ] 09:00-11:00: 性能测试脚本
- [ ] 11:00-12:00: 压力测试
- [ ] 14:00-15:00: 结果分析
- [ ] 15:00-17:00: 周报 + 代码提交

---

## 🚀 快速启动命令

### 环境准备
```bash
cd /workspaces/pmacct
git pull
make clean && make
```

### 启动测试环境
```bash
# Terminal 1: 启动nfacctd
./src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd.log 2>&1 &

# Terminal 2: 监控日志
tail -f /tmp/nfacctd.log | grep SAV

# Terminal 3: 发送测试
cd tests/my-SAV-ipfix-test
python3 scripts/send_ipfix_with_ip.py --sav-rules data/sav_example.json
```

### 状态检查
```bash
# 检查进程
ps aux | grep nfacctd

# 检查监听端口
netstat -tuln | grep 9995

# 查看最新规则
tail -20 /tmp/nfacctd.log | grep "SAV: Rule"
```

---

## 📞 联系信息

**项目仓库**: github.com/Cq-zgclab/pmacct  
**当前分支**: main  
**最后提交**: 2025-12-05 (Hackathon 100% complete)

**下周回来时**:
1. 阅读本文档
2. 查看WORKSTATE.md最新状态
3. 运行quick-start验证环境
4. 选择一个TODO开始工作

---

**祝周末愉快!下周见!** 🎉
