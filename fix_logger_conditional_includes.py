#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
修复Logger.h在条件编译块内的问题
将Logger.h移到条件编译块外面，确保总是被包含
"""

import os
import re
from pathlib import Path

def fix_conditional_logger_include(file_path: Path) -> bool:
    """修复单个文件中的条件Logger.h包含问题"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        
        # 查找在条件编译块内的Logger.h包含
        # 匹配模式：#ifndef 或 #ifdef 后面包含Logger.h的情况
        patterns = [
            # 匹配 #ifndef DISABLE_OPENCV_DNN 内的Logger.h
            r'(#ifndef\s+DISABLE_OPENCV_DNN[^#]*?)#include\s*[<"]([^<>"]*Logger\.h)[>"]([^#]*?#endif)',
            # 匹配其他条件编译块内的Logger.h
            r'(#if(?:def|ndef)?\s+[^#]*?)#include\s*[<"]([^<>"]*Logger\.h)[>"]([^#]*?#endif)',
        ]
        
        for pattern in patterns:
            matches = re.finditer(pattern, content, re.MULTILINE | re.DOTALL)
            for match in matches:
                before_block = match.group(1)
                logger_path = match.group(2)
                after_block = match.group(3)
                
                # 重构：将Logger.h移到条件编译块前面
                # 首先移除原来的Logger.h包含
                new_block = before_block + after_block
                
                # 然后在文件开头添加Logger.h包含
                # 查找第一个#include的位置
                first_include_match = re.search(r'(#include\s*[<"][^<>"]*[>"])', content)
                if first_include_match:
                    # 在第一个include之后添加Logger.h
                    first_include = first_include_match.group(1)
                    logger_include = f'#include "{logger_path}"'
                    
                    # 检查是否已经存在Logger.h包含
                    if logger_include not in content:
                        content = content.replace(first_include, first_include + '\n' + logger_include)
                
                # 替换原来的条件编译块
                content = content.replace(match.group(0), new_block)
                
                print(f"✓ 修复了 {file_path} 中的条件Logger.h包含")
                break
        
        # 如果有修改，写回文件
        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            return True
        
        return False
        
    except Exception as e:
        print(f"✗ 处理文件 {file_path} 时出错: {e}")
        return False

def main():
    print("🔧 修复Logger.h条件包含问题脚本")
    print("=" * 50)
    
    src_dir = Path('src')
    if not src_dir.exists():
        print("❌ src目录不存在")
        return 1
    
    # 查找所有C++文件
    cpp_files = []
    for ext in ['*.cpp', '*.h', '*.hpp', '*.cc', '*.cxx']:
        cpp_files.extend(src_dir.rglob(ext))
    
    print(f"📁 找到 {len(cpp_files)} 个C++文件")
    
    fixed_count = 0
    for file_path in cpp_files:
        # 跳过Logger文件本身
        if file_path.name in ['Logger.cpp', 'Logger.h']:
            continue
            
        if fix_conditional_logger_include(file_path):
            fixed_count += 1
    
    print("=" * 50)
    print(f"✅ 修复完成！共修复了 {fixed_count} 个文件")
    
    return 0

if __name__ == "__main__":
    exit(main())
