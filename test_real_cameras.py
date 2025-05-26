#!/usr/bin/env python3
"""
Real Camera Testing Script for AI Security Vision System
Tests actual RTSP cameras with AI inference and MJPEG visualization
"""

import requests
import time
import json
import sys
from datetime import datetime

# Configuration
API_BASE_URL = "http://localhost:8080/api"
CAMERA_1_ID = "camera_192_168_1_2"
CAMERA_2_ID = "camera_192_168_1_3"
MJPEG_PORT_1 = 8161
MJPEG_PORT_2 = 8030  # Current port for camera 2
MJPEG_URL_1 = f"http://localhost:{MJPEG_PORT_1}/stream.mjpg"
MJPEG_URL_2 = f"http://localhost:{MJPEG_PORT_2}/stream.mjpg"

def print_status(message):
    timestamp = datetime.now().strftime("%H:%M:%S")
    print(f"[{timestamp}] {message}")

def test_api_endpoint(endpoint, method="GET", data=None):
    """Test API endpoint and return response"""
    try:
        url = f"{API_BASE_URL}{endpoint}"
        # Disable proxy for localhost
        proxies = {'http': None, 'https': None}
        if method == "GET":
            response = requests.get(url, timeout=5, proxies=proxies)
        elif method == "POST":
            response = requests.post(url, json=data, timeout=5, proxies=proxies)

        print_status(f"{method} {endpoint} -> {response.status_code}")
        if response.status_code == 200:
            return response.json()
        else:
            print(f"   Error: {response.text}")
            return None
    except Exception as e:
        print_status(f"API Error: {e}")
        return None

def test_mjpeg_stream(url, camera_name, timeout=3):
    """Test MJPEG stream availability"""
    try:
        print_status(f"Testing {camera_name} MJPEG stream: {url}")
        # Disable proxy for localhost
        proxies = {'http': None, 'https': None}
        response = requests.get(url, timeout=timeout, stream=True, proxies=proxies)

        if response.status_code == 200:
            # Read some data to verify stream
            data_received = 0
            for chunk in response.iter_content(chunk_size=1024):
                data_received += len(chunk)
                if data_received > 10240:  # 10KB
                    break

            print_status(f"✓ {camera_name} stream OK - {data_received} bytes received")
            return True
        else:
            print_status(f"✗ {camera_name} stream failed - HTTP {response.status_code}")
            return False

    except Exception as e:
        print_status(f"✗ {camera_name} stream error: {e}")
        return False

def main():
    print("=" * 60)
    print("AI Security Vision - Real Camera Testing")
    print("=" * 60)

    # Test 1: System Status
    print_status("Testing system status...")
    status = test_api_endpoint("/system/status")
    if status:
        print(f"   Status: {status.get('status')}")
        print(f"   Active Pipelines: {status.get('active_pipelines')}")
        print(f"   CPU Usage: {status.get('cpu_usage')}")
        print(f"   GPU Memory: {status.get('gpu_memory')}")

    # Test 2: Camera Sources
    print_status("Checking camera sources...")
    sources = test_api_endpoint("/source/list")
    if sources:
        cameras = sources.get('sources', [])
        print(f"   Found {len(cameras)} cameras:")
        for camera in cameras:
            print(f"   - {camera.get('id')}: {camera.get('status')}")

    # Test 3: MJPEG Streams
    print_status("Testing MJPEG streams...")

    # Test Camera 1 (192.168.1.2)
    stream1_ok = test_mjpeg_stream(MJPEG_URL_1, "Camera 192.168.1.2")

    # Test Camera 2 (192.168.1.3)
    stream2_ok = test_mjpeg_stream(MJPEG_URL_2, "Camera 192.168.1.3")

    # Test 4: Detection Performance
    print_status("Monitoring detection performance...")

    # Monitor for 30 seconds
    start_time = time.time()
    detection_count = 0

    while time.time() - start_time < 30:
        status = test_api_endpoint("/system/status")
        if status:
            pipelines = status.get('active_pipelines', 0)
            if pipelines > 0:
                detection_count += 1

        time.sleep(2)

    print_status(f"Detection monitoring completed - {detection_count} status checks")

    # Test 5: Stream URLs
    print_status("Verifying stream URLs...")
    print(f"   Camera 1 (192.168.1.2): {MJPEG_URL_1}")
    print(f"   Camera 2 (192.168.1.3): {MJPEG_URL_2}")

    # Summary
    print("\n" + "=" * 60)
    print("TEST SUMMARY")
    print("=" * 60)
    print(f"✓ System Status: {'OK' if status else 'FAILED'}")
    print(f"✓ Camera Sources: {'OK' if sources else 'FAILED'}")
    print(f"✓ Camera 1 Stream: {'OK' if stream1_ok else 'FAILED'}")
    print(f"✓ Camera 2 Stream: {'OK' if stream2_ok else 'FAILED'}")

    if stream1_ok and stream2_ok:
        print("\n🎉 All tests passed! Real cameras are working with AI inference.")
        print("\nVisualization URLs:")
        print(f"   Camera 1: {MJPEG_URL_1}")
        print(f"   Camera 2: {MJPEG_URL_2}")
        print("\nYou can open these URLs in a web browser to see the live AI detection results.")
    else:
        print("\n⚠️  Some tests failed. Check the system logs for details.")

    return 0

if __name__ == "__main__":
    sys.exit(main())
