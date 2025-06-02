#!/bin/bash

# 批量替换脚本：将src目录下所有C++文件中的std::cout和std::cerr替换为Logger
# 作者：AI Assistant
# 用途：统一项目日志系统

set -e  # 遇到错误立即退出

# 配置变量
SRC_DIR="src"
BACKUP_DIR="backup_before_logger_$(date +%Y%m%d_%H%M%S)"
LOG_FILE="logger_replacement.log"
LOGGER_HEADER='#include "core/Logger.h"'

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1" | tee -a "$LOG_FILE"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1" | tee -a "$LOG_FILE"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1" | tee -a "$LOG_FILE"
}

log_debug() {
    echo -e "${BLUE}[DEBUG]${NC} $1" | tee -a "$LOG_FILE"
}

# 检查依赖
check_dependencies() {
    log_info "检查依赖工具..."
    
    if ! command -v sed &> /dev/null; then
        log_error "sed 命令未找到"
        exit 1
    fi
    
    if ! command -v find &> /dev/null; then
        log_error "find 命令未找到"
        exit 1
    fi
    
    log_info "依赖检查完成"
}

# 创建备份
create_backup() {
    log_info "创建备份目录: $BACKUP_DIR"
    
    if [ -d "$SRC_DIR" ]; then
        cp -r "$SRC_DIR" "$BACKUP_DIR"
        log_info "源代码已备份到: $BACKUP_DIR"
    else
        log_error "源代码目录 $SRC_DIR 不存在"
        exit 1
    fi
}

# 检查文件是否已包含Logger头文件
has_logger_include() {
    local file="$1"
    grep -q '#include.*Logger\.h' "$file"
}

# 添加Logger头文件包含
add_logger_include() {
    local file="$1"
    
    if has_logger_include "$file"; then
        log_debug "文件 $file 已包含Logger.h"
        return 0
    fi
    
    # 查找合适的插入位置（在其他#include之后）
    if grep -q '^#include' "$file"; then
        # 在最后一个#include之后插入
        sed -i '/^#include.*$/a\
'"$LOGGER_HEADER" "$file"
        log_debug "已在 $file 中添加Logger.h包含"
    else
        # 如果没有其他include，在文件开头添加
        sed -i '1i\
'"$LOGGER_HEADER"'
' "$file"
        log_debug "已在 $file 文件开头添加Logger.h包含"
    fi
}

# 智能替换函数
smart_replace() {
    local file="$1"
    local original_content
    local modified=false
    
    log_debug "处理文件: $file"
    
    # 读取原始内容
    original_content=$(cat "$file")
    
    # 创建临时文件
    local temp_file=$(mktemp)
    cp "$file" "$temp_file"
    
    # 替换规则数组
    declare -A replacements=(
        # std::cout 替换规则
        ['std::cout << "\[([^]]+)\] ([^"]+)" << std::endl;']='LOG_INFO() << "\2";'
        ['std::cout << "\[([^]]+)\] ([^"]+)" << std::endl']='LOG_INFO() << "\2";'
        ['std::cout << "([^"]*)" << std::endl;']='LOG_INFO() << "\1";'
        ['std::cout << "([^"]*)" << std::endl']='LOG_INFO() << "\1";'
        ['std::cout << ([^;]+) << std::endl;']='LOG_INFO() << \1;'
        ['std::cout << ([^;]+) << std::endl']='LOG_INFO() << \1;'
        ['std::cout << ([^;]+);']='LOG_INFO() << \1;'
        ['std::cout']='LOGGER_OUT'
        
        # std::cerr 替换规则
        ['std::cerr << "\[([^]]+)\] ([^"]+)" << std::endl;']='LOG_ERROR() << "\2";'
        ['std::cerr << "\[([^]]+)\] ([^"]+)" << std::endl']='LOG_ERROR() << "\2";'
        ['std::cerr << "([^"]*)" << std::endl;']='LOG_ERROR() << "\1";'
        ['std::cerr << "([^"]*)" << std::endl']='LOG_ERROR() << "\1";'
        ['std::cerr << ([^;]+) << std::endl;']='LOG_ERROR() << \1;'
        ['std::cerr << ([^;]+) << std::endl']='LOG_ERROR() << \1;'
        ['std::cerr << ([^;]+);']='LOG_ERROR() << \1;'
        ['std::cerr']='LOGGER_ERR'
    )
    
    # 应用替换规则
    for pattern in "${!replacements[@]}"; do
        replacement="${replacements[$pattern]}"
        
        # 使用sed进行替换
        if sed -i "s|$pattern|$replacement|g" "$temp_file"; then
            # 检查是否有实际修改
            if ! cmp -s "$file" "$temp_file"; then
                modified=true
                log_debug "应用替换规则: $pattern -> $replacement"
            fi
        fi
    done
    
    # 如果有修改，更新原文件
    if [ "$modified" = true ]; then
        cp "$temp_file" "$file"
        log_info "已更新文件: $file"
        
        # 添加Logger头文件
        add_logger_include "$file"
        
        return 0
    else
        log_debug "文件无需修改: $file"
        rm "$temp_file"
        return 1
    fi
    
    rm "$temp_file"
}

