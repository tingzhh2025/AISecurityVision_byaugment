#!/bin/bash

# 测试Logger替换效果的脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# 检查替换前的状态
check_before_replacement() {
    log_info "检查替换前的状态..."
    
    echo "std::cout 使用统计:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -c "std::cout" | grep -v ":0" | wc -l || echo "0"
    
    echo "std::cerr 使用统计:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -c "std::cerr" | grep -v ":0" | wc -l || echo "0"
    
    echo "Logger.h 包含统计:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -l "Logger.h" | wc -l || echo "0"
}

# 执行替换
run_replacement() {
    log_info "执行Logger替换..."
    
    # 首先尝试Python脚本
    if command -v python3 &> /dev/null; then
        log_info "使用Python智能替换脚本..."
        python3 smart_logger_replacer.py --src-dir src
    else
        log_warn "Python3未找到，使用Bash脚本..."
        chmod +x batch_replace_logger.sh
        ./batch_replace_logger.sh
    fi
}

# 检查替换后的状态
check_after_replacement() {
    log_info "检查替换后的状态..."
    
    echo "剩余 std::cout 使用:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -c "std::cout" | grep -v ":0" | wc -l || echo "0"
    
    echo "剩余 std::cerr 使用:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -c "std::cerr" | grep -v ":0" | wc -l || echo "0"
    
    echo "LOG_INFO 使用统计:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -c "LOG_INFO" | grep -v ":0" | wc -l || echo "0"
    
    echo "LOG_ERROR 使用统计:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -c "LOG_ERROR" | grep -v ":0" | wc -l || echo "0"
    
    echo "包含 Logger.h 的文件:"
    find src -name "*.cpp" -o -name "*.h" | xargs grep -l "Logger.h" | wc -l || echo "0"
}

# 测试编译
test_compilation() {
    log_info "测试编译..."
    
    if [ -d "build" ]; then
        cd build
        if make -j$(nproc) 2>&1 | tee ../compilation_test.log; then
            log_info "✓ 编译成功!"
        else
            log_error "✗ 编译失败，请检查 compilation_test.log"
            cd ..
            return 1
        fi
        cd ..
    else
        log_warn "build目录不存在，跳过编译测试"
    fi
}

# 生成对比报告
generate_comparison_report() {
    log_info "生成对比报告..."
    
    cat > replacement_comparison.txt << EOF
=== Logger替换对比报告 ===
生成时间: $(date)

=== 替换前后对比 ===
EOF
    
    echo "检查剩余的std::cout/cerr使用:" >> replacement_comparison.txt
    find src -name "*.cpp" -o -name "*.h" | xargs grep -n "std::cout\|std::cerr" >> replacement_comparison.txt 2>/dev/null || echo "无剩余使用" >> replacement_comparison.txt
    
    echo "" >> replacement_comparison.txt
    echo "=== Logger使用统计 ===" >> replacement_comparison.txt
    echo "LOG_INFO使用的文件:" >> replacement_comparison.txt
    find src -name "*.cpp" -o -name "*.h" | xargs grep -l "LOG_INFO" >> replacement_comparison.txt 2>/dev/null || echo "无" >> replacement_comparison.txt
    
    echo "" >> replacement_comparison.txt
    echo "LOG_ERROR使用的文件:" >> replacement_comparison.txt
    find src -name "*.cpp" -o -name "*.h" | xargs grep -l "LOG_ERROR" >> replacement_comparison.txt 2>/dev/null || echo "无" >> replacement_comparison.txt
    
    echo "" >> replacement_comparison.txt
    echo "=== 包含Logger.h的文件 ===" >> replacement_comparison.txt
    find src -name "*.cpp" -o -name "*.h" | xargs grep -l "Logger.h" >> replacement_comparison.txt 2>/dev/null || echo "无" >> replacement_comparison.txt
    
    log_info "对比报告已生成: replacement_comparison.txt"
}

# 主函数
main() {
    echo "=== Logger替换测试脚本 ==="
    echo "开始时间: $(date)"
    echo ""
    
    # 检查替换前状态
    check_before_replacement
    
    echo ""
    read -p "是否继续执行替换? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        log_info "用户取消操作"
        exit 0
    fi
    
    # 执行替换
    run_replacement
    
    echo ""
    log_info "替换完成，检查结果..."
    
    # 检查替换后状态
    check_after_replacement
    
    # 生成对比报告
    generate_comparison_report
    
    # 测试编译
    echo ""
    read -p "是否测试编译? (y/N): " -n 1 -r
    echo
    if [[ $REPLY =~ ^[Yy]$ ]]; then
        test_compilation
    fi
    
    echo ""
    echo "=== 测试完成 ==="
    echo "结束时间: $(date)"
    echo ""
    echo "生成的文件:"
    echo "  - logger_replacement_report.txt (详细替换报告)"
    echo "  - replacement_comparison.txt (对比报告)"
    echo "  - compilation_test.log (编译测试日志，如果执行了编译测试)"
    echo ""
    echo "如果替换有问题，可以从备份目录恢复:"
    ls -d backup_* 2>/dev/null | tail -1 | xargs -I {} echo "  cp -r {} src"
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
