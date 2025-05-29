#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
修复Logger.h包含路径的脚本
将错误的包含路径修正为正确的相对路径
"""

import os
import re
from pathlib import Path

def fix_logger_include_path(file_path: Path) -> bool:
    """修复单个文件中的Logger.h包含路径"""
    try:
        with open(file_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        original_content = content
        
        # 计算从当前文件到Logger.h的相对路径
        # 获取文件相对于src目录的路径
        relative_to_src = file_path.relative_to(Path('src'))
        
        # 计算需要返回到src目录的层级数
        depth = len(relative_to_src.parts) - 1  # 减1因为最后一个是文件名
        
        # 构建正确的相对路径
        if depth == 0:
            # 文件在src根目录
            correct_path = 'core/Logger.h'
        else:
            # 文件在子目录中，需要返回上级目录
            prefix = '../' * depth
            correct_path = f'{prefix}core/Logger.h'
        
        # 替换错误的包含路径
        patterns_to_fix = [
            r'#include\s*[<"]core/Logger\.h[>"]',
            r'#include\s*[<"]Logger\.h[>"]',
            r'#include\s*[<"]src/core/Logger\.h[>"]',
        ]
        
        modified = False
        for pattern in patterns_to_fix:
            if re.search(pattern, content):
                content = re.sub(pattern, f'#include "{correct_path}"', content)
                modified = True
        
        # 如果有修改，写回文件
        if modified and content != original_content:
            with open(file_path, 'w', encoding='utf-8') as f:
                f.write(content)
            print(f"✓ 修复了 {file_path} 的Logger.h包含路径 -> {correct_path}")
            return True
        
        return False
        
    except Exception as e:
        print(f"✗ 处理文件 {file_path} 时出错: {e}")
        return False

def main():
    print("🔧 修复Logger.h包含路径脚本")
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
            
        if fix_logger_include_path(file_path):
            fixed_count += 1
    
    print("=" * 50)
    print(f"✅ 修复完成！共修复了 {fixed_count} 个文件")
    
    return 0

if __name__ == "__main__":
    exit(main())
