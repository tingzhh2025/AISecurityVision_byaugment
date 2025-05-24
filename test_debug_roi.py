#!/usr/bin/env python3
"""
Debug script to test ROI API with time fields
"""

import requests
import json

# Test simple ROI creation without time fields first
def test_basic_roi():
    print("=== Testing Basic ROI Creation ===")
    
    roi_data = {
        "id": "debug_roi_basic",
        "camera_id": "debug_camera",
        "name": "Debug ROI Basic",
        "polygon": [
            {"x": 100, "y": 100},
            {"x": 200, "y": 100},
            {"x": 200, "y": 200},
            {"x": 100, "y": 200}
        ],
        "enabled": True,
        "priority": 3
    }
    
    print("Request JSON:")
    print(json.dumps(roi_data, indent=2))
    
    response = requests.post("http://localhost:8080/api/rois", json=roi_data)
    print(f"Response Status: {response.status_code}")
    print(f"Response Body: {response.text}")
    
    return response.status_code == 201

def test_roi_with_time():
    print("\n=== Testing ROI with Time Fields ===")
    
    roi_data = {
        "id": "debug_roi_time",
        "camera_id": "debug_camera",
        "name": "Debug ROI with Time",
        "polygon": [
            {"x": 300, "y": 300},
            {"x": 400, "y": 300},
            {"x": 400, "y": 400},
            {"x": 300, "y": 400}
        ],
        "enabled": True,
        "priority": 4,
        "start_time": "09:00",
        "end_time": "17:00"
    }
    
    print("Request JSON:")
    print(json.dumps(roi_data, indent=2))
    
    response = requests.post("http://localhost:8080/api/rois", json=roi_data)
    print(f"Response Status: {response.status_code}")
    print(f"Response Body: {response.text}")
    
    return response.status_code == 201

def test_get_rois():
    print("\n=== Testing Get ROIs ===")
    
    response = requests.get("http://localhost:8080/api/rois?camera_id=debug_camera")
    print(f"Response Status: {response.status_code}")
    print(f"Response Body: {response.text}")

def cleanup():
    print("\n=== Cleanup ===")
    
    roi_ids = ["debug_roi_basic", "debug_roi_time"]
    for roi_id in roi_ids:
        response = requests.delete(f"http://localhost:8080/api/rois/{roi_id}")
        print(f"Delete {roi_id}: {response.status_code}")

if __name__ == "__main__":
    print("🔍 Debug ROI API Test")
    
    # Test basic ROI first
    basic_success = test_basic_roi()
    
    # Test ROI with time fields
    time_success = test_roi_with_time()
    
    # Get all ROIs
    test_get_rois()
    
    # Cleanup
    cleanup()
    
    print(f"\nResults:")
    print(f"Basic ROI: {'✅' if basic_success else '❌'}")
    print(f"Time ROI: {'✅' if time_success else '❌'}")
