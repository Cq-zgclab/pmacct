#!/bin/bash
# SAV IPFIX Hackathon - Session恢复自动化脚本
# 用途: 快速验证环境并显示项目状态
# 使用: ./scripts/session_resume.sh

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 测试计数
PASSED=0
FAILED=0

echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  SAV IPFIX Session恢复检查${NC}"
echo -e "${BLUE}=========================================${NC}"
echo ""

cd /workspaces/pmacct

# ============================================================================
# 步骤1: Git状态检查
# ============================================================================
echo -e "${YELLOW}[1/6] Git状态检查...${NC}"
echo ""

# 1.1 工作区状态
echo "1.1 工作区状态:"
if git diff-index --quiet HEAD --; then
    echo -e "  ${GREEN}✓${NC} 工作区干净 (无未提交更改)"
    PASSED=$((PASSED + 1))
else
    echo -e "  ${RED}✗${NC} 工作区有未提交更改:"
    git status --short
    FAILED=$((FAILED + 1))
fi

# 1.2 最新提交
echo ""
echo "1.2 最近5次提交:"
git log --oneline -5 | sed 's/^/  /'

# 1.3 未推送commit
echo ""
echo "1.3 未推送到远程的commit:"
UNPUSHED=$(git log origin/main..HEAD --oneline | wc -l)
if [ "$UNPUSHED" -eq 0 ]; then
    echo -e "  ${GREEN}✓${NC} 所有commit已推送到远程"
    PASSED=$((PASSED + 1))
else
    echo -e "  ${YELLOW}⚠${NC} 有 $UNPUSHED 个commit未推送:"
    git log origin/main..HEAD --oneline | sed 's/^/    /'
    echo "  提示: 运行 'git push origin main' 推送"
fi

# ============================================================================
# 步骤2: 关键文件检查
# ============================================================================
echo ""
echo -e "${YELLOW}[2/6] 关键文件检查...${NC}"
echo ""

FILES=(
    "src/nfacctd.c"
    "src/sav_parser.c"
    "include/sav_parser.h"
    "TODO_NEXT_WEEK.md"
    "WORKSTATE.md"
    "HACKATHON_DEMO.md"
    "tests/my-SAV-ipfix-test/demo.sh"
    "tests/my-SAV-ipfix-test/scripts/send_ipfix_with_ip.py"
)

for file in "${FILES[@]}"; do
    if [ -f "$file" ]; then
        SIZE=$(ls -lh "$file" | awk '{print $5}')
        echo -e "  ${GREEN}✓${NC} $file (${SIZE})"
        PASSED=$((PASSED + 1))
    else
        echo -e "  ${RED}✗${NC} $file ${RED}未找到${NC}"
        FAILED=$((FAILED + 1))
    fi
done

# ============================================================================
# 步骤3: 编译检查
# ============================================================================
echo ""
echo -e "${YELLOW}[3/6] 编译检查...${NC}"
echo ""

if [ -f "src/nfacctd" ]; then
    # 检查是否需要重新编译
    NEED_REBUILD=0
    
    # 检查源文件是否比二进制新
    if [ "src/nfacctd.c" -nt "src/nfacctd" ] || \
       [ "src/sav_parser.c" -nt "src/nfacctd" ]; then
        NEED_REBUILD=1
    fi
    
    if [ $NEED_REBUILD -eq 1 ]; then
        echo -e "  ${YELLOW}⚠${NC} nfacctd存在但源文件已更新，建议重新编译"
        echo "     运行: make clean && make src/nfacctd"
    else
        # 测试执行
        VERSION=$(./src/nfacctd -V 2>&1 | head -1 || echo "ERROR")
        if [[ "$VERSION" == *"pmacct"* ]]; then
            echo -e "  ${GREEN}✓${NC} nfacctd已编译: $VERSION"
            PASSED=$((PASSED + 1))
        else
            echo -e "  ${RED}✗${NC} nfacctd无法执行: $VERSION"
            FAILED=$((FAILED + 1))
        fi
    fi
else
    echo -e "  ${RED}✗${NC} nfacctd未编译"
    echo "     运行: make src/nfacctd"
    FAILED=$((FAILED + 1))
fi

# 检查.o文件
echo ""
echo "  编译对象文件:"
if [ -f "src/nfacctd.o" ]; then
    echo -e "    ${GREEN}✓${NC} src/nfacctd.o"
else
    echo -e "    ${YELLOW}⚠${NC} src/nfacctd.o 不存在（首次编译正常）"
fi

if [ -f "src/sav_parser.o" ]; then
    echo -e "    ${GREEN}✓${NC} src/sav_parser.o"
else
    echo -e "    ${YELLOW}⚠${NC} src/sav_parser.o 不存在（首次编译正常）"
fi

# ============================================================================
# 步骤4: 进程检查
# ============================================================================
echo ""
echo -e "${YELLOW}[4/6] 进程检查...${NC}"
echo ""

if pgrep -x "nfacctd" > /dev/null; then
    echo -e "  ${YELLOW}⚠${NC} nfacctd正在运行:"
    ps aux | grep "[n]facctd" | sed 's/^/    /'
    echo "     测试前需要停止: pkill -9 nfacctd"
