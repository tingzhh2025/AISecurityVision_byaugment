#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
修复Logger命名空间问题的脚本
在包含Logger.h的文件中添加using namespace声明或者修改宏调用
"""

import os
import re
from pathlib import Path

def fix_logger_namespace_in_file(file_path: Path) -> bool:
    """修复单个文件中的Logger命名空间问题"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        
        # 检查是否包含Logger.h
        if '#include "../core/Logger.h"' not in content and '#include "core/Logger.h"' not in content:
            return False
        
        # 检查是否已经有using namespace声明
        if 'using namespace AISecurityVision;' in content:
            return False
        
        # 查找第一个Logger.h包含的位置
        logger_include_match = re.search(r'#include\s*[<"]([^<>"]*Logger\.h)[>"]', content)
        if not logger_include_match:
            return False
        
        # 在Logger.h包含之后添加using namespace声明
        logger_include_line = logger_include_match.group(0)
        using_namespace_line = logger_include_line + '\nusing namespace AISecurityVision;'
        
        content = content.replace(logger_include_line, using_namespace_line)
        
        # 如果有修改，写回文件
        if content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"✓ 修复了 {file_path} 的Logger命名空间问题")
            return True
        
        return False
        
    except Exception as e:
        print(f"✗ 处理文件 {file_path} 时出错: {e}")
        return False

def main():
    print("🔧 修复Logger命名空间问题脚本")
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
            
        if fix_logger_namespace_in_file(file_path):
            fixed_count += 1
    
    print("=" * 50)
    print(f"✅ 修复完成！共修复了 {fixed_count} 个文件")
    
    return 0

if __name__ == "__main__":
    exit(main())
