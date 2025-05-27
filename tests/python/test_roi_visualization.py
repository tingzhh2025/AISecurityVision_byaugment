#!/usr/bin/env python3
"""
Test script for ROI Visualization with Priority Color-Coding - Task 73
Tests active ROI display in video stream overlay with time-based toggling.
"""

import requests
import json
import time
import sys
from datetime import datetime, timedelta

# API Configuration
API_BASE_URL = "http://localhost:8080/api"
CAMERA_ID = "test_camera_roi_viz"

def create_test_rois():
    """Create test ROIs with different priorities and time restrictions"""
    print("=== Creating Test ROIs for Visualization ===")
    
    # Get current time for time-based ROI testing
    now = datetime.now()
    current_time = now.strftime("%H:%M")
    future_time = (now + timedelta(minutes=5)).strftime("%H:%M")
    past_time = (now - timedelta(minutes=5)).strftime("%H:%M")
    
    test_rois = [
        {
            "id": "priority_1_roi",
            "name": "Low Priority Zone",
            "polygon": [
                {"x": 50, "y": 50},
                {"x": 200, "y": 50},
                {"x": 200, "y": 150},
                {"x": 50, "y": 150}
            ],
            "enabled": True,
            "priority": 1,  # Green
            "start_time": "",
            "end_time": ""
        },
        {
            "id": "priority_2_roi",
            "name": "Medium-Low Priority Zone",
            "polygon": [
                {"x": 220, "y": 50},
                {"x": 370, "y": 50},
                {"x": 370, "y": 150},
                {"x": 220, "y": 150}
            ],
            "enabled": True,
            "priority": 2,  # Yellow
            "start_time": "",
            "end_time": ""
        },
        {
            "id": "priority_3_roi",
            "name": "Medium Priority Zone",
            "polygon": [
                {"x": 390, "y": 50},
                {"x": 540, "y": 50},
                {"x": 540, "y": 150},
                {"x": 390, "y": 150}
            ],
            "enabled": True,
            "priority": 3,  # Orange
            "start_time": "",
            "end_time": ""
        },
        {
            "id": "priority_4_roi",
            "name": "High Priority Zone",
            "polygon": [
                {"x": 50, "y": 200},
                {"x": 200, "y": 200},
                {"x": 200, "y": 300},
                {"x": 50, "y": 300}
            ],
            "enabled": True,
            "priority": 4,  # Red-Orange
            "start_time": "",
            "end_time": ""
        },
        {
            "id": "priority_5_roi",
            "name": "Critical Priority Zone",
            "polygon": [
                {"x": 220, "y": 200},
                {"x": 370, "y": 200},
                {"x": 370, "y": 300},
                {"x": 220, "y": 300}
            ],
            "enabled": True,
            "priority": 5,  # Red
            "start_time": "",
            "end_time": ""
        },
        {
            "id": "time_restricted_roi",
            "name": "Time-Restricted Zone",
            "polygon": [
                {"x": 390, "y": 200},
                {"x": 540, "y": 200},
                {"x": 540, "y": 300},
                {"x": 390, "y": 300}
            ],
            "enabled": True,
            "priority": 3,
            "start_time": current_time,  # Active now
            "end_time": future_time      # Until 5 minutes from now
        },
        {
            "id": "inactive_time_roi",
            "name": "Inactive Time Zone",
            "polygon": [
                {"x": 50, "y": 350},
                {"x": 200, "y": 350},
                {"x": 200, "y": 450},
                {"x": 50, "y": 450}
            ],
            "enabled": True,
            "priority": 4,
            "start_time": past_time,     # Was active 5 minutes ago
            "end_time": current_time     # Until now (should be inactive)
        },
        {
            "id": "disabled_roi",
            "name": "Disabled Zone",
            "polygon": [
                {"x": 220, "y": 350},
                {"x": 370, "y": 350},
                {"x": 370, "y": 450},
                {"x": 220, "y": 450}
            ],
            "enabled": False,  # Disabled - should not appear
            "priority": 2,
            "start_time": "",
            "end_time": ""
        }
    ]
    
    # Create ROIs using bulk API
    bulk_operations = {
        "operations": []
    }
    
    for roi in test_rois:
        operation = {
            "operation": "create",
            "camera_id": CAMERA_ID,
            **roi
        }
        bulk_operations["operations"].append(operation)
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                               json=bulk_operations, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Created {result['created']} test ROIs")
            return True
        else:
            print(f"   ✗ Failed: {response.text}")
            return False
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False

