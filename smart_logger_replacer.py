#!/usr/bin/env python3
# -*- coding: utf-8 -*-

"""
智能Logger替换脚本
功能：将C++代码中的std::cout和std::cerr智能替换为Logger系统
作者：AI Assistant
"""

import os
import re
import sys
import shutil
import argparse
from pathlib import Path
from datetime import datetime
from typing import List, Tuple, Dict

class LoggerReplacer:
    def __init__(self, src_dir: str = "src", backup_dir: str = None):
        self.src_dir = Path(src_dir)
        self.backup_dir = Path(backup_dir) if backup_dir else Path(f"backup_logger_{datetime.now().strftime('%Y%m%d_%H%M%S')}")
        self.logger_header = '#include "core/Logger.h"'
        self.stats = {
            'total_files': 0,
            'modified_files': 0,
            'cout_replacements': 0,
            'cerr_replacements': 0,
            'header_additions': 0
        }
        
        # 替换规则
        self.replacement_rules = [
            # 复杂的多行模式
            (r'std::cout\s*<<\s*"([^"]*\[[^\]]+\][^"]*)"([^;]*?)<<\s*std::endl\s*;', 
             r'LOG_INFO() << "\1"\2;'),
            (r'std::cerr\s*<<\s*"([^"]*\[[^\]]+\][^"]*)"([^;]*?)<<\s*std::endl\s*;', 
             r'LOG_ERROR() << "\1"\2;'),
            
            # 标准的单行模式
            (r'std::cout\s*<<\s*"([^"]*)"([^;]*?)<<\s*std::endl\s*;', 
             r'LOG_INFO() << "\1"\2;'),
            (r'std::cerr\s*<<\s*"([^"]*)"([^;]*?)<<\s*std::endl\s*;', 
             r'LOG_ERROR() << "\1"\2;'),
            
            # 不带std::endl的模式
            (r'std::cout\s*<<\s*"([^"]*)"([^;]*?);', 
             r'LOG_INFO() << "\1"\2;'),
            (r'std::cerr\s*<<\s*"([^"]*)"([^;]*?);', 
             r'LOG_ERROR() << "\1"\2;'),
            
            # 变量输出模式
            (r'std::cout\s*<<\s*([^";\n]+)\s*<<\s*std::endl\s*;', 
             r'LOG_INFO() << \1;'),
            (r'std::cerr\s*<<\s*([^";\n]+)\s*<<\s*std::endl\s*;', 
             r'LOG_ERROR() << \1;'),
            
            # 简单变量输出
            (r'std::cout\s*<<\s*([^;\n]+)\s*;', 
             r'LOG_INFO() << \1;'),
            (r'std::cerr\s*<<\s*([^;\n]+)\s*;', 
             r'LOG_ERROR() << \1;'),
        ]
        
        # 智能级别判断规则
        self.level_patterns = [
            (r'error|Error|ERROR|fail|Fail|FAIL|exception|Exception', 'LOG_ERROR'),
            (r'warn|Warn|WARN|warning|Warning', 'LOG_WARN'),
            (r'debug|Debug|DEBUG', 'LOG_DEBUG'),
            (r'trace|Trace|TRACE', 'LOG_TRACE'),
            (r'fatal|Fatal|FATAL|critical|Critical', 'LOG_FATAL'),
        ]
    
    def create_backup(self) -> bool:
        """创建源代码备份"""
        try:
            if self.src_dir.exists():
                shutil.copytree(self.src_dir, self.backup_dir)
                print(f"✓ 备份创建成功: {self.backup_dir}")
                return True
            else:
                print(f"✗ 源目录不存在: {self.src_dir}")
                return False
        except Exception as e:
            print(f"✗ 备份创建失败: {e}")
            return False
    
    def has_logger_include(self, content: str) -> bool:
        """检查是否已包含Logger头文件"""
        return bool(re.search(r'#include\s*[<"].*Logger\.h[>"]', content))
    
    def add_logger_include(self, content: str) -> str:
        """添加Logger头文件包含"""
        if self.has_logger_include(content):
            return content
        
        # 查找最后一个#include的位置
        include_pattern = r'(#include\s*[<"][^>"\n]*[>"]\s*\n)'
        includes = list(re.finditer(include_pattern, content))
        
        if includes:
            # 在最后一个include之后插入
            last_include = includes[-1]
            insert_pos = last_include.end()
            new_content = (content[:insert_pos] + 
                          f'{self.logger_header}\n' + 
                          content[insert_pos:])
        else:
            # 如果没有include，在文件开头添加
            new_content = f'{self.logger_header}\n\n{content}'
        
        self.stats['header_additions'] += 1
        return new_content
    
    def determine_log_level(self, text: str) -> str:
        """根据文本内容智能判断日志级别"""
        text_lower = text.lower()
        
        for pattern, level in self.level_patterns:
            if re.search(pattern, text_lower):
                return level
        
        return 'LOG_INFO'  # 默认级别
    
    def smart_replace_cout_cerr(self, content: str) -> Tuple[str, int, int]:
        """智能替换std::cout和std::cerr"""
        original_content = content
        cout_count = 0
        cerr_count = 0
        
        # 首先处理复杂的多行情况
        multiline_cout_pattern = r'std::cout\s*<<\s*([^;]+?)\s*<<\s*std::endl\s*;'
        multiline_cerr_pattern = r'std::cerr\s*<<\s*([^;]+?)\s*<<\s*std::endl\s*;'
        
        def replace_cout(match):
            nonlocal cout_count
            cout_count += 1
            content_part = match.group(1).strip()
            
            # 智能判断日志级别
            log_level = self.determine_log_level(content_part)
            if log_level == 'LOG_INFO':  # 如果是默认级别，保持LOG_INFO
                return f'LOG_INFO() << {content_part};'
            else:
                return f'{log_level}() << {content_part};'
        
        def replace_cerr(match):
            nonlocal cerr_count
            cerr_count += 1
            content_part = match.group(1).strip()
            
            # std::cerr通常是错误，但也可能是警告
            log_level = self.determine_log_level(content_part)
            if log_level == 'LOG_INFO':  # 如果没有特殊标识，默认为ERROR
                log_level = 'LOG_ERROR'
            
            return f'{log_level}() << {content_part};'
        
        # 应用替换
        content = re.sub(multiline_cout_pattern, replace_cout, content, flags=re.MULTILINE | re.DOTALL)
        content = re.sub(multiline_cerr_pattern, replace_cerr, content, flags=re.MULTILINE | re.DOTALL)
        
        # 处理简单情况
        simple_cout_pattern = r'std::cout\s*<<\s*([^;\n]+)\s*;'
        simple_cerr_pattern = r'std::cerr\s*<<\s*([^;\n]+)\s*;'
        
        content = re.sub(simple_cout_pattern, replace_cout, content)
        content = re.sub(simple_cerr_pattern, replace_cerr, content)
        
        # 处理剩余的std::cout和std::cerr（兼容性替换）
        remaining_cout = len(re.findall(r'std::cout', content))
        remaining_cerr = len(re.findall(r'std::cerr', content))
        
        if remaining_cout > 0:
            content = re.sub(r'std::cout', 'LOGGER_OUT', content)
            cout_count += remaining_cout
        
        if remaining_cerr > 0:
            content = re.sub(r'std::cerr', 'LOGGER_ERR', content)
            cerr_count += remaining_cerr
        
        return content, cout_count, cerr_count
    
    def clean_up_formatting(self, content: str) -> str:
        """清理格式，移除多余的空行等"""
        # 移除多余的空行
        content = re.sub(r'\n\s*\n\s*\n', '\n\n', content)
        
        # 确保文件以换行符结尾
        if not content.endswith('\n'):
            content += '\n'
        
        return content
    
    def process_file(self, file_path: Path) -> bool:
        """处理单个文件"""
        try:
            # 跳过Logger相关文件
            if file_path.name in ['Logger.cpp', 'Logger.h']:
                print(f"⏭  跳过Logger文件: {file_path}")
                return False
            
            # 读取文件内容
            with open(file_path, 'r', encoding='utf-8') as f:
                original_content = f.read()
            
            # 检查是否包含std::cout或std::cerr
            if not re.search(r'std::(cout|cerr)', original_content):
                return False
            
            print(f"🔄 处理文件: {file_path}")
            
            # 执行替换
            content, cout_count, cerr_count = self.smart_replace_cout_cerr(original_content)
            
            # 添加Logger头文件
            if cout_count > 0 or cerr_count > 0:
                content = self.add_logger_include(content)
            
            # 清理格式
            content = self.clean_up_formatting(content)
            
            # 如果有修改，写回文件
            if content != original_content:
                with open(file_path, 'w', encoding='utf-8') as f:
                    f.write(content)
                
                self.stats['cout_replacements'] += cout_count
                self.stats['cerr_replacements'] += cerr_count
                
                print(f"✓ 已更新: {file_path} (cout: {cout_count}, cerr: {cerr_count})")
                return True
            
            return False
            
        except Exception as e:
            print(f"✗ 处理文件失败 {file_path}: {e}")
            return False
    
    def find_cpp_files(self) -> List[Path]:
        """查找所有C++文件"""
        extensions = ['*.cpp', '*.h', '*.hpp', '*.cc', '*.cxx', '*.c++', '*.hxx', '*.h++']
        files = []
        
        for ext in extensions:
            files.extend(self.src_dir.rglob(ext))
        
        return sorted(files)
    
    def process_all_files(self) -> None:
        """处理所有文件"""
        cpp_files = self.find_cpp_files()
        self.stats['total_files'] = len(cpp_files)
        
        print(f"📁 找到 {len(cpp_files)} 个C++文件")
        print("=" * 50)
        
        for file_path in cpp_files:
            if self.process_file(file_path):
                self.stats['modified_files'] += 1
        
        print("=" * 50)
        print("✅ 处理完成!")
    
    def generate_report(self) -> None:
        """生成处理报告"""
        report_file = Path("logger_replacement_report.txt")
        
        with open(report_file, 'w', encoding='utf-8') as f:
            f.write("=== Logger替换报告 ===\n")
            f.write(f"生成时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n")
            f.write(f"源目录: {self.src_dir}\n")
            f.write(f"备份目录: {self.backup_dir}\n\n")
            
            f.write("=== 统计信息 ===\n")
            f.write(f"总文件数: {self.stats['total_files']}\n")
            f.write(f"修改文件数: {self.stats['modified_files']}\n")
            f.write(f"std::cout替换次数: {self.stats['cout_replacements']}\n")
            f.write(f"std::cerr替换次数: {self.stats['cerr_replacements']}\n")
            f.write(f"添加Logger.h次数: {self.stats['header_additions']}\n\n")
            
            # 检查剩余的std::cout/cerr
            remaining_files = []
            for file_path in self.find_cpp_files():
                try:
                    with open(file_path, 'r', encoding='utf-8') as file:
                        content = file.read()
                        if re.search(r'std::(cout|cerr)', content):
                            remaining_files.append(str(file_path))
                except:
                    pass
            
            f.write("=== 可能需要手动检查的文件 ===\n")
            if remaining_files:
                for file in remaining_files:
                    f.write(f"{file}\n")
            else:
                f.write("无\n")
        
        print(f"📊 报告已生成: {report_file}")
    
    def print_stats(self) -> None:
        """打印统计信息"""
        print("\n📊 处理统计:")
        print(f"   总文件数: {self.stats['total_files']}")
        print(f"   修改文件数: {self.stats['modified_files']}")
        print(f"   std::cout替换: {self.stats['cout_replacements']}")
        print(f"   std::cerr替换: {self.stats['cerr_replacements']}")
        print(f"   添加Logger.h: {self.stats['header_additions']}")

def main():
    parser = argparse.ArgumentParser(description='智能Logger替换脚本')
    parser.add_argument('--src-dir', default='src', help='源代码目录 (默认: src)')
    parser.add_argument('--backup-dir', help='备份目录 (默认: 自动生成)')
    parser.add_argument('--no-backup', action='store_true', help='不创建备份')
    
    args = parser.parse_args()
    
    print("🚀 智能Logger替换脚本")
    print("=" * 50)
    
    replacer = LoggerReplacer(args.src_dir, args.backup_dir)
    
    # 创建备份
    if not args.no_backup:
        if not replacer.create_backup():
            print("❌ 备份失败，退出")
            return 1
    
    # 处理文件
    replacer.process_all_files()
    
    # 打印统计
    replacer.print_stats()
    
    # 生成报告
    replacer.generate_report()
    
    print(f"\n💾 备份目录: {replacer.backup_dir}")
    print("🔧 建议运行以下命令测试编译:")
    print("   cd build && make")
    
    return 0

if __name__ == "__main__":
    sys.exit(main())
