#!/usr/bin/env python3
"""
测试人员统计功能配置持久化的脚本
用于验证前端配置保存和加载的问题
"""

import requests
import json
import time
import sys
import sqlite3
from typing import Dict, Any

class PersonStatsPersistenceTest:
    def __init__(self, base_url: str = "http://localhost:8080", camera_id: str = "camera_01"):
        self.base_url = base_url
        self.camera_id = camera_id
        self.api_url = f"{base_url}/api"
        
    def test_api_connectivity(self) -> bool:
        """测试API连接性"""
        try:
            response = requests.get(f"{self.api_url}/status", timeout=5)
            print(f"✅ API连接正常: {response.status_code}")
            return True
        except Exception as e:
            print(f"❌ API连接失败: {e}")
            return False
    
    def get_person_stats_config(self) -> Dict[str, Any]:
        """获取人员统计配置"""
        try:
            url = f"{self.api_url}/cameras/{self.camera_id}/person-stats/config"
            response = requests.get(url, timeout=10)
            
            print(f"GET {url}")
            print(f"状态码: {response.status_code}")
            print(f"响应头: {dict(response.headers)}")
            print(f"响应内容: {response.text}")
            
            if response.status_code == 200:
                data = response.json()
                # 检查是否有错误
                if 'error' in data:
                    print(f"❌ API返回错误: {data.get('error', 'Unknown error')}")
                    return {}
                # 检查是否有config字段
                elif 'config' in data:
                    return data['config']
                # 检查是否有success字段
                elif data.get('success'):
                    return data.get('data', {})
                else:
                    print(f"❌ API返回格式错误: {data}")
                    return {}
            else:
                print(f"❌ HTTP错误: {response.status_code}")
                
        except Exception as e:
            print(f"❌ 获取配置失败: {e}")
        
        return {}
    
    def update_person_stats_config(self, config: Dict[str, Any]) -> bool:
        """更新人员统计配置"""
        try:
            url = f"{self.api_url}/cameras/{self.camera_id}/person-stats/config"
            headers = {'Content-Type': 'application/json'}
            
            print(f"POST {url}")
            print(f"请求数据: {json.dumps(config, indent=2)}")
            
            response = requests.post(url, json=config, headers=headers, timeout=10)
            
            print(f"状态码: {response.status_code}")
            print(f"响应内容: {response.text}")
            
            if response.status_code == 200:
                data = response.json()
                if data.get('success') or data.get('status') == 'success':
                    print("✅ 配置更新成功")
                    return True
                else:
                    print(f"❌ API返回错误: {data.get('message', 'Unknown error')}")
            else:
                print(f"❌ HTTP错误: {response.status_code}")
                
        except Exception as e:
            print(f"❌ 更新配置失败: {e}")
        
        return False
    
    def check_database_config(self) -> Dict[str, Any]:
        """直接检查数据库中的配置"""
        try:
            conn = sqlite3.connect('aibox.db')
            cursor = conn.cursor()
            
            # 查询配置表
            config_key = f"person_stats_{self.camera_id}"
            cursor.execute(
                "SELECT value FROM config WHERE category = ? AND key = ?",
                ("person_statistics", config_key)
            )
            
            result = cursor.fetchone()
            conn.close()
            
            if result:
                config_json = result[0]
                print(f"📊 数据库配置: {config_json}")
                return json.loads(config_json)
            else:
                print("📊 数据库中未找到配置")
                return {}
                
        except Exception as e:
            print(f"❌ 数据库查询失败: {e}")
            return {}
    
    def run_persistence_test(self):
        """运行配置持久化测试"""
        print("🧪 开始人员统计配置持久化测试")
        print("=" * 50)
        
        # 1. 测试API连接
        if not self.test_api_connectivity():
            return False
        
        # 2. 获取当前配置
        print("\n📖 步骤1: 获取当前配置")
        current_config = self.get_person_stats_config()
        print(f"当前配置: {json.dumps(current_config, indent=2)}")
        
        # 3. 检查数据库中的配置
        print("\n📊 步骤2: 检查数据库配置")
        db_config = self.check_database_config()
        
        # 4. 更新配置（启用人员统计）
        print("\n✏️ 步骤3: 更新配置（启用人员统计）")
        test_config = {
            "enabled": True,
            "gender_threshold": 0.8,
            "age_threshold": 0.7,
            "batch_size": 6,
            "enable_caching": True
        }
        
        if not self.update_person_stats_config(test_config):
            return False
        
        # 5. 立即验证配置是否保存
        print("\n🔍 步骤4: 验证配置保存")
        time.sleep(1)  # 等待1秒确保保存完成
        
        updated_config = self.get_person_stats_config()
        print(f"更新后配置: {json.dumps(updated_config, indent=2)}")
        
        # 6. 检查数据库是否更新
        print("\n📊 步骤5: 检查数据库更新")
        updated_db_config = self.check_database_config()
        
        # 7. 验证配置一致性
        print("\n✅ 步骤6: 验证配置一致性")
        success = True
        
        for key, expected_value in test_config.items():
            api_value = updated_config.get(key)
            db_value = updated_db_config.get(key)
            
            print(f"  {key}:")
            print(f"    期望值: {expected_value}")
            print(f"    API返回: {api_value}")
            print(f"    数据库值: {db_value}")
            
            if api_value != expected_value:
                print(f"    ❌ API值不匹配")
                success = False
            elif db_value != expected_value:
                print(f"    ❌ 数据库值不匹配")
                success = False
            else:
                print(f"    ✅ 一致")
        
        # 8. 测试禁用配置
        print("\n🔄 步骤7: 测试禁用配置")
        disable_config = test_config.copy()
        disable_config["enabled"] = False
        
        if self.update_person_stats_config(disable_config):
            time.sleep(1)
            final_config = self.get_person_stats_config()
            final_db_config = self.check_database_config()
            
            print(f"禁用后API配置: {json.dumps(final_config, indent=2)}")
            print(f"禁用后数据库配置: {json.dumps(final_db_config, indent=2)}")
            
            if final_config.get("enabled") != False or final_db_config.get("enabled") != False:
                print("❌ 禁用配置失败")
                success = False
            else:
                print("✅ 禁用配置成功")
        
        print("\n" + "=" * 50)
        if success:
            print("🎉 配置持久化测试通过")
        else:
            print("❌ 配置持久化测试失败")
        
        return success

def main():
    if len(sys.argv) > 1:
        camera_id = sys.argv[1]
    else:
        camera_id = "camera_01"
    
    print(f"测试摄像头: {camera_id}")
    
    tester = PersonStatsPersistenceTest(camera_id=camera_id)
    success = tester.run_persistence_test()
    
    sys.exit(0 if success else 1)

if __name__ == "__main__":
    main()
