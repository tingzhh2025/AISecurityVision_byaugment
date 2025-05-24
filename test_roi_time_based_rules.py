#!/usr/bin/env python3
"""
Test script for ROI time-based rule support (Task 70)
Tests time field validation, time-based ROI activation, and API functionality
"""

import requests
import json
import time
from datetime import datetime, timedelta

# Configuration
BASE_URL = "http://localhost:8080"
CAMERA_ID = "test_camera_time"

def test_time_format_validation():
    """Test time format validation for start_time and end_time fields"""
    print("\n=== Testing Time Format Validation ===")
    
    # Test valid time formats
    valid_times = [
        ("09:00", "Valid HH:MM format"),
        ("09:00:00", "Valid HH:MM:SS format"),
        ("23:59", "Valid edge case time"),
        ("00:00:00", "Valid midnight time"),
        ("", "Valid empty time (no restriction)")
    ]
    
    for time_str, description in valid_times:
        roi_data = {
            "id": f"test_roi_valid_{time_str.replace(':', '_')}",
            "camera_id": CAMERA_ID,
            "name": f"Test ROI Valid Time {description}",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 200, "y": 100},
                {"x": 200, "y": 200},
                {"x": 100, "y": 200}
            ],
            "enabled": True,
            "priority": 3,
            "start_time": time_str,
            "end_time": "17:00"
        }
        
        response = requests.post(f"{BASE_URL}/api/rois", json=roi_data)
        if response.status_code == 201:
            print(f"✅ {description}: {time_str}")
            # Clean up
            requests.delete(f"{BASE_URL}/api/rois/{roi_data['id']}")
        else:
            print(f"❌ {description}: {time_str} - {response.text}")
    
    # Test invalid time formats
    invalid_times = [
        ("25:00", "Invalid hour > 23"),
        ("12:60", "Invalid minute > 59"),
        ("12:30:60", "Invalid second > 59"),
        ("abc:def", "Non-numeric format"),
        ("12", "Missing minute"),
        ("12:30:45:67", "Too many components"),
        ("12:3", "Single digit minute without leading zero")
    ]
    
    for time_str, description in invalid_times:
        roi_data = {
            "id": f"test_roi_invalid_{time_str.replace(':', '_')}",
            "camera_id": CAMERA_ID,
            "name": f"Test ROI Invalid Time {description}",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 200, "y": 100},
                {"x": 200, "y": 200},
                {"x": 100, "y": 200}
            ],
            "enabled": True,
            "priority": 3,
            "start_time": time_str,
            "end_time": "17:00"
        }
        
        response = requests.post(f"{BASE_URL}/api/rois", json=roi_data)
        if response.status_code == 400:
            error_data = response.json()
            if error_data.get("error_code") == "INVALID_TIME_FORMAT":
                print(f"✅ {description}: {time_str} - Correctly rejected")
            else:
                print(f"❌ {description}: {time_str} - Wrong error code: {error_data}")
        else:
            print(f"❌ {description}: {time_str} - Should have been rejected but got: {response.status_code}")

def test_time_based_roi_creation():
    """Test creating ROIs with time-based rules"""
    print("\n=== Testing Time-based ROI Creation ===")
    
    # Test ROI with business hours (9 AM to 5 PM)
    business_hours_roi = {
        "id": "business_hours_roi",
        "camera_id": CAMERA_ID,
        "name": "Business Hours Security Zone",
        "polygon": [
            {"x": 50, "y": 50},
            {"x": 150, "y": 50},
            {"x": 150, "y": 150},
            {"x": 50, "y": 150}
        ],
        "enabled": True,
        "priority": 4,
        "start_time": "09:00",
        "end_time": "17:00"
    }
    
    response = requests.post(f"{BASE_URL}/api/rois", json=business_hours_roi)
    if response.status_code == 201:
        result = response.json()
        print(f"✅ Business hours ROI created: {result['roi_id']}")
        print(f"   Start time: {result.get('start_time', 'Not set')}")
        print(f"   End time: {result.get('end_time', 'Not set')}")
    else:
        print(f"❌ Failed to create business hours ROI: {response.text}")
        return
    
    # Test ROI with night shift (10 PM to 6 AM - crosses midnight)
    night_shift_roi = {
        "id": "night_shift_roi",
        "camera_id": CAMERA_ID,
        "name": "Night Shift Security Zone",
        "polygon": [
            {"x": 200, "y": 200},
            {"x": 300, "y": 200},
            {"x": 300, "y": 300},
            {"x": 200, "y": 300}
        ],
        "enabled": True,
        "priority": 5,
        "start_time": "22:00",
        "end_time": "06:00"
    }
    
    response = requests.post(f"{BASE_URL}/api/rois", json=night_shift_roi)
    if response.status_code == 201:
        result = response.json()
        print(f"✅ Night shift ROI created: {result['roi_id']}")
        print(f"   Start time: {result.get('start_time', 'Not set')}")
        print(f"   End time: {result.get('end_time', 'Not set')}")
    else:
        print(f"❌ Failed to create night shift ROI: {response.text}")
        return
    
    # Test ROI without time restrictions
    always_active_roi = {
        "id": "always_active_roi",
        "camera_id": CAMERA_ID,
        "name": "Always Active Security Zone",
        "polygon": [
            {"x": 350, "y": 350},
            {"x": 450, "y": 350},
            {"x": 450, "y": 450},
            {"x": 350, "y": 450}
        ],
        "enabled": True,
        "priority": 3
        # No start_time or end_time - should be always active
    }
    
    response = requests.post(f"{BASE_URL}/api/rois", json=always_active_roi)
    if response.status_code == 201:
        result = response.json()
        print(f"✅ Always active ROI created: {result['roi_id']}")
        print(f"   Start time: {result.get('start_time', 'Not set')}")
        print(f"   End time: {result.get('end_time', 'Not set')}")
    else:
        print(f"❌ Failed to create always active ROI: {response.text}")

