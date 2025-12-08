# SAV IPFIX - RFC 7011 Compliant Implementation

**创建日期**: 2025-12-08  
**目标**: 使用成熟IPFIX库，完全符合RFC 7011规范  
**架构**: 独立collector + 标准库 + 可选pmacct集成

---

## 📊 当前状态评估

### PoC实现问题
- ❌ **SCTP支持不完整**: Sender有SCTP但collector不支持（RFC 7011 MUST要求）
- ❌ **缺少Template管理**: 无Template撤回、重传、生命周期管理
- ❌ **IPC架构不匹配**: pmacct多进程架构导致SAV数据无法正确传递
- ❌ **非标准实现**: 手写parser缺少RFC完整特性
- ⚠️ **可用作参考**: 解析逻辑、数据结构设计可复用

### 保留价值
- ✅ SAV字段解析逻辑 (`sav_parser.c`)
- ✅ 子模板处理 (templates 901-904)
- ✅ 数据结构设计 (`struct sav_rule`)
- ✅ 测试用例和数据 (`tests/my-SAV-ipfix-test/`)

---

## 🎯 新架构设计

```
┌─────────────────────────────────────────────────────────────┐
│                    SAV IPFIX Ecosystem                       │
└─────────────────────────────────────────────────────────────┘

[Phase 1: RFC-Compliant Collector]
┌──────────────────┐
│ Network Devices  │ (Future: Real devices)
│ SAV Generators   │
└────────┬─────────┘
         │ IPFIX over SCTP (Primary)
         │ IPFIX over TCP  (Secondary)
         │ IPFIX over UDP  (Testing)
         ▼
┌────────────────────────────────────────────────────┐
│           SAV IPFIX Collector (NEW)                │
│  ┌──────────────────────────────────────────────┐ │
│  │ libfixbuf / ipfix Library Layer              │ │
│  │  - RFC 7011 Transport (SCTP/TCP/UDP)         │ │
│  │  - Template Management                       │ │
│  │  - Session Management                        │ │
│  │  - Message Framing                           │ │
│  └──────────────┬───────────────────────────────┘ │
│                 ▼                                  │
│  ┌──────────────────────────────────────────────┐ │
│  │ SAV Specific Parser                          │ │
│  │  - IE 30001-30004 / Enterprise 1-4           │ │
│  │  - SubTemplateList decoder (RFC 6313)        │ │
│  │  - Templates 901-904 handler                 │ │
│  └──────────────┬───────────────────────────────┘ │
│                 ▼                                  │
│  ┌──────────────────────────────────────────────┐ │
│  │ Data Storage & Export                        │ │
│  │  - In-memory cache                           │ │
│  │  - JSON file output                          │ │
│  │  - SQLite/PostgreSQL (optional)              │ │
│  │  - REST API (future)                         │ │
│  └──────────────────────────────────────────────┘ │
└────────────────────────────────────────────────────┘

[Phase 2: Generator/Sender]
┌────────────────────────────────────────────────────┐
│        SAV IPFIX Exporter (Python/C)               │
│  - Uses libfixbuf Python bindings / pyfixbuf      │
│  - Generates SAV records from test data           │
│  - RFC-compliant message framing                  │
│  - Template management                            │
└────────────────────────────────────────────────────┘

[Phase 3: Integration (Optional)]
┌────────────────────────────────────────────────────┐
│              pmacct Integration                    │
│  - Reads JSON from SAV collector                  │
│  - Correlates with flow data                      │
│  - Unified dashboard                              │
└────────────────────────────────────────────────────┘
```

---

## 📋 实施计划

### Phase 0: 调研与准备 (Day 1, ~4小时)

#### 0.1 IPFIX库评估 (~2小时)
**候选库**:

1. **libfixbuf** (C) ⭐ 推荐
   - 来源: CERT/NetSA YAF项目
   - 优势: 
     * 完整RFC 7011支持
     * SCTP/TCP/UDP全支持
     * Template自动管理
     * 生产环境验证（NSA、美国国防部使用）
     * 活跃维护
   - 文档: https://tools.netsa.cert.org/fixbuf/
   - License: GPL v2 (与pmacct兼容)

2. **pyfixbuf** (Python binding for libfixbuf)
   - 用途: Python sender快速原型
   - 与libfixbuf C库互操作

3. **go-ipfix** (Go)
   - 来源: VMware
   - 优势: 云原生、高性能
   - 考虑: 如需要高性能独立collector

4. **pmacct现有IPFIX支持**
   - 检查: nfacctd是否可扩展支持SCTP
   - 结论: 需要检查代码

