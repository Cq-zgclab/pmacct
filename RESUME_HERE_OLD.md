# 🚀 Session恢复快速指南

## 📌 如果您1周后回来继续工作

### 选项1: 在新Chat中恢复（推荐）

复制以下内容到新Chat：

```
【SAV IPFIX Hackathon项目 - Session恢复】

上次工作: 2025-12-04
状态: 核心功能100%完成 + 所有测试通过 + 已推送GitHub

请执行以下操作:
1. 运行恢复脚本验证环境
2. 开始TODO_NEXT_WEEK.md中的Day 1任务

命令:
cd /workspaces/pmacct
./scripts/session_resume.sh

准备好后回复"ready"，我将开始Day 1的TCP支持实现。
```

### 选项2: 手动恢复步骤

```bash
# 1. 进入项目目录
cd /workspaces/pmacct

# 2. 运行自动检查脚本 (5分钟)
./scripts/session_resume.sh

# 3. 如果检查通过，阅读计划
cat TODO_NEXT_WEEK.md | head -50

# 4. 开始Day 1任务
cd tests/my-SAV-ipfix-test/scripts
# 编辑 send_ipfix_with_ip.py 添加TCP支持
```

---

## 📚 关键文档位置

| 文档 | 用途 | 位置 |
|------|------|------|
| **SESSION_RESUME.md** | 完整恢复指南 | `/workspaces/pmacct/SESSION_RESUME.md` |
| **TODO_NEXT_WEEK.md** | 5天详细计划 | `/workspaces/pmacct/TODO_NEXT_WEEK.md` |
| **session_resume.sh** | 自动检查脚本 | `/workspaces/pmacct/scripts/session_resume.sh` |
| **WORKSTATE.md** | 项目状态 | `/workspaces/pmacct/WORKSTATE.md` |

---

## 🎯 新Chat开场白模板

```
【恢复SAV IPFIX Hackathon】

项目: pmacct SAV字段完整解析
GitHub: https://github.com/Cq-zgclab/pmacct
上次commit: a00bc52 (2025-12-04)

已完成 (100%):
- ✅ 核心解析功能
- ✅ 所有4个模板验证 (901-904)
- ✅ IPv4/IPv6支持
- ✅ 双编码模式
- ✅ 文档完整

下一步:
Day 1: TCP传输支持 (~2小时)

请先运行: ./scripts/session_resume.sh
然后告诉我结果，我们继续Day 1任务。
```

---

## ⚡ 超快速恢复（30秒）

如果您熟悉项目，只需要：

```bash
cd /workspaces/pmacct
git pull origin main           # 同步远程更改
make clean && make src/nfacctd # 重新编译
./tests/my-SAV-ipfix-test/demo.sh  # 快速测试

# 看到3条SAV规则 → 开始Day 1任务
```

---

## 🆘 遇到问题？

### 问题: 恢复脚本失败

**查看详细文档**:
```bash
cat /workspaces/pmacct/SESSION_RESUME.md | grep -A10 "常见问题"
```

### 问题: 编译失败

**强制清理重编译**:
```bash
cd /workspaces/pmacct
make clean
rm -f src/nfacctd.o src/sav_parser.o
make src/nfacctd
```

### 问题: 测试失败

**查看调试日志**:
```bash
./src/nfacctd -f /tmp/nfacctd_test.conf -d > /tmp/debug.log 2>&1 &
# 发送测试消息
grep "SAV:" /tmp/debug.log
```

---

## 📞 GitHub仓库

**URL**: https://github.com/Cq-zgclab/pmacct

**最新状态**: 
- Branch: main
- Latest commit: a00bc52
- Status: All 12 commits pushed

---

**创建日期**: 2025-12-04  
**用途**: 1周以上间隔后快速恢复工作

**优先级**: 
1. 🥇 运行 `./scripts/session_resume.sh`
2. 🥈 阅读 `SESSION_RESUME.md`
3. 🥉 查看 `TODO_NEXT_WEEK.md`