def test_roi_retrieval_with_time_fields():
    """Test retrieving ROIs and verifying time fields are included"""
    print("\n=== Testing ROI Retrieval with Time Fields ===")
    
    # Get all ROIs for the camera
    response = requests.get(f"{BASE_URL}/api/rois?camera_id={CAMERA_ID}")
    if response.status_code == 200:
        result = response.json()
        rois = result.get("rois", [])
        print(f"✅ Retrieved {len(rois)} ROIs")
        
        for roi in rois:
            print(f"   ROI: {roi['name']}")
            print(f"     ID: {roi['id']}")
            print(f"     Priority: {roi['priority']}")
            print(f"     Start time: {roi.get('start_time', 'Not set')}")
            print(f"     End time: {roi.get('end_time', 'Not set')}")
            print(f"     Enabled: {roi['enabled']}")
            print()
    else:
        print(f"❌ Failed to retrieve ROIs: {response.text}")

def test_roi_update_with_time_fields():
    """Test updating ROI time fields"""
    print("\n=== Testing ROI Update with Time Fields ===")
    
    # Update the business hours ROI to extend hours
    updated_roi = {
        "camera_id": CAMERA_ID,
        "name": "Extended Business Hours Security Zone",
        "polygon": [
            {"x": 50, "y": 50},
            {"x": 150, "y": 50},
            {"x": 150, "y": 150},
            {"x": 50, "y": 150}
        ],
        "enabled": True,
        "priority": 4,
        "start_time": "08:00",
        "end_time": "18:00"
    }
    
    response = requests.put(f"{BASE_URL}/api/rois/business_hours_roi", json=updated_roi)
    if response.status_code == 200:
        result = response.json()
        print(f"✅ ROI updated: {result['roi_id']}")
        print(f"   New start time: {result.get('start_time', 'Not set')}")
        print(f"   New end time: {result.get('end_time', 'Not set')}")
    else:
        print(f"❌ Failed to update ROI: {response.text}")

def cleanup_test_rois():
    """Clean up test ROIs"""
    print("\n=== Cleaning Up Test ROIs ===")
    
    test_roi_ids = [
        "business_hours_roi",
        "night_shift_roi", 
        "always_active_roi"
    ]
    
    for roi_id in test_roi_ids:
        response = requests.delete(f"{BASE_URL}/api/rois/{roi_id}")
        if response.status_code == 200:
            print(f"✅ Deleted ROI: {roi_id}")
        else:
            print(f"❌ Failed to delete ROI {roi_id}: {response.text}")

def main():
    """Run all time-based ROI tests"""
    print("🚀 Starting ROI Time-based Rules Test Suite (Task 70)")
    print(f"Testing against: {BASE_URL}")
    print(f"Camera ID: {CAMERA_ID}")
    
    try:
        # Test time format validation
        test_time_format_validation()
        
        # Test ROI creation with time fields
        test_time_based_roi_creation()
        
        # Test ROI retrieval
        test_roi_retrieval_with_time_fields()
        
        # Test ROI updates
        test_roi_update_with_time_fields()
        
        # Verify final state
        test_roi_retrieval_with_time_fields()
        
        print("\n✅ All time-based ROI tests completed!")
        
    except Exception as e:
        print(f"\n❌ Test suite failed with error: {e}")
    
    finally:
        # Clean up
        cleanup_test_rois()

if __name__ == "__main__":
    main()