**任务**:
- [ ] 克隆libfixbuf仓库并阅读文档
- [ ] 编译示例程序（ipfixDump）
- [ ] 测试与现有PoC sender互操作
- [ ] 评估学习曲线和集成难度
- [ ] 决策: 使用哪个库

#### 0.2 RFC 7011深度阅读 (~2小时)
**重点章节**:
- [ ] Section 3: IPFIX Message Format
- [ ] Section 5: IPFIX Information Elements
- [ ] Section 8: Template Management
- [ ] Section 10: **Transport Protocol** (关键!)
  * 10.1 SCTP - REQUIRED
  * 10.2 TCP - MAY
  * 10.3 UDP - MAY
- [ ] Section 11: Security Considerations

**RFC 6313 (subTemplateList)**:
- [ ] Section 4.5: SubTemplateList encoding
- [ ] Section 4.5.3: Semantic field

**输出**:
- [ ] RFC要求清单文档
- [ ] 必须实现的特性列表
- [ ] 测试用例设计

---

### Phase 1: Collector实现 (Day 2-4, ~12小时)

#### 1.1 环境搭建 (~1小时)
```bash
# 安装libfixbuf
cd /tmp
git clone https://github.com/cert-netsa/libfixbuf.git
cd libfixbuf
./autogen.sh
./configure --prefix=/usr/local
make
sudo make install
sudo ldconfig

# 安装依赖
sudo apt-get install -y \
    libglib2.0-dev \
    libsctp-dev \
    lksctp-tools
```

**任务**:
- [ ] 安装libfixbuf及依赖
- [ ] 编译示例程序
- [ ] 验证SCTP支持: `ipfixDump --in=sctp --port=4739`
- [ ] 创建工作目录结构

#### 1.2 基础Collector框架 (~3小时)
**文件**: `sav-collector/src/collector.c`

```c
// 基于libfixbuf的SAV collector框架
#include <fixbuf/public.h>

// 1. 初始化SCTP listener (RFC 7011 Section 10.1)
// 2. 注册SAV Information Elements (30001-30004)
// 3. 注册SubTemplateList decoder
// 4. 接收循环
// 5. Template callback处理
```

**关键实现**:
- [ ] SCTP socket监听 (端口4739, IANA分配给IPFIX)
- [ ] TCP fallback listener
- [ ] UDP listener (测试用)
- [ ] fbListener创建和配置
- [ ] fbSession管理
- [ ] fbCollector创建

#### 1.3 SAV Information Element注册 (~2小时)
**文件**: `sav-collector/src/sav_ie.c`

```c
// SAV IE定义 (draft-cao-opsawg-ipfix-sav-01)
static fbInfoElement_t sav_info_elements[] = {
    // Standard IANA (pending)
    {"savRuleType",           30001, 1, 0, 0},
    {"savTargetType",         30002, 1, 0, 0},
    {"savMatchedContentList", 30003, FB_IE_VARLEN, 0, 0},
    {"savPolicyAction",       30004, 1, 0, 0},
    
    // Enterprise (PEN=0, IE 1-4) for testing
    {"savRuleType",           1, 1, 0, FB_IE_VENDOR_BIT_REVERSE},
    // ... other enterprise definitions
    
    // Sub-template fields (901-904)
    {"savInterfaceId",        30005, 4, 0, 0},
    {"savIPv4Prefix",         30006, 4, 0, 0},
    {"savIPv6Prefix",         30007, 16, 0, 0},
    {"savPrefixLength",       30008, 1, 0, 0},
    
    FB_IE_NULL
};

// 注册到infoModel
fbInfoModel_t *infoModel = fbInfoModelAlloc();
fbInfoModelAddElementArray(infoModel, sav_info_elements);
```

**任务**:
- [ ] 定义所有SAV IE
- [ ] 支持双编码模式（Standard + Enterprise）
- [ ] 注册到fbInfoModel

#### 1.4 Template处理 (~3小时)
**文件**: `sav-collector/src/template_handler.c`

```c
// Template callback
static void template_callback(
    fbSession_t    *session,
    uint16_t        tid,
    fbTemplate_t   *tmpl,
    void           *app_ctx,
    void          **tmpl_ctx,
    fbTemplateCtxFree_fn *fn)
{
    // 识别SAV template (包含savRuleType等字段)
    // 设置template context for decoding
    // 准备subTemplateList decoder
}

// Data record callback
static gboolean record_callback(
    fbSession_t   *session,
    uint16_t       tid,
    fbRecord_t    *record,
    void          *ctx)
{
    // 解码main template
    // 解码subTemplateList (savMatchedContentList)
    // 调用SAV parser
    // 输出到JSON
}
```

