#!/usr/bin/env python3
"""
直接RKNN检测测试
跳过摄像头连接测试，直接添加摄像头源进行RKNN检测
"""

import requests
import json
import time
import webbrowser
import os

# 禁用代理
os.environ.pop('http_proxy', None)
os.environ.pop('https_proxy', None)
os.environ.pop('HTTP_PROXY', None)
os.environ.pop('HTTPS_PROXY', None)

def test_api():
    """测试API连接"""
    try:
        response = requests.get("http://localhost:8080/api/system/status", timeout=5)
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

def add_camera():
    """添加摄像头源"""
    print("📹 添加摄像头源...")

    camera_config = {
        "id": "camera1_rknn",
        "name": "Camera1_RKNN_Test",
        "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
        "protocol": "rtsp",
        "width": 1920,
        "height": 1080,
        "fps": 25,
        "enabled": True
    }

    try:
        response = requests.post(
            "http://localhost:8080/api/source/add",
            headers={"Content-Type": "application/json"},
            json=camera_config,
            timeout=30
        )

        print(f"📊 响应状态码: {response.status_code}")
        print(f"📊 响应内容: {response.text}")

        if response.status_code == 200 or response.status_code == 201:
            result = response.json()
            camera_id = result.get('id', result.get('camera_id'))
            print(f"✅ 摄像头源添加成功: {camera_id}")
            return camera_id
        else:
            print(f"❌ 添加摄像头源失败: {response.status_code}")
            return None

    except Exception as e:
        print(f"❌ 添加摄像头源异常: {e}")
        return None

def get_sources():
    """获取摄像头源列表"""
    try:
        response = requests.get("http://localhost:8080/api/source/list", timeout=5)
        if response.status_code == 200:
            sources = response.json()
            print(f"📊 摄像头源列表: {sources}")
            return sources
        else:
            print(f"❌ 获取源列表失败: {response.status_code}")
            return None
    except Exception as e:
        print(f"❌ 获取源列表异常: {e}")
        return None

def start_streaming(camera_id):
    """启动流媒体"""
    try:
        response = requests.post(
            "http://localhost:8080/api/stream/start",
            headers={"Content-Type": "application/json"},
            json={"camera_id": camera_id},
            timeout=10
        )

        print(f"📊 流媒体响应: {response.status_code} - {response.text}")

        if response.status_code == 200:
            result = response.json()
            print(f"✅ 流媒体启动成功: {result}")
            return True
        else:
            print(f"❌ 启动流媒体失败: {response.status_code}")
            return False

    except Exception as e:
        print(f"❌ 启动流媒体异常: {e}")
        return False

def monitor_system(duration=30):
    """监控系统状态"""
    print(f"\n📊 监控系统状态 ({duration}秒)...")

    start_time = time.time()
    while time.time() - start_time < duration:
        try:
            # 系统状态
            response = requests.get("http://localhost:8080/api/system/status", timeout=5)
            if response.status_code == 200:
                status = response.json()
                pipelines = status.get('active_pipelines', 0)
                cpu = status.get('cpu_usage', 'N/A')
                print(f"📈 活跃管道: {pipelines}, CPU: {cpu}%")

            # 源列表
            response = requests.get("http://localhost:8080/api/source/list", timeout=5)
            if response.status_code == 200:
                sources = response.json()
                print(f"📹 摄像头源数量: {len(sources.get('sources', []))}")
                for source in sources.get('sources', []):
                    name = source.get('name', 'Unknown')
                    status_str = source.get('status', 'unknown')
                    fps = source.get('fps', 'N/A')
                    print(f"   - {name}: 状态={status_str}, FPS={fps}")

            time.sleep(5)

        except KeyboardInterrupt:
            print("\n🛑 监控被中断")
            break
        except Exception as e:
            print(f"❌ 监控异常: {e}")
            time.sleep(5)

def open_browser():
    """打开浏览器查看结果"""
    print("\n🌐 打开可视化界面...")

    urls = [
        "http://localhost:8080",      # 主控制面板
        "http://localhost:8161",      # 摄像头1流
        "http://localhost:8162",      # 摄像头2流
    ]

    for url in urls:
        try:
            webbrowser.open(url)
            print(f"📱 已打开: {url}")
            time.sleep(1)
        except Exception as e:
            print(f"⚠️  打开 {url} 失败: {e}")

def main():
    """主函数"""
    print("🚀 直接RKNN检测测试")
    print("=" * 50)

    # 1. 测试API
    print("\n1️⃣ 测试API连接...")
    if not test_api():
        print("❌ API连接失败，请检查系统是否运行")
        return

    # 2. 获取当前源列表
    print("\n2️⃣ 获取当前源列表...")
    get_sources()

    # 3. 添加摄像头
    print("\n3️⃣ 添加摄像头源...")
    camera_id = add_camera()

    if camera_id:
        # 4. 启动流媒体
        print("\n4️⃣ 启动流媒体...")
        start_streaming(camera_id)

        # 5. 等待系统稳定
        print("\n5️⃣ 等待系统稳定...")
        time.sleep(10)

        # 6. 打开浏览器
        print("\n6️⃣ 打开可视化界面...")
        open_browser()

        # 7. 监控系统
        print("\n7️⃣ 监控系统状态...")
        try:
            monitor_system(60)  # 监控1分钟
        except KeyboardInterrupt:
            print("\n🛑 测试被中断")

    print("\n✅ 测试完成!")
    print("\n💡 如果看到活跃管道数量 > 0，说明RKNN检测正在工作")
    print("📱 可以通过以下地址查看:")
    print("   - 主控制面板: http://localhost:8080")
    print("   - 实时检测流: http://localhost:8161")

if __name__ == "__main__":
    main()
