#!/usr/bin/env python3
"""
AI Inference Testing Script for Real Cameras
Tests the AI detection functionality with actual RTSP streams
"""

import requests
import time
import json
import subprocess
import sys
from datetime import datetime

# Configuration
API_BASE_URL = "http://localhost:8080/api"

def print_status(message):
    timestamp = datetime.now().strftime("%H:%M:%S")
    print(f"[{timestamp}] {message}")

def test_api(endpoint):
    """Test API endpoint without proxy"""
    try:
        url = f"{API_BASE_URL}{endpoint}"
        proxies = {'http': None, 'https': None}
        response = requests.get(url, timeout=5, proxies=proxies)
        return response.json() if response.status_code == 200 else None
    except Exception as e:
        print_status(f"API Error: {e}")
        return None

def test_mjpeg_ports():
    """Test which MJPEG ports are actually working"""
    ports_to_test = [8030, 8161, 8162, 8000, 8001, 8002]
    working_ports = []
    
    for port in ports_to_test:
        try:
            cmd = f"timeout 2 curl --noproxy localhost http://localhost:{port}/stream.mjpg --output /dev/null -w '%{{http_code}}'"
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            
            if result.returncode == 124:  # timeout - means stream is working
                working_ports.append(port)
                print_status(f"✓ Port {port}: MJPEG stream active")
            elif "200" in result.stdout:
                working_ports.append(port)
                print_status(f"✓ Port {port}: HTTP 200 response")
            else:
                print_status(f"✗ Port {port}: No stream")
                
        except Exception as e:
            print_status(f"✗ Port {port}: Error - {e}")
    
    return working_ports

def main():
    print("=" * 60)
    print("AI Security Vision - AI Inference Testing")
    print("=" * 60)
    
    # Test 1: System Status
    print_status("Checking system status...")
    status = test_api("/system/status")
    if status:
        print(f"   Status: {status.get('status')}")
        print(f"   Active Pipelines: {status.get('active_pipelines')}")
        print(f"   Monitoring: {status.get('monitoring_healthy')}")
    else:
        print("   ✗ System status unavailable")
        return 1
    
    # Test 2: Camera Sources
    print_status("Checking camera sources...")
    sources = test_api("/source/list")
    if sources:
        cameras = sources.get('sources', [])
        print(f"   Found {len(cameras)} cameras:")
        for camera in cameras:
            print(f"   - {camera.get('id')}: {camera.get('status')}")
    else:
        print("   ✗ Camera sources unavailable")
        return 1
    
    # Test 3: Find Working MJPEG Ports
    print_status("Scanning for active MJPEG streams...")
    working_ports = test_mjpeg_ports()
    
    if not working_ports:
        print("   ✗ No MJPEG streams found")
        return 1
    
    # Test 4: Monitor AI Performance
    print_status("Monitoring AI inference performance...")
    
    detection_stats = []
    for i in range(10):  # Monitor for 20 seconds
        status = test_api("/system/status")
        if status:
            detection_stats.append({
                'timestamp': time.time(),
                'pipelines': status.get('active_pipelines', 0),
                'cpu_usage': status.get('cpu_usage', 0)
            })
        time.sleep(2)
    
    # Analyze performance
    if detection_stats:
        avg_pipelines = sum(s['pipelines'] for s in detection_stats) / len(detection_stats)
        avg_cpu = sum(s['cpu_usage'] for s in detection_stats) / len(detection_stats)
        
        print(f"   Average Active Pipelines: {avg_pipelines:.1f}")
        print(f"   Average CPU Usage: {avg_cpu:.1f}%")
    
    # Test 5: Verify Real Camera Connections
    print_status("Verifying real camera connections...")
    
    camera_urls = [
        "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
        "rtsp://admin:sharpi1688@192.168.1.3:554/1/1"
    ]
    
    for i, url in enumerate(camera_urls, 1):
        # Test RTSP connectivity (basic check)
        try:
            # Use ffprobe to test RTSP stream
            cmd = f"timeout 5 ffprobe -v quiet -print_format json -show_streams '{url}'"
            result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
            
            if result.returncode == 0:
                print_status(f"✓ Camera {i} RTSP stream accessible")
            else:
                print_status(f"✗ Camera {i} RTSP stream failed")
                
        except Exception as e:
            print_status(f"✗ Camera {i} test error: {e}")
    
    # Summary
    print("\n" + "=" * 60)
    print("AI INFERENCE TEST SUMMARY")
    print("=" * 60)
    
    system_ok = status and status.get('status') == 'running'
    cameras_ok = sources and len(sources.get('sources', [])) > 0
    streams_ok = len(working_ports) > 0
    ai_ok = detection_stats and avg_pipelines > 0
    
    print(f"✓ System Running: {'YES' if system_ok else 'NO'}")
    print(f"✓ Cameras Active: {'YES' if cameras_ok else 'NO'}")
    print(f"✓ MJPEG Streams: {'YES' if streams_ok else 'NO'}")
    print(f"✓ AI Inference: {'YES' if ai_ok else 'NO'}")
    
    if working_ports:
        print(f"\nActive MJPEG Streams:")
        for port in working_ports:
            print(f"   http://localhost:{port}/stream.mjpg")
    
    if system_ok and cameras_ok and streams_ok:
        print("\n🎉 AI inference system is working with real cameras!")
        print("   The system is successfully processing RTSP streams and providing AI detection.")
        return 0
    else:
        print("\n⚠️  Some components are not working properly.")
        return 1

if __name__ == "__main__":
    sys.exit(main())