**任务**:
- [ ] 实现Template callback
- [ ] 实现Data record callback
- [ ] SubTemplateList解码 (fbSubTemplateList API)
- [ ] Template 901-904特殊处理

#### 1.5 SAV数据解码 (~2小时)
**文件**: `sav-collector/src/sav_decoder.c`

```c
// 复用现有sav_parser.c逻辑
typedef struct sav_record_s {
    uint8_t   rule_type;
    uint8_t   target_type;
    uint8_t   policy_action;
    uint16_t  sub_template_id;
    
    // Decoded rules from subTemplateList
    uint32_t     rule_count;
    sav_rule_t  *rules;  // 复用现有struct sav_rule
} sav_record_t;

int decode_sav_record(fbRecord_t *rec, sav_record_t *sav);
```

**任务**:
- [ ] 复用`sav_parser.c`的解析逻辑
- [ ] 适配libfixbuf的API
- [ ] 处理varlen字段
- [ ] 处理4种sub-template

#### 1.6 数据输出 (~1小时)
**文件**: `sav-collector/src/output.c`

```c
// JSON output (与PoC格式兼容)
void output_sav_json(sav_record_t *sav, FILE *fp) {
    fprintf(fp, "{\"timestamp\":%ld,", time(NULL));
    fprintf(fp, "\"sav_validation_mode\":\"%s\",", 
            mode_to_string(sav->rule_type));
    fprintf(fp, "\"sav_matched_rules\":[");
    // ... output rules array
    fprintf(fp, "]}\n");
}
```

**输出选项**:
- [x] JSON文件 (立即实现)
- [ ] SQLite数据库 (Phase 2)
- [ ] REST API (Phase 3)
- [ ] Prometheus metrics (Phase 3)

---

### Phase 2: Sender/Exporter重构 (Day 5-6, ~8小时)

#### 2.1 使用pyfixbuf重写Sender (~4小时)
**文件**: `tests/sav-sender-rfc7011/send_sav_ipfix.py`

```python
#!/usr/bin/env python3
"""
RFC 7011 Compliant SAV IPFIX Sender
Uses pyfixbuf for proper IPFIX message generation
"""
import pyfixbuf as fixbuf

# 1. Create session
session = fixbuf.Session(info_model)

# 2. Add SAV IEs to info model
session.add_internal_template(sav_template)

# 3. Create exporter (SCTP/TCP/UDP)
exporter = fixbuf.Exporter.for_spec(
    "sctp://collector.example.com:4739"
)

# 4. Export SAV records
for rule_set in sav_data:
    record = create_sav_record(rule_set)
    session.export(record)
    
session.flush()
```

**任务**:
- [ ] 安装pyfixbuf: `pip install pyfixbuf`
- [ ] 重写sender使用pyfixbuf API
- [ ] 支持SCTP/TCP/UDP transport selection
- [ ] Template自动管理
- [ ] 读取现有test data (`data/sav_example.json`)

#### 2.2 C语言Sender (可选, ~4小时)
**文件**: `tests/sav-sender-c/sav_exporter.c`

使用libfixbuf C API，更高性能，可作为embedded exporter参考实现。

**任务**:
- [ ] fbExporter创建
- [ ] fbSession配置
- [ ] Template发送
- [ ] Data record发送
- [ ] 编译和测试

---

### Phase 3: 测试验证 (Day 7, ~4小时)

#### 3.1 RFC 7011合规性测试
**测试点**:

**Transport Layer (RFC 7011 Section 10)**:
- [ ] SCTP连接建立和数据传输
- [ ] TCP with 2-byte length prefix
- [ ] UDP datagram完整性
- [ ] SCTP多流: Template on Stream 0, Data on Stream 1+
- [ ] 连接中断和重连
- [ ] Template重传 (SCTP丢失处理)

**Template Management (RFC 7011 Section 8)**:
- [ ] Template定义正确导出
- [ ] Template ID唯一性
- [ ] Template撤回 (Template Withdrawal)
- [ ] Template超时和重传 (UDP模式)
- [ ] Options Template支持

**Message Format (RFC 7011 Section 3)**:
- [ ] Message Header格式验证
- [ ] Set Header格式验证
- [ ] Field length encoding
- [ ] Padding处理

**SubTemplateList (RFC 6313)**:
- [ ] Semantic field正确性
- [ ] Sub-template ID匹配
- [ ] Nested list decoding
- [ ] 4种sub-template (901-904)

#### 3.2 互操作性测试
**工具**:
- [ ] ipfixDump (libfixbuf工具)
- [ ] Wireshark IPFIX dissector
- [ ] nProbe (商业IPFIX collector)

