#!/bin/bash

# 脚本：查找src目录下所有使用std::cout、std::cerr等标准输出函数的文件和行数
# 输出到根目录的replace_std.txt文件

OUTPUT_FILE="replace_std.txt"

# 清空输出文件
> "$OUTPUT_FILE"

echo "正在扫描src目录下的C++文件..."
echo "查找std::cout、std::cerr、std::clog、printf、fprintf等标准输出函数的使用..."
echo ""

# 添加文件头
echo "=== C++标准输出函数使用情况报告 ===" >> "$OUTPUT_FILE"
echo "扫描目录: src/" >> "$OUTPUT_FILE"
echo "生成时间: $(date)" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 定义要查找的模式
patterns=(
    "std::cout"
    "std::cerr" 
    "std::clog"
    "std::wcout"
    "std::wcerr"
    "std::wclog"
    "printf"
    "fprintf"
    "sprintf"
    "snprintf"
    "puts"
    "fputs"
)

# 统计变量
total_files=0
total_matches=0

# 遍历每个模式
for pattern in "${patterns[@]}"; do
    echo "=== 查找 $pattern ===" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
    
    # 使用grep查找匹配的文件和行数
    # -n: 显示行号
    # -H: 显示文件名
    # -r: 递归搜索
    # --include: 只包含C++文件
    matches=$(grep -nHr --include="*.cpp" --include="*.cc" --include="*.cxx" --include="*.c++" --include="*.hpp" --include="*.h" --include="*.hh" --include="*.hxx" --include="*.h++" "$pattern" src/ 2>/dev/null)
    
    if [ -n "$matches" ]; then
        echo "$matches" >> "$OUTPUT_FILE"
        
        # 统计匹配数量
        match_count=$(echo "$matches" | wc -l)
        file_count=$(echo "$matches" | cut -d: -f1 | sort -u | wc -l)
        
        echo "" >> "$OUTPUT_FILE"
        echo "找到 $match_count 处使用，涉及 $file_count 个文件" >> "$OUTPUT_FILE"
        
        total_matches=$((total_matches + match_count))
        total_files=$((total_files + file_count))
    else
        echo "未找到使用 $pattern 的代码" >> "$OUTPUT_FILE"
    fi
    
    echo "" >> "$OUTPUT_FILE"
    echo "----------------------------------------" >> "$OUTPUT_FILE"
    echo "" >> "$OUTPUT_FILE"
done

# 添加汇总信息
echo "=== 汇总统计 ===" >> "$OUTPUT_FILE"
echo "总计找到 $total_matches 处标准输出函数使用" >> "$OUTPUT_FILE"
echo "涉及约 $total_files 个文件（可能有重复计算）" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 生成按文件分组的汇总
echo "=== 按文件分组汇总 ===" >> "$OUTPUT_FILE"
echo "" >> "$OUTPUT_FILE"

# 获取所有匹配的文件，去重并排序
all_files=$(grep -lr --include="*.cpp" --include="*.cc" --include="*.cxx" --include="*.c++" --include="*.hpp" --include="*.h" --include="*.hh" --include="*.hxx" --include="*.h++" -E "(std::(cout|cerr|clog|wcout|wcerr|wclog)|printf|fprintf|sprintf|snprintf|puts|fputs)" src/ 2>/dev/null | sort -u)

if [ -n "$all_files" ]; then
    for file in $all_files; do
        echo "文件: $file" >> "$OUTPUT_FILE"
        
        # 为每个文件查找所有匹配的行
        for pattern in "${patterns[@]}"; do
            matches=$(grep -n "$pattern" "$file" 2>/dev/null)
            if [ -n "$matches" ]; then
                echo "  $pattern:" >> "$OUTPUT_FILE"
                echo "$matches" | sed 's/^/    /' >> "$OUTPUT_FILE"
            fi
        done
        echo "" >> "$OUTPUT_FILE"
    done
else
    echo "未找到任何使用标准输出函数的文件" >> "$OUTPUT_FILE"
fi

echo "扫描完成！结果已保存到 $OUTPUT_FILE"
echo "文件内容预览："
echo "----------------------------------------"
head -20 "$OUTPUT_FILE"
echo "----------------------------------------"
echo "查看完整结果请打开: $OUTPUT_FILE"
