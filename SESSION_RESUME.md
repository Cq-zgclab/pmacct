# SAV IPFIX Hackathon - Session 恢复指南

## 📅 上次工作时间
**日期**: 2025-12-04 (December 4, 2025)  
**最后提交**: 5192b4c - "docs: Update WORKSTATE and add TODO_NEXT_WEEK plan"  
**已推送**: ✅ Yes (11 commits pushed to GitHub)

---

## ✅ 已完成工作总结

### 核心功能 (100% 完成)
1. ✅ **完整的SAV字段提取** (`src/nfacctd.c` - process_sav_fields函数)
   - 123行完整实现，替换原3行stub
   - ext_db_get_ie支持双编码模式（标准IANA 30001-30004 + 企业PEN=0/IE 1-4）
   - RFC 7011变长字段解码
   - 调用parse_sav_sub_template_list解析规则

2. ✅ **SAV Parser修复** (`src/sav_parser.c`)
   - parse_sav_sub_template_list返回template_id via输出参数
   - 修复template ID读取bug (34048 → 901)
   - 支持所有4个子模板 (901-904)

3. ✅ **所有模板验证通过**
   - Template 901 (IPv4 if→prefix): 3规则 ✓ (192.0.2.0/24, 198.51.100.0/24, 203.0.113.0/24)
   - Template 902 (IPv6 if→prefix): 2规则 ✓ (2001:db8:a::/48, 2001:db8:b::/48)
   - Template 903 (IPv4 prefix→if): 2规则 ✓ (198.51.100.0/24, 203.0.113.0/24)
   - Template 904 (IPv6 prefix→if): 2规则 ✓ (2001:db8:c::/48, 2001:db8:d::/48)

4. ✅ **文档完整**
   - WORKSTATE.md: 项目状态100%核心完成
   - HACKATHON_DEMO.md: 演示输出样例
   - TODO_NEXT_WEEK.md: 5天详细计划
   - README.md: 更新的功能列表

5. ✅ **测试工具**
   - `tests/my-SAV-ipfix-test/demo.sh`: 自动化测试脚本
   - `scripts/send_ipfix_with_ip.py`: 完整的IPFIX发送器
   - 支持双编码模式切换

---

## 🎯 下一步任务 (TODO_NEXT_WEEK.md)

### Day 1 (Monday) - TCP支持 (~2小时)
**文件**: `tests/my-SAV-ipfix-test/scripts/send_ipfix_with_ip.py`

**任务**:
1. 添加 `send_via_tcp()` 函数
2. TCP需要2字节长度前缀 (RFC 7011 Section 10.2.1)
3. 添加 `--transport tcp` 命令行参数
4. 测试: 发送Template 901消息via TCP

**关键技术点**:
```python
def send_via_tcp(host, port, message):
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.connect((host, port))
    # TCP需要2字节长度前缀
    length_prefix = struct.pack('!H', len(message))
    sock.sendall(length_prefix + message)
    sock.close()
```

### Day 2 (Tuesday) - SCTP支持 (~3小时)
**依赖**: `apk add lksctp-tools-dev && pip3 install pysctp`

**任务**:
1. 检查pysctp可用性
2. 实现 `send_via_sctp()` 函数
3. SCTP多流支持 (Stream 0=模板, Stream 1=数据)
4. 添加 `--transport sctp` 参数

### Day 3-4 (Wed-Thu) - JSON输出 (~8小时)
**挑战**: pmacct IPC限制，vlen字段难以传递

**可能方案**:
- 方案A: 修改IPC buffer支持动态大小vlen
- 方案B: 序列化SAV规则为固定长度字段
- 方案C: 使用自定义primitive存储规则摘要

### Day 5 (Friday) - 性能测试 (~4小时)
**目标**: 1000+ pps稳定处理

**测试脚本**:
```bash
# 压力测试
for i in {1..1000}; do
  python3 send_ipfix_with_ip.py --count 10 &
done
wait
```

---

## 🔧 快速恢复步骤

### 步骤1: 环境验证 (2分钟)

```bash
cd /workspaces/pmacct

# 1.1 检查Git状态
git status
git log --oneline -5

# 1.2 验证远程同步
git fetch origin
git log origin/main..HEAD  # 应该输出空（无未推送commit）

# 1.3 检查关键文件
ls -lh src/sav_parser.c src/nfacctd TODO_NEXT_WEEK.md WORKSTATE.md
```

