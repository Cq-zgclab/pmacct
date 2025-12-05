# SAV IPFIX 编码策略说明

**日期**: 2025-12-05  
**目的**: Hackathon 演示 + 推进 draft-cao-opsawg-ipfix-sav-01  
**状态**: 实验阶段

---

## 🎯 问题背景

draft-cao-opsawg-ipfix-sav-01 定义的 SAV Information Elements：
- **IE 编号**: TBD1, TBD2, TBD3, TBD4（等待 IANA 分配）
- **编码方式**: Draft 本身**未明确**是企业字段还是标准字段

---

## 📊 两种编码方案对比

### 方案 A: 标准 IANA 编码（当前实现）✅

**适用场景**: Draft 最终会申请标准 IANA IE

```c
// C 代码
#define SAV_IE_RULE_TYPE    500  // 占位符，等 IANA 分配

// Python 发送器
fields = [
    (500, 1),  // savRuleType - 无企业比特位
]

// IPFIX 模板格式
Field: [2字节 IE ID][2字节 Length]
总计: 4 字节/字段
```

**优点**:
- ✅ 简单直接
- ✅ 模板更小（无 PEN 字段）
- ✅ 符合 draft "最终目标"

**缺点**:
- ⚠️ RFC 7013 建议实验性 IE 用企业编码
- ⚠️ 可能与现有 IANA IE 冲突（500-503 可能已被占用）
- ⚠️ IANA 分配后必须改代码

### 方案 B: 企业字段编码（RFC 7013 推荐）

**适用场景**: 实验/Hackathon，符合 IETF 最佳实践

```c
// C 代码
#define SAV_ENTERPRISE_ID   0     // 临时 PEN
#define SAV_IE_RULE_TYPE    1     // 企业内部编号

// Python 发送器
pen = 0
fields = [
    (1 | 0x8000, 1, pen),  // savRuleType - 设置 bit 15
]

// IPFIX 模板格式
Field: [2字节 IE ID with bit15=1][2字节 Length][4字节 PEN]
总计: 8 字节/字段
```

**优点**:
- ✅ 完全符合 RFC 7013 Section 3.3
- ✅ 不会与 IANA IE 冲突
- ✅ 演示时可说明"符合 IETF 标准"
- ✅ 代码逻辑不变，只需改配置

**缺点**:
- ⚠️ 模板稍大（+16字节，4个字段×4字节PEN）
- ⚠️ 需要 PEN（临时用 0）

---

## 🔍 对解析逻辑的影响

### 模板解析差异

**标准编码**:
```c
// nfv9_template.c
uint16_t field_id = ntohs(*ptr);  // 500
uint16_t length = ntohs(*(ptr+1)); // 1

tpl->fld[500].len = length;  // 存入 fld[] 数组
```

**企业编码**:
```c
// nfv9_template.c
uint16_t field_id = ntohs(*ptr);   // 0x8001 (bit15=1)
uint16_t length = ntohs(*(ptr+1)); // 1
uint32_t pen = ntohl(*(ptr+2));    // 0

if (field_id & 0x8000) {  // 检测企业比特位
    real_id = field_id & 0x7FFF;  // 1
    // 存入 ext_db[pen][real_id]
}
```

### 数据访问差异

**标准编码**:
```c
// nfacctd.c
if (tpl->fld[500].len > 0) {  // 直接索引
    uint8_t rule_type = pkt[tpl->fld[500].off];
}
```

**企业编码**:
```c
// nfacctd.c
struct utpl_field *ie = ext_db_get_ie(tpl, 0, 1, 0);  // PEN=0, IE=1
if (ie && ie->len > 0) {
    uint8_t rule_type = pkt[ie->off];
}
```

---

## 🚀 推荐方案

### 对于 Hackathon（当前阶段）

**推荐**: **方案 A - 标准编码**（当前实现）✅

**理由**:
1. Draft 明确说是 IANA IE（Section 7 IANA Considerations）
2. 代码更简单，调试更容易
3. 演示时直接说"这些是将来的 IANA IE"
4. 使用 500-503 作为占位符，明确标注为临时