# 高级替换函数（处理复杂情况）
advanced_replace() {
    local file="$1"
    local temp_file=$(mktemp)
    local modified=false
    
    cp "$file" "$temp_file"
    
    # 处理多行std::cout语句
    # 例如: std::cout << "message" 
    #              << variable << std::endl;
    python3 << 'EOF' "$temp_file"
import sys
import re

def process_file(filename):
    with open(filename, 'r', encoding='utf-8') as f:
        content = f.read()
    
    original_content = content
    
    # 处理多行std::cout
    # 匹配 std::cout << ... << std::endl; 的模式
    cout_pattern = r'std::cout\s*<<\s*([^;]+?)\s*<<\s*std::endl\s*;'
    content = re.sub(cout_pattern, r'LOG_INFO() << \1;', content, flags=re.MULTILINE | re.DOTALL)
    
    # 处理多行std::cerr
    cerr_pattern = r'std::cerr\s*<<\s*([^;]+?)\s*<<\s*std::endl\s*;'
    content = re.sub(cerr_pattern, r'LOG_ERROR() << \1;', content, flags=re.MULTILINE | re.DOTALL)
    
    # 处理简单的std::cout << "message";
    simple_cout = r'std::cout\s*<<\s*([^;]+)\s*;'
    content = re.sub(simple_cout, r'LOG_INFO() << \1;', content)
    
    # 处理简单的std::cerr << "message";
    simple_cerr = r'std::cerr\s*<<\s*([^;]+)\s*;'
    content = re.sub(simple_cerr, r'LOG_ERROR() << \1;', content)
    
    # 清理多余的空行
    content = re.sub(r'\n\s*\n\s*\n', '\n\n', content)
    
    if content != original_content:
        with open(filename, 'w', encoding='utf-8') as f:
            f.write(content)
        return True
    return False

if __name__ == "__main__":
    filename = sys.argv[1]
    modified = process_file(filename)
    sys.exit(0 if modified else 1)
EOF
    
    if [ $? -eq 0 ]; then
        cp "$temp_file" "$file"
        modified=true
        log_debug "应用高级替换规则到: $file"
    fi
    
    rm "$temp_file"
    return $([ "$modified" = true ] && echo 0 || echo 1)
}

# 处理单个文件
process_file() {
    local file="$1"
    local basename=$(basename "$file")
    
    # 跳过Logger相关文件
    if [[ "$basename" == "Logger.cpp" || "$basename" == "Logger.h" ]]; then
        log_debug "跳过Logger文件: $file"
        return 0
    fi
    
    # 检查文件是否包含std::cout或std::cerr
    if ! grep -q -E "(std::cout|std::cerr)" "$file"; then
        log_debug "文件不包含std::cout或std::cerr: $file"
        return 0
    fi
    
    log_info "处理文件: $file"
    
    # 首先尝试智能替换
    if smart_replace "$file"; then
        log_info "✓ 智能替换完成: $file"
    else
        # 如果智能替换没有修改，尝试高级替换
        if advanced_replace "$file"; then
            add_logger_include "$file"
            log_info "✓ 高级替换完成: $file"
        else
            log_warn "文件可能需要手动处理: $file"
        fi
    fi
}

# 主处理函数
main_process() {
    log_info "开始批量替换处理..."
    
    local total_files=0
    local processed_files=0
    
    # 查找所有C++源文件和头文件
    while IFS= read -r -d '' file; do
        ((total_files++))
        
        if process_file "$file"; then
            ((processed_files++))
        fi
        
    done < <(find "$SRC_DIR" -type f \( -name "*.cpp" -o -name "*.h" -o -name "*.hpp" -o -name "*.cc" -o -name "*.cxx" \) -print0)
    
    log_info "处理完成！"
    log_info "总文件数: $total_files"
    log_info "已处理文件数: $processed_files"
}

# 生成报告
generate_report() {
    local report_file="logger_replacement_report.txt"
    
    log_info "生成替换报告: $report_file"
    
    cat > "$report_file" << EOF
=== Logger替换报告 ===
生成时间: $(date)
备份目录: $BACKUP_DIR
日志文件: $LOG_FILE

=== 替换统计 ===
EOF
    
    # 统计替换情况
    echo "std::cout 替换统计:" >> "$report_file"
    find "$SRC_DIR" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs grep -l "LOG_INFO\|LOGGER_OUT" | wc -l >> "$report_file"
    
    echo "std::cerr 替换统计:" >> "$report_file"
    find "$SRC_DIR" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs grep -l "LOG_ERROR\|LOGGER_ERR" | wc -l >> "$report_file"
    
    echo "" >> "$report_file"
    echo "=== 包含Logger.h的文件 ===" >> "$report_file"
    find "$SRC_DIR" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs grep -l "Logger\.h" >> "$report_file"
    
    echo "" >> "$report_file"
    echo "=== 可能需要手动检查的文件 ===" >> "$report_file"
    find "$SRC_DIR" -name "*.cpp" -o -name "*.h" -o -name "*.hpp" | xargs grep -l "std::cout\|std::cerr" >> "$report_file" 2>/dev/null || echo "无" >> "$report_file"
    
    log_info "报告已生成: $report_file"
}

# 主函数
main() {
    echo "=== Logger批量替换脚本 ==="
    echo "开始时间: $(date)"
    echo ""
    
    # 清空日志文件
    > "$LOG_FILE"
    
    # 检查依赖
    check_dependencies
    
    # 创建备份
    create_backup
    
    # 主处理
    main_process
    
    # 生成报告
    generate_report
    
    echo ""
    echo "=== 替换完成 ==="
    echo "结束时间: $(date)"
    echo "备份目录: $BACKUP_DIR"
    echo "日志文件: $LOG_FILE"
    echo "报告文件: logger_replacement_report.txt"
    echo ""
    echo "请检查替换结果，如有问题可从备份目录恢复。"
    echo "建议运行以下命令测试编译："
    echo "  cd build && make"
}

# 脚本入口
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    main "$@"
fi
