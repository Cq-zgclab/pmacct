# 工作状态记录 (Work State Record)

**最后更新**: 2025年12月4日  
**当前阶段**: Phase 1A 完成，准备启动 Phase 1B

---

## 📋 总体进度

### ✅ Phase 1A: Python发送端完整实现 (已完成)
- [x] RFC 6313 subTemplateList 完整支持
- [x] 4个子模板实现 (Template 901-904)
- [x] RFC 7011 变长编码
- [x] IPv4 + IPv6 双栈测试
- [x] JSON 规则文件支持
- [x] 文件重组织 (25个文件 → 7个目录)
- [x] 添加标准注释和文档
- [x] 统一测试入口创建

### 🔧 Phase 1B: pmacct C代码解析端 (下一步)
- [ ] **高优先级**: nfv9_template.c 扩展 - 识别子模板 901-904
- [ ] **高优先级**: 实现 subTemplateList 递归解析
- [ ] **高优先级**: sav_parser.c 从头实现 (当前为空文件)
- [ ] **中优先级**: JSON 输出增强 - 显示解析后的 SAV 规则
- [ ] **低优先级**: sav_primitives.lst 完善字段映射

---

## 📁 当前文件结构

```
tests/my-SAV-ipfix-test/
├── config/                    # 配置文件
│   ├── nfacctd-00.conf       # nfacctd 测试配置
│   ├── requirements.txt      # Python 依赖 (空)
│   └── sav_primitives.lst    # SAV 字段定义 (待完善)
├── docker/                    # Docker 相关
│   ├── Dockerfile.sender     # 发送端容器
│   └── docker-compose.yml    # 编排文件
├── docs/                      # 文档目录 (7个文件)
│   ├── EXECUTION_SUMMARY.md
│   ├── FILE_REORGANIZATION.md
│   ├── IMPROVEMENTS_AND_USECASES.md
│   ├── PHASE1A_SUMMARY.txt
│   ├── README.run_local.md
│   ├── SAV_IPFIX_VALIDATION_REPORT.md
│   ├── USECASE_TEST_GUIDE.md
│   └── send_ipfix_with_ip_ANALYSIS.md
├── scripts/                   # Python 脚本 (5个)
│   ├── send_ipfix.py         # 原始发送脚本
│   ├── send_ipfix_with_ip.py # 主工具 (250+ 行新代码)
│   ├── send_templates_batch.py
│   ├── send_usecase1_attack.py
│   └── test_sav_ipfix.py
├── test-data/                 # 测试数据 (4个 JSON + README)
│   ├── README.md
│   ├── sav_rules_example.json           # Template 901 (IPv4 if2prefix)
│   ├── sav_rules_ipv6_example.json      # Template 902 (IPv6 if2prefix)
│   ├── sav_rules_prefix2if_ipv4.json    # Template 903 (IPv4 prefix2if)
│   └── sav_rules_prefix2if_ipv6.json    # Template 904 (IPv6 prefix2if)
├── tests/                     # 测试脚本 (3个)
│   ├── run_all_tests.sh      # 统一测试入口 ⭐
│   ├── test_all_templates.sh # 完整测试 (6个场景)
│   └── test_phase1a.sh       # Phase 1A 验证
├── output/                    # 运行时输出 (已 gitignore)
├── .gitignore
└── README.md
```

---

## 🎯 Phase 1B 实施计划 (选项B: 完整实现)

### 1. nfv9_template.c 扩展识别 ⭐⭐⭐⭐⭐
**位置**: `src/nfv9_template.c` (lines ~164-400)  
**任务**: 扩展 `handle_template_v2()` 识别 Template ID 901-904

**实施步骤**:
```c
// 在 handle_template_v2() 中添加:
if (template_id >= 901 && template_id <= 904) {
    // 注册子模板到缓存
    // 901: interface(32), ipv4_prefix(32), prefix_len(8)
    // 902: interface(32), ipv6_prefix(128), prefix_len(8)
    // 903: ipv4_prefix(32), prefix_len(8), interface(32)
    // 904: ipv6_prefix(128), prefix_len(8), interface(32)
}
```