**测试场景**:
```bash
# 1. Sender → 我们的Collector
./send_sav_ipfix.py --transport sctp --collector localhost:4739

# 2. 我们的Sender → ipfixDump
ipfixDump --in=sctp --port=4739

# 3. Wireshark抓包验证
tcpdump -i lo -w sav_ipfix.pcap port 4739
wireshark sav_ipfix.pcap
```

#### 3.3 性能测试
```bash
# 高负载测试
./send_sav_ipfix.py \
    --rate 1000  \
    --duration 300 \
    --rules-per-message 10
    
# 监控collector性能
top -p $(pgrep sav-collector)
cat /proc/$(pgrep sav-collector)/status
```

**指标**:
- [ ] Messages per second
- [ ] CPU使用率 < 50%
- [ ] 内存占用 < 100MB
- [ ] 丢包率 < 0.1%
- [ ] 解码延迟 < 10ms

---

### Phase 4: 文档与标准化 (Day 8, ~4小时)

#### 4.1 Implementation Report
**文件**: `docs/RFC7011_IMPLEMENTATION_REPORT.md`

```markdown
# SAV IPFIX Implementation Report

## 1. Overview
- Implementation: sav-collector v1.0
- Based on: libfixbuf 3.x
- RFC compliance: RFC 7011, RFC 6313, draft-cao-opsawg-ipfix-sav-01

## 2. Transport Support
- ✅ SCTP (REQUIRED per RFC 7011)
- ✅ TCP (optional)
- ✅ UDP (optional)

## 3. Features Implemented
- ✅ Template Management
- ✅ SubTemplateList (RFC 6313)
- ✅ SAV-specific IEs (30001-30004)
- ✅ 4 sub-templates (901-904)

## 4. Testing Results
...

## 5. Interoperability
Tested with: ipfixDump, Wireshark, nProbe

## 6. Known Limitations
...
```

#### 4.2 API文档
**工具**: Doxygen

```bash
# 生成API文档
cd sav-collector
doxygen Doxyfile
```

#### 4.3 用户指南
**文件**: `docs/USER_GUIDE.md`

- [ ] 安装说明
- [ ] 配置文件格式
- [ ] 运行示例
- [ ] 故障排查

---

## 📂 新目录结构

```
pmacct/
├── sav-collector/              # 新的RFC-compliant collector
│   ├── src/
│   │   ├── main.c             # 主程序
│   │   ├── collector.c        # libfixbuf wrapper
│   │   ├── sav_ie.c           # IE定义和注册
│   │   ├── template_handler.c # Template callbacks
│   │   ├── sav_decoder.c      # SAV-specific解码
│   │   ├── output.c           # JSON/DB输出
│   │   └── sav_parser.c       # 复用现有parser逻辑
│   ├── include/
│   │   └── sav_collector.h
│   ├── tests/
│   │   ├── test_sctp.c
│   │   ├── test_template.c
│   │   └── test_subtemplateList.c
│   ├── Makefile
│   └── README.md
│
├── tests/sav-sender-rfc7011/   # 新的RFC-compliant sender
│   ├── send_sav_ipfix.py      # pyfixbuf版本
│   ├── sav_exporter.c         # C语言版本 (可选)
│   ├── data/
│   │   └── sav_rules.json     # 测试数据
│   └── README.md
│
├── docs/
│   ├── RFC7011_IMPLEMENTATION_REPORT.md
│   ├── ARCHITECTURE.md        # 新架构说明
│   ├── API_REFERENCE.md
│   └── USER_GUIDE.md
│
├── tests/my-SAV-ipfix-test/    # 旧的PoC (保留参考)
│   └── README_LEGACY.md       # 标注为legacy
│
└── TODO_RFC7011_COMPLIANT.md   # 本文件
```

---

## 🎯 里程碑和时间估算

| Phase | 任务 | 时间 | 输出 |
|-------|-----|------|-----|
| **Phase 0** | 调研与准备 | 4h | 库选型、RFC笔记 |
| **Phase 1** | Collector实现 | 12h | sav-collector可执行文件 |
| **Phase 2** | Sender重构 | 8h | RFC-compliant sender |
| **Phase 3** | 测试验证 | 4h | 测试报告 |
| **Phase 4** | 文档 | 4h | Implementation Report |
| **总计** | | **32小时** | **生产级实现** |

**按工作日**: 约4-5天 (每天6-8小时)

---

## ✅ 验收标准

