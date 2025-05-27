#!/usr/bin/env python3
"""
Live Detection Test Script
测试实时AI检测功能并查看可视化结果
"""

import requests
import json
import time
import webbrowser
import subprocess
import sys

def test_api_connection():
    """测试API连接"""
    try:
        response = requests.get("http://localhost:8080/api/system/status", timeout=5)
        print(f"✅ API连接成功: {response.status_code}")
        return True
    except Exception as e:
        print(f"❌ API连接失败: {e}")
        return False

def add_camera_task():
    """添加摄像头检测任务"""
    task_config = {
        "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
        "name": "Camera1_Live_Detection"
    }

    try:
        response = requests.post(
            "http://localhost:8080/api/source/add",
            headers={"Content-Type": "application/json"},
            json=task_config,
            timeout=10
        )

        if response.status_code == 200:
            result = response.json()
            print(f"✅ 摄像头任务添加成功: {result}")
            return result.get('camera_id')
        else:
            print(f"❌ 添加任务失败: {response.status_code} - {response.text}")
            return None

    except Exception as e:
        print(f"❌ 添加任务异常: {e}")
        return None

def check_task_status(task_id):
    """检查任务状态"""
    try:
        response = requests.get(f"http://localhost:8080/api/tasks/{task_id}", timeout=5)
        if response.status_code == 200:
            task_info = response.json()
            print(f"📊 任务状态: {task_info}")
            return task_info
        else:
            print(f"❌ 获取任务状态失败: {response.status_code}")
            return None
    except Exception as e:
        print(f"❌ 检查任务状态异常: {e}")
        return None

def get_system_status():
    """获取系统状态"""
    try:
        response = requests.get("http://localhost:8080/api/system/status", timeout=5)
        if response.status_code == 200:
            status = response.json()
            print(f"🖥️  系统状态: {status}")
            return status
        else:
            print(f"❌ 获取系统状态失败: {response.status_code}")
            return None
    except Exception as e:
        print(f"❌ 获取系统状态异常: {e}")
        return None

def open_visualization():
    """打开可视化界面"""
    print("🌐 打开可视化界面...")

    # 主控制面板
    print("📱 主控制面板: http://localhost:8080")

    # 实时视频流 (如果任务启动成功)
    print("📹 实时检测流: http://localhost:8161")

    # 尝试打开浏览器
    try:
        webbrowser.open("http://localhost:8080")
        time.sleep(2)
        webbrowser.open("http://localhost:8161")
    except Exception as e:
        print(f"⚠️  自动打开浏览器失败: {e}")

def main():
    """主函数"""
    print("🚀 开始实时AI检测测试...")
    print("=" * 50)

    # 1. 测试API连接
    if not test_api_connection():
        print("❌ 请确保AI安全视觉系统正在运行")
        return

    # 2. 获取系统状态
    get_system_status()

    # 3. 添加摄像头任务
    print("\n📹 添加摄像头检测任务...")
    task_id = add_camera_task()

    if not task_id:
        print("❌ 无法添加摄像头任务")
        return

    # 4. 等待任务启动
    print(f"\n⏳ 等待任务启动 (ID: {task_id})...")
    for i in range(10):
        time.sleep(2)
        task_info = check_task_status(task_id)
        if task_info and task_info.get('status') == 'running':
            print("✅ 任务启动成功!")
            break
        print(f"⏳ 等待中... ({i+1}/10)")

    # 5. 打开可视化界面
    print("\n🎯 打开实时检测可视化...")
    open_visualization()

    # 6. 持续监控
    print("\n📊 开始实时监控 (按Ctrl+C停止)...")
    try:
        while True:
            time.sleep(5)
            status = get_system_status()
            if status:
                pipelines = status.get('active_pipelines', 0)
                cpu = status.get('cpu_usage', 'N/A')
                print(f"📈 活跃管道: {pipelines}, CPU使用率: {cpu}%")

            if task_id:
                task_info = check_task_status(task_id)
                if task_info:
                    fps = task_info.get('fps', 'N/A')
                    detections = task_info.get('total_detections', 'N/A')
                    print(f"🎯 FPS: {fps}, 总检测数: {detections}")

    except KeyboardInterrupt:
        print("\n🛑 停止监控")

    print("\n✅ 测试完成!")
    print("💡 提示:")
    print("   - 主控制面板: http://localhost:8080")
    print("   - 实时检测流: http://localhost:8161")
    print("   - 使用Ctrl+C停止系统")

if __name__ == "__main__":
    main()