**关键点**:
- 子模板只有3个字段 (固定长度)
- 需要缓存子模板定义用于后续解析
- 验证字段顺序和类型

---

### 2. subTemplateList 递归解析 ⭐⭐⭐⭐⭐
**位置**: `src/nfv9_template.c` (lines 1266-1320)  
**任务**: 修改 `resolve_vlen_template()` 支持 RFC 6313

**实施步骤**:
```c
int parse_sub_template_list(u_char *data, uint16_t len, 
                             struct template_cache_entry *tpl) {
    // 1. 读取变长长度 (1 或 3 字节)
    uint16_t total_len = decode_varlen(&data, &len);
    
    // 2. 读取 semantic (1字节)
    uint8_t semantic = *data++;
    
    // 3. 读取 template_id (2字节)
    uint16_t sub_template_id = ntohs(*(uint16_t*)data);
    data += 2;
    
    // 4. 查找子模板定义
    struct template_cache_entry *sub_tpl = find_template(sub_template_id);
    
    // 5. 递归解析记录
    while (data < data_end) {
        parse_template_record(data, sub_tpl);
        data += calculate_record_length(sub_tpl);
    }
}
```

**关键点**:
- RFC 7011 变长编码: `<255` 用1字节, `≥255` 用3字节 (0xFF + 2字节长度)
- semantic=0x03 表示 allOf (所有规则)
- 需要递归调用模板解析逻辑
- 当前 `resolve_vlen_template()` 只处理简单变长字段

---

### 3. sav_parser.c 实现 ⭐⭐⭐⭐⭐
**位置**: `src/sav_parser.c` (当前为空)  
**任务**: 从头实现 SAV 规则解析

**数据结构**:
```c
// include/sav_parser.h (新建)
struct sav_rule {
    uint32_t interface_id;          // 接口ID
    union {
        uint32_t ipv4[4];           // IPv4前缀 (主机序)
        uint8_t ipv6[16];           // IPv6前缀
    };
    uint8_t prefix_len;             // 前缀长度
    uint8_t validation_mode;        // 验证模式 (从主模板)
};

int parse_sav_matched_content(
    u_char *data, 
    uint16_t len, 
    uint16_t template_id,        // 901-904
    uint8_t validation_mode,      // 从主字段传入
    struct sav_rule **rules,      // 输出规则数组
    int *count                    // 规则数量
);
```

**解析逻辑**:
```c
int parse_sav_matched_content(...) {
    // 1. 解析 subTemplateList 头部
    uint16_t total_len = decode_varlen(&data, &len);
    uint8_t semantic = *data++;
    uint16_t sub_template_id = ntohs(*(uint16_t*)data);
    data += 2;
    
    // 2. 根据 template_id 确定记录大小
    int record_size;
    switch(sub_template_id) {
        case 901: record_size = 9; break;  // 4+4+1
        case 902: record_size = 21; break; // 4+16+1
        case 903: record_size = 9; break;
        case 904: record_size = 21; break;
    }
    
    // 3. 解析每条规则
    *count = (total_len - 3) / record_size;
    *rules = malloc(*count * sizeof(struct sav_rule));
    
    for (int i = 0; i < *count; i++) {
        if (sub_template_id == 901 || sub_template_id == 902) {
            // interface_id first
            (*rules)[i].interface_id = ntohl(*(uint32_t*)data);
            data += 4;
            // then prefix
            if (sub_template_id == 901) {
                (*rules)[i].ipv4[0] = ntohl(*(uint32_t*)data);
                data += 4;
            } else {
                memcpy((*rules)[i].ipv6, data, 16);
                data += 16;
            }
        } else {
            // prefix first
            // ... (similar logic)
        }
        (*rules)[i].prefix_len = *data++;
        (*rules)[i].validation_mode = validation_mode;
    }
    
    return 0;
}
```