def verify_roi_visualization():
    """Verify ROIs are visible in the video stream"""
    print("\n=== Verifying ROI Visualization ===")
    
    # Check if streaming is enabled
    try:
        response = requests.get(f"{API_BASE_URL}/cameras")
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            cameras = response.json()
            print(f"   ✓ Found {len(cameras.get('cameras', []))} cameras")
            
            # Look for our test camera
            test_camera_found = False
            for camera in cameras.get('cameras', []):
                if camera.get('id') == CAMERA_ID:
                    test_camera_found = True
                    print(f"   ✓ Test camera found: {CAMERA_ID}")
                    break
            
            if not test_camera_found:
                print(f"   ⚠ Test camera {CAMERA_ID} not found - ROIs may not be visible")
        else:
            print(f"   ✗ Failed to get cameras: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    # Check MJPEG stream endpoint
    print(f"\n   📺 Video stream should be available at:")
    print(f"   📺 http://localhost:8080/stream/mjpeg")
    print(f"   📺 Web interface: http://localhost:8080/")
    
    # Verify ROIs are in database
    try:
        response = requests.get(f"{API_BASE_URL}/rois?camera_id={CAMERA_ID}")
        print(f"\n   Database ROI check - Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Found {result['count']} ROIs in database")
            
            # Show ROI details
            for roi in result.get('rois', []):
                status = "ACTIVE" if roi['enabled'] else "DISABLED"
                time_info = ""
                if roi.get('start_time') and roi.get('end_time'):
                    time_info = f" ({roi['start_time']}-{roi['end_time']})"
                print(f"     - {roi['name']}: Priority {roi['priority']}, {status}{time_info}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")

def test_time_based_toggling():
    """Test time-based ROI activation/deactivation"""
    print("\n=== Testing Time-Based ROI Toggling ===")
    
    # Create a ROI that will become active in 10 seconds
    future_time = (datetime.now() + timedelta(seconds=10)).strftime("%H:%M")
    end_time = (datetime.now() + timedelta(seconds=30)).strftime("%H:%M")
    
    toggle_roi = {
        "operations": [{
            "operation": "create",
            "camera_id": CAMERA_ID,
            "id": "toggle_test_roi",
            "name": "Toggle Test Zone",
            "polygon": [
                {"x": 300, "y": 350},
                {"x": 450, "y": 350},
                {"x": 450, "y": 450},
                {"x": 300, "y": 450}
            ],
            "enabled": True,
            "priority": 3,
            "start_time": future_time,
            "end_time": end_time
        }]
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                               json=toggle_roi, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            print(f"   ✓ Created time-based toggle ROI")
            print(f"   ⏰ ROI will become active at {future_time}")
            print(f"   ⏰ ROI will become inactive at {end_time}")
            print(f"   👀 Watch the video stream to see the ROI appear/disappear!")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")

def cleanup_test_rois():
    """Clean up all test ROIs"""
    print("\n=== Cleaning Up Test ROIs ===")
    
    # Get all ROIs for the test camera
    try:
        response = requests.get(f"{API_BASE_URL}/rois?camera_id={CAMERA_ID}")
        if response.status_code == 200:
            result = response.json()
            roi_ids = [roi['roi_id'] for roi in result.get('rois', [])]
            
            if roi_ids:
                # Delete all test ROIs using bulk API
                cleanup_operations = {
                    "operations": [{"operation": "delete", "roi_id": roi_id} for roi_id in roi_ids]
                }
                
                response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                                       json=cleanup_operations, 
                                       headers={"Content-Type": "application/json"})
                print(f"   Status: {response.status_code}")
                if response.status_code == 200:
                    result = response.json()
                    print(f"   ✓ Cleaned up {result['deleted']} test ROIs")
                else:
                    print(f"   ✗ Cleanup failed: {response.text}")
            else:
                print("   ✓ No test ROIs to clean up")
        else:
            print(f"   ✗ Failed to get ROIs: {response.text}")
    except Exception as e:
        print(f"   ✗ Cleanup error: {e}")

def main():
    """Main test function"""
    print("=== ROI Visualization Test - Task 73 ===\n")
    
    # Step 1: Create test ROIs with different priorities
    if not create_test_rois():
        print("Failed to create test ROIs. Exiting.")
        return False
    
    # Step 2: Verify ROI visualization setup
    verify_roi_visualization()
    
    # Step 3: Test time-based toggling
    test_time_based_toggling()
    
    # Step 4: Instructions for manual verification
    print("\n=== Manual Verification Instructions ===")
    print("1. 🌐 Open web browser and go to: http://localhost:8080/")
    print("2. 📺 Navigate to the video stream or MJPEG endpoint")
    print("3. 👀 Verify you can see ROIs with different colors:")
    print("   - 🟢 Green: Priority 1 (Low)")
    print("   - 🟡 Yellow: Priority 2 (Medium-Low)")
    print("   - 🟠 Orange: Priority 3 (Medium)")
    print("   - 🔴 Red-Orange: Priority 4 (High)")
    print("   - 🔴 Red: Priority 5 (Critical)")
    print("4. ⏰ Watch for time-based ROI toggling")
    print("5. 🏷️ Verify ROI labels show name, priority, and time restrictions")
    
    # Wait for user input before cleanup
    input("\nPress Enter when you've finished testing to clean up...")
    
    # Step 5: Cleanup
    cleanup_test_rois()
    
    print("\n=== ROI Visualization Test Complete ===")
    return True

if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
