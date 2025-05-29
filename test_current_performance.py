#!/usr/bin/env python3
"""
测试当前MJPEG推流性能
"""

import time
import requests
import threading
from datetime import datetime

def test_mjpeg_stream_performance():
    """测试MJPEG流性能"""
    print("=== MJPEG流性能测试 ===")
    
    # 测试URL
    urls = [
        "http://localhost:8161",
        "http://localhost:8162"
    ]
    
    for url in urls:
        print(f"\n测试流: {url}")
        try:
            # 测试连接
            response = requests.get(url, timeout=5, stream=True)
            if response.status_code == 200:
                print(f"✓ 流连接成功")
                
                # 测试帧率
                frame_count = 0
                start_time = time.time()
                test_duration = 10  # 测试10秒
                
                for chunk in response.iter_content(chunk_size=1024):
                    if b'\xff\xd8' in chunk:  # JPEG开始标记
                        frame_count += 1
                    
                    if time.time() - start_time > test_duration:
                        break
                
                elapsed = time.time() - start_time
                fps = frame_count / elapsed
                print(f"✓ 检测到 {frame_count} 帧，用时 {elapsed:.1f}s")
                print(f"✓ 估算FPS: {fps:.1f}")
                
                if fps >= 15:
                    print("✅ 性能良好")
                elif fps >= 10:
                    print("⚠️  性能一般")
                else:
                    print("❌ 性能较差，需要优化")
                    
            else:
                print(f"❌ 连接失败: {response.status_code}")
                
        except Exception as e:
            print(f"❌ 测试失败: {e}")

def test_api_performance():
    """测试API响应性能"""
    print("\n=== API性能测试 ===")
    
    api_tests = [
        ("GET", "http://localhost:8080/api/system/status"),
        ("GET", "http://localhost:8080/api/cameras"),
        ("GET", "http://localhost:8080/api/system/performance")
    ]
    
    for method, url in api_tests:
        try:
            start_time = time.time()
            response = requests.request(method, url, timeout=5)
            elapsed = (time.time() - start_time) * 1000
            
            print(f"{method} {url.split('/')[-1]}: {elapsed:.1f}ms - {response.status_code}")
            
        except Exception as e:
            print(f"❌ {url}: {e}")

def monitor_system_resources():
    """监控系统资源"""
    print("\n=== 系统资源监控 ===")
    
    try:
        # NPU频率
        with open('/sys/class/devfreq/fdab0000.npu/cur_freq', 'r') as f:
            npu_freq = int(f.read().strip()) / 1000000
        print(f"NPU频率: {npu_freq:.0f} MHz")
        
        # NPU调度器
        with open('/sys/class/devfreq/fdab0000.npu/governor', 'r') as f:
            npu_gov = f.read().strip()
        print(f"NPU调度器: {npu_gov}")
        
        # 内存使用
        with open('/proc/meminfo', 'r') as f:
            lines = f.readlines()
            for line in lines:
                if 'MemTotal:' in line:
                    total = int(line.split()[1]) // 1024
                elif 'MemAvailable:' in line:
                    available = int(line.split()[1]) // 1024
                    break
        
        used = total - available
        usage = (used / total) * 100
        print(f"内存使用: {used}MB / {total}MB ({usage:.1f}%)")
        
        # CPU负载
        with open('/proc/loadavg', 'r') as f:
            load = f.read().split()[0]
        print(f"CPU负载: {load}")
        
    except Exception as e:
        print(f"❌ 资源监控失败: {e}")

def test_inference_timing():
    """测试推理时间"""
    print("\n=== 推理时间测试 ===")
    
    # 通过API测试推理
    try:
        # 添加摄像头
        camera_data = {
            "name": "TestCamera",
            "rtsp_url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
            "enabled": True
        }
        
        response = requests.post("http://localhost:8080/api/cameras", 
                               json=camera_data, timeout=10)
        
        if response.status_code == 200:
            print("✓ 摄像头添加成功")
            
            # 等待几秒让系统开始处理
            time.sleep(5)
            
            # 获取性能统计
            perf_response = requests.get("http://localhost:8080/api/system/performance", 
                                       timeout=5)
            
            if perf_response.status_code == 200:
                perf_data = perf_response.json()
                print(f"✓ 性能数据获取成功")
                
                # 查找推理时间信息
                if 'inference_time' in str(perf_data):
                    print(f"推理时间数据: {perf_data}")
                else:
                    print("⚠️  未找到推理时间数据")
            else:
                print(f"❌ 性能数据获取失败: {perf_response.status_code}")
                
        else:
            print(f"❌ 摄像头添加失败: {response.status_code}")
            
    except Exception as e:
        print(f"❌ 推理测试失败: {e}")

def main():
    print("🔍 当前系统性能测试")
    print("=" * 50)
    print(f"测试时间: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    
    # 系统资源监控
    monitor_system_resources()
    
    # API性能测试
    test_api_performance()
    
    # MJPEG流性能测试
    test_mjpeg_stream_performance()
    
    # 推理时间测试
    test_inference_timing()
    
    print("\n" + "=" * 50)
    print("📊 性能测试完成")
    print("\n如果发现性能问题，将实施zero-copy优化:")
    print("1. 使用RKNN zero-copy模式")
    print("2. 减少内存拷贝操作")
    print("3. 优化数据流水线")
    print("4. 使用DMA缓冲区")

if __name__ == "__main__":
    main()