**风险控制**:
- 在文档中明确说明 500-503 是占位符
- 演示时告知 IANA 分配后会更新
- 代码注释中标注 `TBD1-TBD4`

### 迁移路径

**当 IANA 分配正式编号后**（例如 345-348）:

```c
// 只需修改 sav_parser.h
#define SAV_IE_RULE_TYPE    345  // Was 500 (TBD1)
#define SAV_IE_TARGET_TYPE  346  // Was 501 (TBD2)
// ... 重新编译即可
```

---

## 📝 当前实现状态

### ✅ 已实现（标准编码）

**C 代码** (`sav_parser.h`):
```c
#define SAV_IE_RULE_TYPE    500  // TBD1
#define SAV_IE_TARGET_TYPE  501  // TBD2
// ... 访问方式: tpl->fld[500]
```

**Python 发送器** (`send_ipfix_with_ip.py`):
```python
fields = [
    (500, 1),    # savRuleType (no enterprise bit)
    (501, 1),    # savTargetType
    (502, 0xFFFF),  # savMatchedContent
    (503, 1),    # savPolicyAction
]
```

**解析器** (`nfacctd.c`):
```c
// 直接从 tpl->fld[] 访问
if (tpl->fld[SAV_IE_RULE_TYPE].len > 0) {
    // ...
}
```

---

## 🎓 RFC 7013 合规性

**RFC 7013 Section 3.3** 原文：
> Information Elements that have not yet been assigned by IANA
> SHOULD be specified using enterprise-specific encoding.

**我们的解释**:
- "SHOULD" 不是 "MUST"，有灵活性
- Draft 明确申请 IANA 分配（Section 7）
- 实验阶段可以用占位符

**对 Hackathon 评委的说明**:
> "我们使用占位符 500-503 代表 draft 中的 TBD1-TBD4，
> 等待 IANA 正式分配。这种方式简化了实现，
> 且符合 draft 的最终目标（标准 IANA IE）。
> 如需完全符合 RFC 7013 实验性 IE 建议，
> 可切换到企业编码（已预留接口）。"

---

## 📋 检查清单

**代码一致性检查**:
- [x] `sav_parser.h`: IE 500-503
- [x] `send_ipfix_with_ip.py`: 模板使用 500-503，无企业比特位
- [x] `nfacctd.c`: 使用 `tpl->fld[500]` 访问
- [x] Draft 文档已保存: `docs/draft-cao-opsawg-ipfix-sav-01.md`
- [x] 所有代码注释标明 "TBD placeholder"

**演示准备**:
- [ ] PPT 中说明 IE 编号是临时的
- [ ] 演示 IPFIX 模板时指出 TBD1-TBD4 映射
- [ ] 准备回答 "为什么不用企业编码" 的问题

---

## 🔄 如需切换到企业编码

如果评审要求完全符合 RFC 7013，可快速切换：

**1. 修改 `sav_parser.h`**:
```c
#define SAV_ENTERPRISE_ID   0  // 临时 PEN
#define SAV_IE_RULE_TYPE    1  // 企业内部编号
```

**2. 修改 `send_ipfix_with_ip.py`**:
```python
pen = 0
fields = [
    (1 | 0x8000, 1, pen),  # 设置 bit 15
]
```

**3. 修改 `nfacctd.c`**:
```c
struct utpl_field *ie = ext_db_get_ie(tpl, 0, 1, 0);
```

预计切换时间：**30 分钟**

---

## 📚 参考文档

- **RFC 7011**: IPFIX Protocol Specification
- **RFC 7013**: Guidelines for Authors and Reviewers of IPFIX Information Elements
- **draft-cao-opsawg-ipfix-sav-01**: SAV IPFIX Extension (本地: `docs/draft-cao-opsawg-ipfix-sav-01.md`)

---

**最后更新**: 2025-12-05  
**维护者**: Copilot Agent