else
    echo -e "  ${GREEN}✓${NC} 无nfacctd进程运行（可以启动测试）"
    PASSED=$((PASSED + 1))
fi

# ============================================================================
# 步骤5: 端口检查
# ============================================================================
echo ""
echo -e "${YELLOW}[5/6] 端口检查...${NC}"
echo ""

# 检查9995端口（测试端口）
if netstat -tuln 2>/dev/null | grep -q ":9995"; then
    echo -e "  ${YELLOW}⚠${NC} 端口9995已被占用:"
    netstat -tuln | grep ":9995" | sed 's/^/    /'
    echo "     可能需要: lsof -i :9995 | tail -n +2 | awk '{print \$2}' | xargs kill"
else
    echo -e "  ${GREEN}✓${NC} 端口9995空闲（可用于测试）"
    PASSED=$((PASSED + 1))
fi

# ============================================================================
# 步骤6: 快速功能测试（可选）
# ============================================================================
echo ""
echo -e "${YELLOW}[6/6] 快速功能测试（可选）${NC}"
echo ""

read -p "是否运行快速功能测试? (y/N): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    echo ""
    echo "启动快速测试..."
    
    # 创建临时配置
    cat > /tmp/nfacctd_test.conf << 'EOF'
daemonize: false
debug: false
pidfile: /tmp/nfacctd_test.pid
nfacctd_ip: 0.0.0.0
nfacctd_port: 9995
nfacctd_pre_processing_checks: true
plugins: print
print_output: json
print_refresh_time: 1
EOF
    
    # 启动nfacctd
    ./src/nfacctd -f /tmp/nfacctd_test.conf > /tmp/nfacctd_test.log 2>&1 &
    NFACCTD_PID=$!
    echo "  nfacctd已启动 (PID: $NFACCTD_PID)"
    sleep 2
    
    # 检查启动
    if kill -0 $NFACCTD_PID 2>/dev/null; then
        echo -e "  ${GREEN}✓${NC} nfacctd启动成功"
        
        # 运行demo
        cd tests/my-SAV-ipfix-test
        if [ -x "./demo.sh" ]; then
            echo ""
            echo "  运行demo.sh..."
            ./demo.sh 2>&1 | grep -A3 "SAV Rules Parsed" || true
            
            # 检查结果
            sleep 1
            SAV_COUNT=$(grep -c "SAV: Rule" /tmp/nfacctd_test.log 2>/dev/null || echo "0")
            if [ "$SAV_COUNT" -gt 0 ]; then
                echo -e "  ${GREEN}✓${NC} 测试成功: 解析了 $SAV_COUNT 条SAV规则"
                PASSED=$((PASSED + 1))
                
                # 显示示例输出
                echo ""
                echo "  示例输出:"
                grep "SAV: Rule" /tmp/nfacctd_test.log 2>/dev/null | head -3 | sed 's/^/    /'
            else
                echo -e "  ${RED}✗${NC} 测试失败: 未检测到SAV规则解析"
                FAILED=$((FAILED + 1))
            fi
        else
            echo -e "  ${YELLOW}⚠${NC} demo.sh不存在或不可执行"
        fi
        cd /workspaces/pmacct
        
        # 停止nfacctd
        kill $NFACCTD_PID 2>/dev/null || true
        wait $NFACCTD_PID 2>/dev/null || true
        echo "  nfacctd已停止"
    else
        echo -e "  ${RED}✗${NC} nfacctd启动失败"
        echo "  查看日志: cat /tmp/nfacctd_test.log"
        FAILED=$((FAILED + 1))
    fi
else
    echo "  跳过功能测试"
fi

# ============================================================================
# 总结
# ============================================================================
echo ""
echo -e "${BLUE}=========================================${NC}"
echo -e "${BLUE}  检查完成${NC}"
echo -e "${BLUE}=========================================${NC}"
echo ""
echo -e "通过: ${GREEN}$PASSED${NC}"
echo -e "失败: ${RED}$FAILED${NC}"
echo ""

if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}✓ 环境就绪，可以开始工作！${NC}"
    echo ""
    echo "下一步:"
    echo "  1. 阅读 TODO_NEXT_WEEK.md 了解任务"
    echo "  2. 开始 Day 1: TCP支持实现"
    echo "     编辑: tests/my-SAV-ipfix-test/scripts/send_ipfix_with_ip.py"
    echo ""
    exit 0
else
    echo -e "${RED}✗ 发现问题，请先解决后再继续${NC}"
    echo ""
    echo "常见解决方案:"
    if [ -f "src/nfacctd" ]; then
        echo "  - 重新编译: make clean && make src/nfacctd"
    else
        echo "  - 首次编译: ./configure && make"
    fi
    echo "  - 停止旧进程: pkill -9 nfacctd"
    echo "  - 释放端口: lsof -i :9995 | tail -n +2 | awk '{print \$2}' | xargs kill"
    echo ""
    exit 1
fi