**预期输出**:
```
On branch main
Your branch is up to date with 'origin/main'.
nothing to commit, working tree clean
```

### 步骤2: 重新编译 (1分钟)

```bash
# 2.1 清理旧文件
make clean

# 2.2 重新编译nfacctd
make src/nfacctd

# 2.3 验证编译
./src/nfacctd -V
# 应该输出: pmacct 1.7.10-git [日期-时间 (commit hash)]
```

### 步骤3: 快速功能测试 (2分钟)

```bash
# 3.1 启动nfacctd (后台)
./src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd.log 2>&1 &
NFACCTD_PID=$!
sleep 2

# 3.2 运行demo脚本
cd tests/my-SAV-ipfix-test
./demo.sh

# 3.3 检查结果
tail -20 /tmp/nfacctd.log | grep "SAV:"

# 3.4 清理
kill $NFACCTD_PID
```

**预期输出** (应该看到3条规则):
```
INFO ( nfacctd_core/core ): SAV: Parsed 3 rule(s) from sub-template 901
INFO ( nfacctd_core/core ): SAV: Rule #1: interface=1 prefix=192.0.2.0/24 mode=0
INFO ( nfacctd_core/core ): SAV: Rule #2: interface=2 prefix=198.51.100.0/24 mode=0
INFO ( nfacctd_core/core ): SAV: Rule #3: interface=3 prefix=203.0.113.0/24 mode=0
```

### 步骤4: 开始新任务 (立即)

✅ **如果上述测试全部通过**, 可以开始Day 1任务:

```bash
cd tests/my-SAV-ipfix-test/scripts
# 编辑 send_ipfix_with_ip.py 添加TCP支持
```

❌ **如果测试失败**, 需要调试:

```bash
# 查看完整日志
cat /tmp/nfacctd.log

# 检查端口占用
netstat -tuln | grep 9995

# 重新编译（强制刷新）
cd /workspaces/pmacct
rm -f src/nfacctd.o src/sav_parser.o src/nfacctd
make src/nfacctd
```

---

## 🐛 常见问题排查

### 问题1: Template ID读取错误 (34048而非901)

**原因**: 编译缓存未刷新

**解决**:
```bash
cd /workspaces/pmacct
rm -f src/nfacctd.o src/sav_parser.o
make src/nfacctd
```

### 问题2: nfacctd启动失败

**检查**:
```bash
# 端口占用
lsof -i :9995
# 或
netstat -tuln | grep 9995

# 终止旧进程
pkill -9 nfacctd
```

### 问题3: 没有SAV输出

**调试**:
```bash
# 启用DEBUG模式
./src/nfacctd -f /tmp/nfacctd_test.conf -d > /tmp/debug.log 2>&1

# 检查template接收
grep "template ID" /tmp/debug.log

# 检查IE字段
grep "30001\|30002\|30003\|30004" /tmp/debug.log
```

### 问题4: IPv4地址显示错误

**症状**: 显示 `2:c0::` 而非 `192.0.2.0`

**原因**: template_id未正确传递给sav_rule_to_string

**已修复**: parse_sav_sub_template_list现在返回template_id via输出参数

---

## 📂 关键文件位置

### 核心代码
```
/workspaces/pmacct/
├── src/
│   ├── nfacctd.c              # process_sav_fields() (lines 1797-1920)
│   ├── sav_parser.c           # parse_sav_sub_template_list() (lines 141-270)
│   └── sav_parser.h           # 函数签名
├── include/
│   └── sav_parser.h           # 头文件
└── tests/my-SAV-ipfix-test/
    ├── demo.sh                # 快速测试脚本 ⭐
    ├── scripts/
    │   └── send_ipfix_with_ip.py  # IPFIX发送器 (需要添加TCP支持)
    └── data/
        └── sav_example.json   # 测试数据
```

### 文档
```
/workspaces/pmacct/
├── WORKSTATE.md               # 项目状态（100%核心完成）
├── HACKATHON_DEMO.md          # 演示输出样例
├── TODO_NEXT_WEEK.md          # 5天详细计划 ⭐⭐⭐
├── README.md                  # 项目README
└── SESSION_RESUME.md          # 本文档 ⭐
```

---

## 💬 新Chat Session开场白模板

复制以下内容，在新Chat中粘贴：