### 必须满足 (MUST)
- [ ] ✅ SCTP transport工作 (RFC 7011 Section 10.1)
- [ ] ✅ 能够接收和解析SAV IPFIX消息
- [ ] ✅ 正确解码subTemplateList (RFC 6313)
- [ ] ✅ 支持4种sub-template (901-904)
- [ ] ✅ JSON输出格式正确
- [ ] ✅ 通过ipfixDump验证
- [ ] ✅ 通过Wireshark验证
- [ ] ✅ Template管理正确 (注册/撤回)

### 应该满足 (SHOULD)
- [ ] TCP transport工作
- [ ] UDP transport工作
- [ ] 性能测试通过 (>1000 msg/s)
- [ ] 内存无泄漏 (valgrind)
- [ ] 完整的错误处理
- [ ] 日志级别可配置

### 可以满足 (MAY)
- [ ] REST API
- [ ] 数据库存储
- [ ] 与pmacct集成
- [ ] Web UI
- [ ] Prometheus metrics

---

## 🚀 快速启动 (Phase 1完成后)

### 启动Collector
```bash
cd sav-collector
./sav-collector \
    --listen sctp://0.0.0.0:4739 \
    --output-json /tmp/sav_records.json \
    --log-level info
```

### 发送测试数据
```bash
cd tests/sav-sender-rfc7011
./send_sav_ipfix.py \
    --transport sctp \
    --collector localhost:4739 \
    --data data/sav_rules.json
```

### 验证输出
```bash
tail -f /tmp/sav_records.json | jq .
```

---

## 🔄 与PoC的关系

### 保留
- ✅ `sav_parser.c` 的解析逻辑
- ✅ `struct sav_rule` 数据结构
- ✅ 测试数据 (`data/sav_example.json`)
- ✅ 文档和RFC分析

### 替换
- ❌ 手写IPFIX parser → libfixbuf
- ❌ UDP-only nfacctd → 独立SCTP collector
- ❌ 直接文件输出 → 标准IPC + JSON API
- ❌ Python纯手工编码 → pyfixbuf

### 迁移路径
```
PoC (pmacct集成)
       ↓
   保留parser逻辑
       ↓
RFC-compliant collector (libfixbuf)
       ↓
   (可选) JSON → pmacct plugin
```

---

## 📞 参考资源

### RFC文档
- **RFC 7011**: IPFIX Protocol Specification (MUST READ)
- **RFC 6313**: Export of Structured Data in IPFIX
- **RFC 4960**: SCTP Protocol
- **draft-cao-opsawg-ipfix-sav-01**: SAV IPFIX定义

### 库和工具
- **libfixbuf**: https://tools.netsa.cert.org/fixbuf/
- **pyfixbuf**: https://github.com/britram/pyfixbuf (unofficial)
- **YAF**: https://tools.netsa.cert.org/yaf/ (参考实现)

### 示例代码
- libfixbuf examples: `libfixbuf/src/ipfixDump.c`
- YAF exporter: YAF项目中的导出器实现

---

## 🎓 学习路径

### Day 1: 基础
1. 阅读RFC 7011 Section 1-5, 10
2. 编译libfixbuf和示例程序
3. 运行ipfixDump并理解输出

### Day 2-4: 实现
1. 创建collector框架
2. 注册SAV IEs
3. 实现callbacks
4. 集成sav_parser

### Day 5-6: Sender
1. 学习pyfixbuf API
2. 重写sender
3. 端到端测试

### Day 7: 验证
1. RFC合规性checklist
2. 互操作性测试
3. 性能测试

### Day 8: 文档
1. Implementation Report
2. API文档
3. User Guide

---

## ❓ FAQ

**Q: 为什么不继续修改pmacct?**  
A: pmacct的多进程IPC架构不适合IPFIX的复杂数据结构。使用专用库可以获得完整RFC支持，且更易维护。

**Q: 现有PoC代码会浪费吗?**  
A: 不会。Parser逻辑、数据结构、测试用例都可以复用。只是传输和Template管理层改用标准库。

**Q: 性能会下降吗?**  
A: 相反，libfixbuf是优化过的生产级库，性能会更好。

**Q: 学习曲线陡峭吗?**  
A: 相比深入修改pmacct更简单。libfixbuf的API清晰，有完整文档和示例。

**Q: 支持Windows吗?**  
A: libfixbuf主要支持Linux/Unix。Windows需要MinGW或Cygwin。

**Q: 最终能与pmacct集成吗?**  
A: 可以。通过JSON API或共享内存，pmacct plugin可以读取SAV数据并关联flow信息。

---

**下一步行动**: 从Phase 0开始，安装libfixbuf并阅读RFC 7011 Section 10! 🚀