**关键点**:
- Template 901/902: interface_id 在前
- Template 903/904: prefix 在前
- 需要字节序转换 (网络序 → 主机序)
- IPv6 地址是 16 字节连续存储

---

### 4. JSON 输出增强 ⭐⭐⭐⭐
**位置**: `src/print_plugin.c` (需定位具体函数)  
**任务**: 修改输出格式

**当前输出** (不可读):
```json
{
  "sav_matched_content": "0x1b0003038d0000001389..."
}
```

**目标输出**:
```json
{
  "sav_validation_mode": "interface-to-prefix",
  "sav_matched_rules": [
    {
      "interface_id": 5001,
      "prefix": "198.51.100.0/24"
    },
    {
      "interface_id": 5002,
      "prefix": "203.0.113.0/24"
    }
  ]
}
```

**实施步骤**:
1. 搜索 `print_plugin.c` 中 `sav_matched_content` 的输出代码
2. 调用 `parse_sav_matched_content()` 解析二进制数据
3. 将 `struct sav_rule` 数组序列化为 JSON 数组
4. 添加 `sav_validation_mode` 字段 (映射 0-3 到可读名称)

---

### 5. sav_primitives.lst 完善 ⭐⭐⭐
**位置**: `config/sav_primitives.lst` (当前为空)  
**任务**: 定义字段映射

**待添加内容**:
```
# SAV IPFIX Fields Primitives
# Enterprise ID: 45575 (draft-cao-opsawg-ipfix-sav-01)

type=sav_rule_type id=45575:900 len=1
type=sav_target_type id=45575:901 len=1
type=sav_matched_content id=45575:902 len=v
type=sav_policy_action id=45575:903 len=1

# Validation Modes:
# 0: interface-to-prefix (ACL)
# 1: prefix-to-interface (uRPF)
# 2: prefix-to-as (BGP AS Path)
# 3: interface-to-as (BGP Peer)
```

---

## 🧪 测试验证 (Phase 1A 已完成)

### 测试结果
```bash
# Template 901 (IPv4 interface-to-prefix)
$ python3 scripts/send_ipfix_with_ip.py \
    --sav-rules test-data/sav_rules_example.json \
    --sub-template-id 901 \
    --use-complete-message
✅ 198 bytes sent (3 rules × 9 bytes = 27 bytes payload)

# Template 902 (IPv6 interface-to-prefix)
$ python3 scripts/send_ipfix_with_ip.py \
    --sav-rules test-data/sav_rules_ipv6_example.json \
    --sub-template-id 902 \
    --use-complete-message
✅ 213 bytes sent (2 rules × 21 bytes = 42 bytes payload)

# Template 903 (IPv4 prefix-to-interface)
$ python3 scripts/send_ipfix_with_ip.py \
    --sav-rules test-data/sav_rules_prefix2if_ipv4.json \
    --sub-template-id 903 \
    --use-complete-message
✅ 189 bytes sent (2 rules × 9 bytes = 18 bytes payload)

# Template 904 (IPv6 prefix-to-interface)
$ python3 scripts/send_ipfix_with_ip.py \
    --sav-rules test-data/sav_rules_prefix2if_ipv6.json \
    --sub-template-id 904 \
    --use-complete-message
✅ 213 bytes sent (2 rules × 21 bytes = 42 bytes payload)
```

### 快速测试命令
```bash
# 运行所有测试
cd /workspaces/pmacct/tests/my-SAV-ipfix-test
./tests/run_all_tests.sh --quick

# 完整测试 (包含清理)
./tests/run_all_tests.sh --full
```

---

## 📚 技术参考

### RFC 和标准
- **RFC 7011**: IPFIX Protocol Specification (变长编码: Section 7)
- **RFC 6313**: Export of Structured Data in IPFIX (subTemplateList: Section 4.5.2)
- **draft-cao-opsawg-ipfix-sav-01**: SAV Information Elements (Enterprise ID: 45575)