```
【SAV IPFIX Hackathon项目 - Session恢复】

上次工作日期: 2025-12-04
项目状态: 核心解析功能100%完成，所有4个模板验证通过

已完成:
- ✅ process_sav_fields() 完整实现 (123行)
- ✅ 修复template ID解析bug (34048→901)
- ✅ 支持双编码模式 (标准IANA + 企业PEN=0)
- ✅ 验证所有4个sub-template (901-904, IPv4+IPv6)
- ✅ 11个commit已推送到GitHub

下一步任务:
- 📋 Day 1: TCP传输支持 (~2小时)
- 📋 Day 2: SCTP传输支持 (~3小时)
- 📋 Day 3-4: JSON输出集成 (~8小时)
- 📋 Day 5: 性能测试 (~4小时)

请先执行 SESSION_RESUME.md 中的"快速恢复步骤"验证环境，
然后开始TODO_NEXT_WEEK.md中的Day 1任务。

代码位置:
- 核心: src/nfacctd.c (process_sav_fields, lines 1797-1920)
- 解析: src/sav_parser.c (parse_sav_sub_template_list, lines 141-270)
- 测试: tests/my-SAV-ipfix-test/demo.sh
- 待修改: scripts/send_ipfix_with_ip.py (添加TCP支持)

准备开始。
```

---

## 🔍 技术要点速查

### 1. 双编码模式切换
```c
// 标准IANA模式 (默认)
sav_matched_content = ext_db_get_ie(tpl, 0, 30003, 0);

// 企业模式 (PEN=0, IE 3)
if (!sav_matched_content) 
    sav_matched_content = ext_db_get_ie(tpl, 0, 3, 0);
```

### 2. 变长字段解码 (RFC 7011)
```c
uint8_t first_byte = *data_ptr++;
if (first_byte == 255) {
    content_len = ntohs(*((uint16_t *)data_ptr));
    data_ptr += 2;
} else {
    content_len = first_byte;
}
```

### 3. Template ID传递机制
```c
// 调用方
uint16_t sub_template_id = 0;
ret = parse_sav_sub_template_list(data_ptr, content_len, 
                                   validation_mode, 
                                   &rules, &rule_count, 
                                   &sub_template_id);  // ← 输出参数

// 使用template_id格式化地址
sav_rule_to_string(&rules[i], sub_template_id, rule_str, sizeof(rule_str));
```

### 4. Sub-Template结构
```
901: IPv4 Interface→Prefix  (9 bytes)  = interface_id(4) + ipv4(4) + prefix_len(1)
902: IPv6 Interface→Prefix  (21 bytes) = interface_id(4) + ipv6(16) + prefix_len(1)
903: IPv4 Prefix→Interface  (9 bytes)  = ipv4(4) + prefix_len(1) + interface_id(4)
904: IPv6 Prefix→Interface  (21 bytes) = ipv6(16) + prefix_len(1) + interface_id(4)
```

---

## 📞 紧急联系信息

### GitHub仓库
- **URL**: https://github.com/Cq-zgclab/pmacct
- **分支**: main
- **最新commit**: 5192b4c (2025-12-04)

### 关键参考文档
- RFC 7011: IPFIX Protocol Specification
- RFC 6313: Export of Structured Data in IPFIX (subTemplateList)
- RFC 7013: Guidelines for IPFIX IE Authors (Enterprise IEs)
- draft-cao-opsawg-ipfix-sav-01: SAV using IPFIX

### 快速链接
- pmacct官网: http://www.pmacct.net/
- pmacct Wiki: https://github.com/pmacct/pmacct/wiki
- IANA IPFIX Registry: https://www.iana.org/assignments/ipfix/ipfix.xhtml

---

## ✅ 恢复检查清单

在新Session中依次执行，打勾确认：

- [ ] Git状态检查 (`git status` 显示 "working tree clean")
- [ ] 远程同步确认 (`git log origin/main..HEAD` 输出为空)
- [ ] 重新编译成功 (`make src/nfacctd` 无错误)
- [ ] nfacctd版本正确 (`./src/nfacctd -V` 显示正确版本)
- [ ] demo.sh测试通过 (看到3条SAV规则输出)
- [ ] TODO_NEXT_WEEK.md已阅读 (理解Day 1任务)
- [ ] 编辑器打开send_ipfix_with_ip.py (准备添加TCP函数)

**全部打勾后**, 可以开始Day 1的TCP实现！

---

**文档版本**: 1.0  
**创建日期**: 2025-12-04  
**适用场景**: 1周以上间隔后恢复工作

**保存位置**: `/workspaces/pmacct/SESSION_RESUME.md`
