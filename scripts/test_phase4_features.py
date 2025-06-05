#!/usr/bin/env python3
"""
AI Security Vision System - Phase 4 Features Test

这个脚本用于测试第四阶段的核心功能：
- API安全加固验证
- 性能优化验证
- 文档生成验证
- 监控功能验证
"""

import os
import sys
import json
import time
import requests
import subprocess
from datetime import datetime
from typing import Dict, List, Any

class Phase4Tester:
    """第四阶段功能测试器"""
    
    def __init__(self, api_url: str = "http://localhost:8080"):
        self.api_url = api_url
        self.test_results = []
        
    def log_test(self, test_name: str, success: bool, message: str = "", details: Dict = None):
        """记录测试结果"""
        result = {
            "test_name": test_name,
            "success": success,
            "message": message,
            "details": details or {},
            "timestamp": datetime.now().isoformat()
        }
        self.test_results.append(result)
        
        status = "✅ PASS" if success else "❌ FAIL"
        print(f"   {status} {test_name}: {message}")
        
    def test_api_connectivity(self) -> bool:
        """测试API连接性"""
        print("🔗 Testing API connectivity...")
        
        try:
            response = requests.get(f"{self.api_url}/api/system/status", timeout=5)
            if response.status_code == 200:
                data = response.json()
                success = data.get("success", False)
                self.log_test("API Connectivity", success, f"Status: {response.status_code}")
                return success
            else:
                self.log_test("API Connectivity", False, f"HTTP {response.status_code}")
                return False
        except Exception as e:
            self.log_test("API Connectivity", False, f"Connection error: {e}")
            return False
    
    def test_rate_limiting(self) -> bool:
        """测试API限流功能"""
        print("🚦 Testing API rate limiting...")
        
        try:
            # 发送大量请求测试限流
            blocked_count = 0
            total_requests = 50
            
            for i in range(total_requests):
                try:
                    response = requests.get(f"{self.api_url}/api/system/status", timeout=1)
                    if response.status_code == 429:  # Too Many Requests
                        blocked_count += 1
                except:
                    pass
            
            # 如果有请求被限流，说明限流功能工作正常
            success = blocked_count > 0
            self.log_test("Rate Limiting", success, 
                         f"Blocked {blocked_count}/{total_requests} requests",
                         {"blocked_count": blocked_count, "total_requests": total_requests})
            return success
            
        except Exception as e:
            self.log_test("Rate Limiting", False, f"Test error: {e}")
            return False
    
    def test_input_validation(self) -> bool:
        """测试输入验证功能"""
        print("🛡️  Testing input validation...")
        
        try:
            # 测试XSS攻击防护
            xss_payload = {"id": "<script>alert('xss')</script>", "name": "test"}
            response = requests.post(f"{self.api_url}/api/cameras", 
                                   json=xss_payload, timeout=5)
            
            # 应该返回400错误或者过滤掉恶意内容
            xss_blocked = response.status_code == 400 or "script" not in str(response.text)
            
            # 测试SQL注入防护
            sql_payload = {"id": "test'; DROP TABLE cameras; --", "name": "test"}
            response = requests.post(f"{self.api_url}/api/cameras", 
                                   json=sql_payload, timeout=5)
            
            # 应该返回400错误或者过滤掉恶意内容
            sql_blocked = response.status_code == 400 or "DROP" not in str(response.text)
            
            success = xss_blocked and sql_blocked
            self.log_test("Input Validation", success, 
                         f"XSS blocked: {xss_blocked}, SQL blocked: {sql_blocked}")
            return success
            
        except Exception as e:
            self.log_test("Input Validation", False, f"Test error: {e}")
            return False
    
    def test_api_documentation(self) -> bool:
        """测试API文档功能"""
        print("📚 Testing API documentation...")
        
        try:
            # 检查OpenAPI规范文件
            openapi_file = "docs/api/openapi.yaml"
            openapi_exists = os.path.exists(openapi_file)
            
            # 检查Swagger UI（如果实现了）
            swagger_accessible = False
            try:
                response = requests.get(f"{self.api_url}/api/docs", timeout=5)
                swagger_accessible = response.status_code == 200
            except:
                pass
            
            success = openapi_exists
            self.log_test("API Documentation", success, 
                         f"OpenAPI file: {openapi_exists}, Swagger UI: {swagger_accessible}")
            return success
            
        except Exception as e:
            self.log_test("API Documentation", False, f"Test error: {e}")
            return False
    
    def test_performance_monitoring(self) -> bool:
        """测试性能监控功能"""
        print("📊 Testing performance monitoring...")
        
        try:
            # 检查系统指标端点
            response = requests.get(f"{self.api_url}/api/system/metrics", timeout=5)
            metrics_available = response.status_code == 200
            
            # 检查性能优化脚本
            optimizer_script = "scripts/performance_optimizer.py"
            optimizer_exists = os.path.exists(optimizer_script)
            
            success = metrics_available and optimizer_exists
            self.log_test("Performance Monitoring", success, 
                         f"Metrics API: {metrics_available}, Optimizer script: {optimizer_exists}")
            return success
            
        except Exception as e:
            self.log_test("Performance Monitoring", False, f"Test error: {e}")
            return False
    
    def test_docker_configuration(self) -> bool:
        """测试Docker配置"""
        print("🐳 Testing Docker configuration...")
        
        try:
            # 检查Dockerfile
            dockerfile_exists = os.path.exists("Dockerfile")
            
            # 检查docker-compose配置
            compose_exists = os.path.exists("docker/docker-compose.yml")
            
            # 检查健康检查脚本
            healthcheck_exists = os.path.exists("docker/healthcheck.sh")
            
            success = dockerfile_exists and compose_exists and healthcheck_exists
            self.log_test("Docker Configuration", success, 
                         f"Dockerfile: {dockerfile_exists}, Compose: {compose_exists}, Healthcheck: {healthcheck_exists}")
            return success
            
        except Exception as e:
            self.log_test("Docker Configuration", False, f"Test error: {e}")
            return False
    
    def test_security_features(self) -> bool:
        """测试安全功能"""
        print("🔒 Testing security features...")
        
        try:
            # 检查安全组件文件
            rate_limiter_exists = os.path.exists("src/security/RateLimiter.h")
            input_validator_exists = os.path.exists("src/security/InputValidator.h")
            access_logger_exists = os.path.exists("src/security/AccessLogger.h")
            
            # 测试JWT认证（如果有token）
            auth_working = False
            try:
                response = requests.post(f"{self.api_url}/api/auth/login", 
                                       json={"username": "admin", "password": "admin123"}, 
                                       timeout=5)
                auth_working = response.status_code in [200, 401]  # 200成功或401认证失败都说明端点工作
            except:
                pass
            
            success = rate_limiter_exists and input_validator_exists and access_logger_exists
            self.log_test("Security Features", success, 
                         f"Components exist: {success}, Auth endpoint: {auth_working}")
            return success
            
        except Exception as e:
            self.log_test("Security Features", False, f"Test error: {e}")
            return False
    
    def test_database_optimization(self) -> bool:
        """测试数据库优化"""
        print("🗄️  Testing database optimization...")
        
        try:
            # 检查连接池组件
            connection_pool_exists = os.path.exists("src/database/ConnectionPool.h")
            
            # 测试数据库响应时间
            start_time = time.time()
            response = requests.get(f"{self.api_url}/api/system/info", timeout=5)
            response_time = (time.time() - start_time) * 1000
            
            # 响应时间应该小于100ms
            fast_response = response_time < 100 and response.status_code == 200
            
            success = connection_pool_exists and fast_response
            self.log_test("Database Optimization", success, 
                         f"Connection pool: {connection_pool_exists}, Response time: {response_time:.1f}ms")
            return success
            
        except Exception as e:
            self.log_test("Database Optimization", False, f"Test error: {e}")
            return False
    
    def run_all_tests(self) -> Dict[str, Any]:
        """运行所有测试"""
        print("🚀 Starting Phase 4 Features Test")
        print("=" * 50)
        
        # 运行各项测试
        tests = [
            ("API Connectivity", self.test_api_connectivity),
            ("Rate Limiting", self.test_rate_limiting),
            ("Input Validation", self.test_input_validation),
            ("API Documentation", self.test_api_documentation),
            ("Performance Monitoring", self.test_performance_monitoring),
            ("Docker Configuration", self.test_docker_configuration),
            ("Security Features", self.test_security_features),
            ("Database Optimization", self.test_database_optimization),
        ]
        
        passed_tests = 0
        total_tests = len(tests)
        
        for test_name, test_func in tests:
            try:
                if test_func():
                    passed_tests += 1
            except Exception as e:
                self.log_test(test_name, False, f"Unexpected error: {e}")
        
        # 计算成功率
        success_rate = (passed_tests / total_tests) * 100
        
        print("\n" + "=" * 50)
        print(f"📊 Test Summary:")
        print(f"   Total Tests: {total_tests}")
        print(f"   Passed: {passed_tests}")
        print(f"   Failed: {total_tests - passed_tests}")
        print(f"   Success Rate: {success_rate:.1f}%")
        
        # 生成测试报告
        report = {
            "summary": {
                "total_tests": total_tests,
                "passed_tests": passed_tests,
                "failed_tests": total_tests - passed_tests,
                "success_rate": success_rate,
                "test_time": datetime.now().isoformat()
            },
            "results": self.test_results
        }
        
        # 保存报告
        report_file = "phase4_test_report.json"
        with open(report_file, 'w') as f:
            json.dump(report, f, indent=2)
        
        print(f"📄 Test report saved to: {report_file}")
        
        if success_rate >= 70:
            print("🎉 Phase 4 features are working well!")
        else:
            print("⚠️  Some Phase 4 features need attention.")
        
        return report

def main():
    """主函数"""
    import argparse
    
    parser = argparse.ArgumentParser(description="Phase 4 Features Test")
    parser.add_argument("--api-url", default="http://localhost:8080", help="API base URL")
    args = parser.parse_args()
    
    tester = Phase4Tester(args.api_url)
    result = tester.run_all_tests()
    
    # 根据成功率设置退出码
    success_rate = result["summary"]["success_rate"]
    sys.exit(0 if success_rate >= 70 else 1)

if __name__ == "__main__":
    main()