### pmacct 代码关键点
- `src/nfv9_template.c:handle_template_v2()`: 模板注册入口 (lines 164-400)
- `src/nfv9_template.c:resolve_vlen_template()`: 变长字段处理 (lines 1266-1320)
- `src/nfv9_template.c:get_ipfix_vlen()`: 变长长度解码
- `src/sav_parser.c`: 空文件，需完整实现

### 数据格式
```
subTemplateList 结构 (RFC 6313):
┌─────────────────────────────────────────┐
│ Variable-Length (1 or 3 bytes)          │  总长度
├─────────────────────────────────────────┤
│ Semantic (1 byte) = 0x03 (allOf)        │
├─────────────────────────────────────────┤
│ Template ID (2 bytes) = 901-904         │
├─────────────────────────────────────────┤
│ Record 1 (9 or 21 bytes)                │
│ Record 2 (9 or 21 bytes)                │
│ ...                                     │
└─────────────────────────────────────────┘

Record 格式 (Template 901 - IPv4 interface-to-prefix):
┌─────────────────────────────────────────┐
│ interface_id (4 bytes, uint32)          │  e.g. 5001
├─────────────────────────────────────────┤
│ ipv4_prefix (4 bytes, IPv4Address)      │  e.g. 198.51.100.0
├─────────────────────────────────────────┤
│ prefix_len (1 byte, uint8)              │  e.g. 24
└─────────────────────────────────────────┘
Total: 9 bytes

Record 格式 (Template 902 - IPv6 interface-to-prefix):
┌─────────────────────────────────────────┐
│ interface_id (4 bytes, uint32)          │  e.g. 5001
├─────────────────────────────────────────┤
│ ipv6_prefix (16 bytes, IPv6Address)     │  e.g. 2001:db8:1::
├─────────────────────────────────────────┤
│ prefix_len (1 byte, uint8)              │  e.g. 48
└─────────────────────────────────────────┘
Total: 21 bytes
```

---

## 🔄 Git 状态

**最后提交**: `fa3c4d5`  
**提交信息**: `feat: Phase 1A - Complete subTemplateList implementation`  
**分支**: `main`

### 提交内容
- ✅ 新增 30 个文件 (文件重组织)
- ✅ 4638 行新增代码
- ✅ 删除旧的平铺结构
- ✅ 所有 Phase 1A 成果已保存

---

## 💡 明天恢复工作

### 1. 查看本文件
```bash
cat /workspaces/pmacct/tests/my-SAV-ipfix-test/WORKSTATE.md
```

### 2. 开始 Phase 1B 第一步
```bash
cd /workspaces/pmacct
# 查看 nfv9_template.c 的模板注册逻辑
grep -n "handle_template_v2" src/nfv9_template.c
```

### 3. 向 AI 简单说明
> "继续 Phase 1B，从 nfv9_template.c 扩展开始"

AI 会自动读取此文件，知道所有上下文，无需重新解释。

---

## 📊 工作量估算

| 任务 | 优先级 | 预估时间 | 复杂度 |
|------|--------|----------|--------|
| nfv9_template.c 识别子模板 | ⭐⭐⭐⭐⭐ | 1-2小时 | 中 |
| subTemplateList 递归解析 | ⭐⭐⭐⭐⭐ | 3-4小时 | 高 |
| sav_parser.c 实现 | ⭐⭐⭐⭐⭐ | 2-3小时 | 中 |
| JSON 输出增强 | ⭐⭐⭐⭐ | 1-2小时 | 低 |
| sav_primitives.lst 完善 | ⭐⭐⭐ | 0.5小时 | 低 |
| **总计** | - | **7.5-11.5小时** | - |

---

## ✅ 检查清单 (明天开始前)

- [ ] 拉取最新代码 (`git pull`)
- [ ] 阅读本 WORKSTATE.md
- [ ] 确认开发环境 (`which python3`, `pmacct --version`)
- [ ] 快速测试 Phase 1A (`./tests/run_all_tests.sh --quick`)
- [ ] 开始 Phase 1B: 打开 `src/nfv9_template.c`

---

**祝明天继续顺利！** 🚀
