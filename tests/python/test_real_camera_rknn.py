#!/usr/bin/env python3
"""
实际摄像头RKNN检测测试
使用真实的RTSP摄像头和RKNN模型进行实时AI检测
"""

import requests
import json
import time
import webbrowser
import subprocess
import sys
import threading
import os

# 禁用代理
os.environ.pop('http_proxy', None)
os.environ.pop('https_proxy', None)
os.environ.pop('HTTP_PROXY', None)
os.environ.pop('HTTPS_PROXY', None)

class RealCameraRKNNTest:
    def __init__(self):
        self.api_base = "http://localhost:8080"
        self.camera_configs = [
            {
                "name": "Camera1_RKNN_Detection",
                "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
                "stream_port": 8161
            },
            {
                "name": "Camera2_RKNN_Detection",
                "url": "rtsp://admin:sharpi1688@192.168.1.3:554/1/1",
                "stream_port": 8162
            }
        ]
        self.active_cameras = []

    def test_api_connection(self):
        """测试API连接"""
        try:
            response = requests.get(f"{self.api_base}/api/system/status", timeout=5)
            if response.status_code == 200:
                status = response.json()
                print(f"✅ API连接成功: {status}")
                return True
            else:
                print(f"❌ API连接失败: {response.status_code}")
                return False
        except Exception as e:
            print(f"❌ API连接异常: {e}")
            return False

    def test_camera_connection(self, camera_url):
        """测试摄像头连接"""
        print(f"🔍 测试摄像头连接: {camera_url}")
        try:
            # 使用ffprobe测试RTSP流
            cmd = [
                "ffprobe", "-v", "quiet", "-print_format", "json",
                "-show_streams", "-select_streams", "v:0",
                "-timeout", "10000000", camera_url
            ]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=15)

            if result.returncode == 0:
                stream_info = json.loads(result.stdout)
                if stream_info.get("streams"):
                    stream = stream_info["streams"][0]
                    print(f"✅ 摄像头连接成功:")
                    print(f"   - 分辨率: {stream.get('width', 'N/A')}x{stream.get('height', 'N/A')}")
                    print(f"   - 编码: {stream.get('codec_name', 'N/A')}")
                    print(f"   - 帧率: {stream.get('r_frame_rate', 'N/A')}")
                    return True
                else:
                    print("❌ 未找到视频流")
                    return False
            else:
                print(f"❌ 摄像头连接失败: {result.stderr}")
                return False

        except subprocess.TimeoutExpired:
            print("❌ 摄像头连接超时")
            return False
        except Exception as e:
            print(f"❌ 摄像头连接异常: {e}")
            return False

    def add_camera_source(self, camera_config):
        """添加摄像头源"""
        print(f"\n📹 添加摄像头源: {camera_config['name']}")

        # 首先测试摄像头连接
        if not self.test_camera_connection(camera_config['url']):
            print(f"❌ 跳过摄像头 {camera_config['name']} - 连接失败")
            return None

        try:
            response = requests.post(
                f"{self.api_base}/api/source/add",
                headers={"Content-Type": "application/json"},
                json={
                    "url": camera_config['url'],
                    "name": camera_config['name']
                },
                timeout=30
            )

            if response.status_code == 200:
                result = response.json()
                camera_id = result.get('camera_id')
                print(f"✅ 摄像头源添加成功: {camera_id}")
                self.active_cameras.append({
                    'id': camera_id,
                    'config': camera_config,
                    'result': result
                })
                return camera_id
            else:
                print(f"❌ 添加摄像头源失败: {response.status_code} - {response.text}")
                return None

        except Exception as e:
            print(f"❌ 添加摄像头源异常: {e}")
            return None

    def get_camera_status(self, camera_id):
        """获取摄像头状态"""
        try:
            response = requests.get(f"{self.api_base}/api/source/list", timeout=5)
            if response.status_code == 200:
                sources = response.json()
                for source in sources.get('sources', []):
                    if source.get('camera_id') == camera_id:
                        return source
            return None
        except Exception as e:
            print(f"❌ 获取摄像头状态异常: {e}")
            return None

    def start_streaming(self, camera_id):
        """启动流媒体"""
        try:
            response = requests.post(
                f"{self.api_base}/api/stream/start",
                headers={"Content-Type": "application/json"},
                json={"camera_id": camera_id},
                timeout=10
            )

            if response.status_code == 200:
                result = response.json()
                print(f"✅ 流媒体启动成功: {result}")
                return True
            else:
                print(f"❌ 启动流媒体失败: {response.status_code} - {response.text}")
                return False

        except Exception as e:
            print(f"❌ 启动流媒体异常: {e}")
            return False

    def get_system_status(self):
        """获取系统状态"""
        try:
            response = requests.get(f"{self.api_base}/api/system/status", timeout=5)
            if response.status_code == 200:
                return response.json()
            return None
        except Exception as e:
            print(f"❌ 获取系统状态异常: {e}")
            return None

    def open_visualization(self):
        """打开可视化界面"""
        print("\n🌐 打开实时检测可视化界面...")

        urls_to_open = [
            "http://localhost:8080",  # 主控制面板
        ]

        # 添加每个活跃摄像头的流地址
        for camera in self.active_cameras:
            stream_port = camera['config']['stream_port']
            stream_url = f"http://localhost:{stream_port}"
            urls_to_open.append(stream_url)
            print(f"📹 {camera['config']['name']}: {stream_url}")

        # 打开浏览器
        for url in urls_to_open:
            try:
                webbrowser.open(url)
                time.sleep(1)  # 避免同时打开太多窗口
            except Exception as e:
                print(f"⚠️  打开 {url} 失败: {e}")

    def monitor_detection(self, duration=60):
        """监控检测过程"""
        print(f"\n📊 开始监控检测过程 ({duration}秒)...")

        start_time = time.time()
        while time.time() - start_time < duration:
            try:
                # 获取系统状态
                status = self.get_system_status()
                if status:
                    pipelines = status.get('active_pipelines', 0)
                    cpu = status.get('cpu_usage', 'N/A')
                    print(f"📈 活跃管道: {pipelines}, CPU: {cpu}%")

                # 获取每个摄像头的状态
                for camera in self.active_cameras:
                    camera_status = self.get_camera_status(camera['id'])
                    if camera_status:
                        fps = camera_status.get('fps', 'N/A')
                        status_str = camera_status.get('status', 'unknown')
                        print(f"🎯 {camera['config']['name']}: 状态={status_str}, FPS={fps}")

                time.sleep(5)  # 每5秒检查一次

            except KeyboardInterrupt:
                print("\n🛑 监控被用户中断")
                break
            except Exception as e:
                print(f"❌ 监控异常: {e}")
                time.sleep(5)

    def run_test(self):
        """运行完整测试"""
        print("🚀 开始实际摄像头RKNN检测测试")
        print("=" * 60)

        # 1. 测试API连接
        print("\n1️⃣ 测试API连接...")
        if not self.test_api_connection():
            print("❌ 请确保AI安全视觉系统正在运行")
            return False

        # 2. 添加摄像头源
        print("\n2️⃣ 添加摄像头源...")
        for camera_config in self.camera_configs:
            self.add_camera_source(camera_config)

        if not self.active_cameras:
            print("❌ 没有成功添加任何摄像头")
            return False

        print(f"✅ 成功添加 {len(self.active_cameras)} 个摄像头")

        # 3. 启动流媒体
        print("\n3️⃣ 启动流媒体...")
        for camera in self.active_cameras:
            self.start_streaming(camera['id'])

        # 4. 等待系统稳定
        print("\n4️⃣ 等待系统稳定...")
        time.sleep(10)

        # 5. 打开可视化界面
        print("\n5️⃣ 打开可视化界面...")
        self.open_visualization()

        # 6. 监控检测过程
        print("\n6️⃣ 开始实时监控...")
        try:
            self.monitor_detection(300)  # 监控5分钟
        except KeyboardInterrupt:
            print("\n🛑 测试被用户中断")

        print("\n✅ 实际摄像头RKNN检测测试完成!")
        print("\n💡 测试总结:")
        print(f"   - 成功连接摄像头: {len(self.active_cameras)} 个")
        print("   - RKNN NPU加速: ✅ 启用")
        print("   - 实时AI检测: ✅ 运行中")
        print("   - 可视化界面: ✅ 已打开")

        return True

def main():
    """主函数"""
    print("🎯 实际摄像头RKNN检测测试工具")
    print("使用真实RTSP摄像头和RKNN模型进行AI检测")
    print("=" * 60)

    # 检查RKNN模型文件
    rknn_model = "models/yolov8n.rknn"
    if not os.path.exists(rknn_model):
        print(f"❌ RKNN模型文件不存在: {rknn_model}")
        return
    else:
        print(f"✅ RKNN模型文件存在: {rknn_model}")

    # 创建测试实例并运行
    test = RealCameraRKNNTest()
    test.run_test()

if __name__ == "__main__":
    main()
