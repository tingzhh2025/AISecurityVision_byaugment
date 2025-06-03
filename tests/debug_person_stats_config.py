#!/usr/bin/env python3
"""
人员统计配置保存问题调试脚本
用于测试和验证人员统计功能的配置保存和加载机制
"""

import requests
import json
import time
import sqlite3
import sys
import os

# API配置
API_BASE = "http://localhost:8080/api"
DB_PATH = "aibox.db"

def check_database_config(camera_id):
    """检查数据库中的配置"""
    print(f"\n=== 检查数据库配置 (Camera: {camera_id}) ===")
    
    try:
        conn = sqlite3.connect(DB_PATH)
        cursor = conn.cursor()
        
        # 查询人员统计配置
        config_key = f"person_stats_{camera_id}"
        cursor.execute(
            "SELECT category, key, value, created_at, updated_at FROM config WHERE category = ? AND key = ?",
            ("person_statistics", config_key)
        )
        
        result = cursor.fetchone()
        if result:
            category, key, value, created_at, updated_at = result
            print(f"✓ 找到配置记录:")
            print(f"  Category: {category}")
            print(f"  Key: {key}")
            print(f"  Value: {value}")
            print(f"  Created: {created_at}")
            print(f"  Updated: {updated_at}")
            
            # 解析JSON配置
            try:
                config_data = json.loads(value)
                print(f"  Enabled: {config_data.get('enabled', 'N/A')}")
                print(f"  Gender Threshold: {config_data.get('gender_threshold', 'N/A')}")
                print(f"  Age Threshold: {config_data.get('age_threshold', 'N/A')}")
                return config_data
            except json.JSONDecodeError as e:
                print(f"✗ JSON解析失败: {e}")
                return None
        else:
            print(f"✗ 未找到配置记录 (key: {config_key})")
            return None
            
    except sqlite3.Error as e:
        print(f"✗ 数据库错误: {e}")
        return None
    finally:
        if conn:
            conn.close()

def test_api_endpoint(method, url, data=None):
    """测试API端点"""
    try:
        if method == "GET":
            response = requests.get(url, timeout=10)
        elif method == "POST":
            headers = {"Content-Type": "application/json"}
            response = requests.post(url, json=data, headers=headers, timeout=10)
        else:
            print(f"✗ 不支持的HTTP方法: {method}")
            return None
            
        print(f"{method} {url}")
        print(f"Status: {response.status_code}")
        
        if response.status_code == 200:
            try:
                json_data = response.json()
                print(f"Response: {json.dumps(json_data, indent=2, ensure_ascii=False)}")
                return json_data
            except json.JSONDecodeError:
                print(f"Response (text): {response.text}")
                return response.text
        else:
            print(f"Error: {response.text}")
            return None
            
    except requests.RequestException as e:
        print(f"✗ 请求失败: {e}")
        return None

def test_person_stats_workflow(camera_id):
    """测试完整的人员统计工作流程"""
    print(f"\n=== 测试人员统计工作流程 (Camera: {camera_id}) ===")
    
    # 1. 获取初始配置
    print("\n1. 获取初始配置:")
    initial_config = test_api_endpoint("GET", f"{API_BASE}/cameras/{camera_id}/person-stats/config")
    
    # 2. 检查数据库中的初始状态
    print("\n2. 检查数据库初始状态:")
    db_config_before = check_database_config(camera_id)
    
    # 3. 启用人员统计
    print("\n3. 启用人员统计:")
    enable_result = test_api_endpoint("POST", f"{API_BASE}/cameras/{camera_id}/person-stats/enable")
    
    # 4. 检查启用后的配置
    print("\n4. 检查启用后的配置:")
    config_after_enable = test_api_endpoint("GET", f"{API_BASE}/cameras/{camera_id}/person-stats/config")
    
    # 5. 检查数据库中启用后的状态
    print("\n5. 检查数据库启用后状态:")
    db_config_after_enable = check_database_config(camera_id)
    
    # 6. 更新配置参数
    print("\n6. 更新配置参数:")
    new_config = {
        "enabled": True,
        "gender_threshold": 0.85,
        "age_threshold": 0.75,
        "batch_size": 8,
        "enable_caching": True
    }
    update_result = test_api_endpoint("POST", f"{API_BASE}/cameras/{camera_id}/person-stats/config", new_config)
    
    # 7. 检查更新后的配置
    print("\n7. 检查更新后的配置:")
    config_after_update = test_api_endpoint("GET", f"{API_BASE}/cameras/{camera_id}/person-stats/config")
    
    # 8. 检查数据库中更新后的状态
    print("\n8. 检查数据库更新后状态:")
    db_config_after_update = check_database_config(camera_id)
    
    # 9. 禁用人员统计
    print("\n9. 禁用人员统计:")
    disable_result = test_api_endpoint("POST", f"{API_BASE}/cameras/{camera_id}/person-stats/disable")
    
    # 10. 检查禁用后的配置
    print("\n10. 检查禁用后的配置:")
    config_after_disable = test_api_endpoint("GET", f"{API_BASE}/cameras/{camera_id}/person-stats/config")
    
    # 11. 检查数据库中禁用后的状态
    print("\n11. 检查数据库禁用后状态:")
    db_config_after_disable = check_database_config(camera_id)
    
    # 分析结果
    print("\n=== 分析结果 ===")
    
    # 检查启用状态是否正确保存
    if db_config_after_enable and db_config_after_enable.get('enabled') == True:
        print("✓ 启用状态正确保存到数据库")
    else:
        print("✗ 启用状态未正确保存到数据库")
    
    # 检查配置更新是否正确保存
    if db_config_after_update:
        if (db_config_after_update.get('gender_threshold') == 0.85 and 
            db_config_after_update.get('age_threshold') == 0.75):
            print("✓ 配置参数正确保存到数据库")
        else:
            print("✗ 配置参数未正确保存到数据库")
    
    # 检查禁用状态是否正确保存
    if db_config_after_disable and db_config_after_disable.get('enabled') == False:
        print("✓ 禁用状态正确保存到数据库")
    else:
        print("✗ 禁用状态未正确保存到数据库")

def main():
    """主函数"""
    print("人员统计配置保存问题调试脚本")
    print("=" * 50)

    # 检查数据库文件是否存在
    if not os.path.exists(DB_PATH):
        print(f"✗ 数据库文件不存在: {DB_PATH}")
        sys.exit(1)
    else:
        print(f"✓ 数据库文件存在: {DB_PATH}")

    # 测试API连接
    print("\n=== 测试API连接 ===")
    try:
        response = requests.get(f"{API_BASE}/system/status", timeout=5)
        if response.status_code == 200:
            print("✓ API服务连接正常")
            print(f"  Status: {response.json()}")
        else:
            print(f"✗ API服务响应异常: {response.status_code}")
            sys.exit(1)
    except Exception as e:
        print(f"✗ 无法连接到API服务: {e}")
        sys.exit(1)

    # 使用测试摄像头ID
    camera_id = "test_camera"
    print(f"\n使用测试摄像头ID: {camera_id}")

    # 运行测试
    test_person_stats_workflow(camera_id)

    print("\n=== 调试完成 ===")

if __name__ == "__main__":
    main()
