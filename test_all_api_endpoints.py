#!/usr/bin/env python3
"""
AI Security Vision System - Comprehensive API Endpoint Tester
测试所有后端API端点的响应状态和数据格式
"""

import requests
import json
import time
from datetime import datetime
from typing import Dict, List, Tuple, Optional
import sys

class APITester:
    def __init__(self, base_url: str = "http://localhost:8080"):
        self.base_url = base_url
        self.session = requests.Session()
        self.session.timeout = 10
        self.results = []
        
    def test_endpoint(self, method: str, path: str, description: str, 
                     data: Optional[Dict] = None, headers: Optional[Dict] = None) -> Dict:
        """测试单个API端点"""
        url = f"{self.base_url}{path}"
        start_time = time.time()
        
        try:
            if method.upper() == "GET":
                response = self.session.get(url, headers=headers)
            elif method.upper() == "POST":
                response = self.session.post(url, json=data, headers=headers)
            elif method.upper() == "PUT":
                response = self.session.put(url, json=data, headers=headers)
            elif method.upper() == "DELETE":
                response = self.session.delete(url, headers=headers)
            else:
                raise ValueError(f"Unsupported method: {method}")
                
            response_time = (time.time() - start_time) * 1000  # ms
            
            # 尝试解析JSON响应
            try:
                json_data = response.json()
                is_json = True
            except:
                json_data = None
                is_json = False
                
            result = {
                "method": method.upper(),
                "path": path,
                "description": description,
                "status_code": response.status_code,
                "response_time_ms": round(response_time, 2),
                "is_json": is_json,
                "content_length": len(response.content),
                "success": 200 <= response.status_code < 300,
                "response_data": json_data if is_json else response.text[:200],
                "error": None
            }
            
        except Exception as e:
            result = {
                "method": method.upper(),
                "path": path,
                "description": description,
                "status_code": 0,
                "response_time_ms": 0,
                "is_json": False,
                "content_length": 0,
                "success": False,
                "response_data": None,
                "error": str(e)
            }
            
        self.results.append(result)
        return result
        
    def run_all_tests(self):
        """运行所有API端点测试"""
        print("🚀 开始测试AI Security Vision System API端点...")
        print(f"📡 后端地址: {self.base_url}")
        print("=" * 80)
        
        # 定义所有API端点
        endpoints = [
            # System Management
            ("GET", "/api/system/status", "获取系统状态"),
            ("GET", "/api/system/info", "获取系统信息"),
            ("GET", "/api/system/config", "获取系统配置"),
            ("POST", "/api/system/config", "更新系统配置", {"key": "test", "value": "test"}),
            ("GET", "/api/system/metrics", "获取系统指标"),
            ("GET", "/api/system/stats", "获取系统统计"),
            ("GET", "/api/system/pipeline-stats", "获取管道统计"),
            
            # Camera Management
            ("GET", "/api/cameras", "获取所有摄像头"),
            ("POST", "/api/cameras", "添加新摄像头", {
                "id": "test_camera",
                "name": "Test Camera",
                "rtsp_url": "rtsp://test:test@192.168.1.100:554/stream",
                "enabled": True
            }),
            ("GET", "/api/cameras/test_camera", "获取特定摄像头"),
            ("PUT", "/api/cameras/test_camera", "更新摄像头配置", {
                "name": "Updated Test Camera",
                "enabled": False
            }),
            ("DELETE", "/api/cameras/test_camera", "删除摄像头"),
            ("POST", "/api/cameras/test-connection", "测试摄像头连接", {
                "rtsp_url": "rtsp://test:test@192.168.1.100:554/stream"
            }),
            
            # Camera Configuration
            ("GET", "/api/cameras/configs", "获取摄像头配置"),
            ("POST", "/api/cameras/config", "更新摄像头配置", {
                "camera_id": "test_camera",
                "detection_enabled": True,
                "recording_enabled": False
            }),
            
            # AI Detection
            ("GET", "/api/detection/categories", "获取检测类别"),
            ("POST", "/api/detection/categories", "更新检测类别", {
                "enabled_categories": ["person", "car", "bicycle"]
            }),
            ("GET", "/api/detection/available-categories", "获取可用检测类别"),
            ("GET", "/api/detection/config", "获取检测配置"),
            ("PUT", "/api/detection/config", "更新检测配置", {
                "confidence_threshold": 0.5,
                "nms_threshold": 0.4
            }),
            ("GET", "/api/detection/stats", "获取检测统计"),
            
            # Person Statistics
            ("GET", "/api/person-stats/camera_01", "获取人员统计"),
            ("GET", "/api/person-stats/camera_01/hourly", "获取小时统计"),
            ("GET", "/api/person-stats/camera_01/daily", "获取日统计"),
            ("GET", "/api/person-stats/camera_01/age-gender", "获取年龄性别统计"),
            
            # Network Management
            ("GET", "/api/network/interfaces", "获取网络接口"),
            ("POST", "/api/network/config", "更新网络配置", {
                "interface": "eth0",
                "ip": "192.168.1.100",
                "netmask": "255.255.255.0"
            }),
            ("GET", "/api/network/ports", "获取端口配置"),
            ("POST", "/api/network/ports", "更新端口配置", {
                "api_port": 8080,
                "mjpeg_start_port": 8090
            }),
            
            # ONVIF Discovery
            ("GET", "/api/source/discover", "ONVIF设备发现"),
            ("POST", "/api/source/add-discovered", "添加发现的设备", {
                "device_url": "http://192.168.1.100:80/onvif/device_service",
                "username": "admin",
                "password": "password"
            }),
            
            # Alerts
            ("GET", "/api/alerts", "获取报警列表"),
            ("GET", "/api/alerts/recent", "获取最近报警"),
            ("PUT", "/api/alerts/123/acknowledge", "确认报警"),
            ("DELETE", "/api/alerts/123", "删除报警"),
            
            # Face Management
            ("GET", "/api/faces", "获取人脸列表"),
            ("POST", "/api/faces/add", "添加人脸"),
            ("DELETE", "/api/faces/123", "删除人脸"),
            ("POST", "/api/faces/verify", "人脸验证"),
            
            # ReID Configuration
            ("GET", "/api/reid/config", "获取ReID配置"),
            ("POST", "/api/reid/config", "更新ReID配置"),
            ("PUT", "/api/reid/threshold", "更新ReID阈值"),
            ("GET", "/api/reid/status", "获取ReID状态"),
            
            # Authentication (需要实现)
            ("POST", "/api/auth/login", "用户登录", {
                "username": "admin",
                "password": "password"
            }),
            ("POST", "/api/auth/logout", "用户登出"),
            ("GET", "/api/auth/user", "获取当前用户"),
            ("GET", "/api/auth/users", "获取用户列表"),
            ("POST", "/api/auth/users", "创建用户"),
            ("PUT", "/api/auth/users/status", "启用/禁用用户"),
            
            # Recordings (占位符)
            ("GET", "/api/recordings", "获取录像列表"),
            ("GET", "/api/recordings/123", "获取特定录像"),
            ("DELETE", "/api/recordings/123", "删除录像"),
            
            # Logs and Statistics (占位符)
            ("GET", "/api/logs", "获取系统日志"),
            ("GET", "/api/statistics", "获取统计信息"),
        ]
        
        # 执行测试
        for i, endpoint in enumerate(endpoints, 1):
            method, path, description = endpoint[:3]
            data = endpoint[3] if len(endpoint) > 3 else None
            
            print(f"[{i:2d}/{len(endpoints)}] {method:6s} {path:40s} - {description}")
            
            result = self.test_endpoint(method, path, description, data)
            
            # 显示结果
            status_icon = "✅" if result["success"] else "❌"
            status_code = result["status_code"]
            response_time = result["response_time_ms"]
            
            if result["error"]:
                print(f"         {status_icon} {status_code} - ERROR: {result['error']}")
            else:
                print(f"         {status_icon} {status_code} - {response_time}ms")
            
            time.sleep(0.1)  # 避免请求过快
            
        print("=" * 80)
        self.print_summary()
        
    def print_summary(self):
        """打印测试总结"""
        total = len(self.results)
        successful = sum(1 for r in self.results if r["success"])
        failed = total - successful
        
        print(f"📊 测试总结:")
        print(f"   总计: {total} 个端点")
        print(f"   成功: {successful} 个 ({successful/total*100:.1f}%)")
        print(f"   失败: {failed} 个 ({failed/total*100:.1f}%)")
        
        if failed > 0:
            print(f"\n❌ 失败的端点:")
            for result in self.results:
                if not result["success"]:
                    error_msg = result["error"] or f"HTTP {result['status_code']}"
                    print(f"   {result['method']} {result['path']} - {error_msg}")
        
        # 性能统计
        response_times = [r["response_time_ms"] for r in self.results if r["response_time_ms"] > 0]
        if response_times:
            avg_time = sum(response_times) / len(response_times)
            max_time = max(response_times)
            print(f"\n⏱️  响应时间:")
            print(f"   平均: {avg_time:.1f}ms")
            print(f"   最大: {max_time:.1f}ms")
            
            # 慢端点警告
            slow_endpoints = [r for r in self.results if r["response_time_ms"] > 200]
            if slow_endpoints:
                print(f"\n🐌 慢端点 (>200ms):")
                for result in slow_endpoints:
                    print(f"   {result['method']} {result['path']} - {result['response_time_ms']:.1f}ms")

if __name__ == "__main__":
    base_url = sys.argv[1] if len(sys.argv) > 1 else "http://localhost:8080"
    
    tester = APITester(base_url)
    tester.run_all_tests()
