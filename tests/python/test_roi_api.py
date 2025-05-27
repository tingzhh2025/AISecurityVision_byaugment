#!/usr/bin/env python3
"""
Test script for ROI Management API - Task 68
Tests all CRUD operations for ROI management with polygon coordinates and metadata.
"""

import requests
import json
import time
import sys

# API Configuration
API_BASE_URL = "http://localhost:8080/api"
CAMERA_ID = "test_camera_01"

def test_roi_api():
    """Test all ROI management API endpoints"""
    print("=== ROI Management API Test - Task 68 ===\n")
    
    # Test data for ROI creation
    test_roi = {
        "id": "test_roi_001",
        "camera_id": CAMERA_ID,
        "name": "Main Entrance Zone",
        "polygon": [
            {"x": 100, "y": 100},
            {"x": 400, "y": 100},
            {"x": 400, "y": 300},
            {"x": 100, "y": 300}
        ],
        "enabled": True,
        "priority": 1
    }
    
    # Test 1: Create ROI (POST /api/rois)
    print("1. Testing ROI Creation (POST /api/rois)")
    try:
        response = requests.post(f"{API_BASE_URL}/rois", 
                               json=test_roi, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 201:
            result = response.json()
            print(f"   ✓ ROI created successfully: {result['roi_id']}")
            print(f"   ✓ Camera: {result['camera_id']}")
            print(f"   ✓ Name: {result['name']}")
            print(f"   ✓ Polygon points: {result['polygon_points']}")
        else:
            print(f"   ✗ Failed: {response.text}")
            return False
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False
    
    print()
    
    # Test 2: Get all ROIs (GET /api/rois)
    print("2. Testing Get All ROIs (GET /api/rois)")
    try:
        response = requests.get(f"{API_BASE_URL}/rois")
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Retrieved {result['count']} ROIs")
            if result['count'] > 0:
                roi = result['rois'][0]
                print(f"   ✓ First ROI: {roi['id']} - {roi['name']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 3: Get ROIs by camera (GET /api/rois?camera_id=...)
    print(f"3. Testing Get ROIs by Camera (GET /api/rois?camera_id={CAMERA_ID})")
    try:
        response = requests.get(f"{API_BASE_URL}/rois", params={"camera_id": CAMERA_ID})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Retrieved {result['count']} ROIs for camera {CAMERA_ID}")
            if result['count'] > 0:
                roi = result['rois'][0]
                print(f"   ✓ ROI: {roi['id']} - {roi['name']}")
                print(f"   ✓ Enabled: {roi['enabled']}")
                print(f"   ✓ Priority: {roi['priority']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 4: Get specific ROI (GET /api/rois/{id})
    print(f"4. Testing Get Specific ROI (GET /api/rois/{test_roi['id']})")
    try:
        response = requests.get(f"{API_BASE_URL}/rois/{test_roi['id']}")
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Retrieved ROI: {result['id']}")
            print(f"   ✓ Name: {result['name']}")
            print(f"   ✓ Camera: {result['camera_id']}")
            print(f"   ✓ Polygon: {len(result['polygon'])} points")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 5: Update ROI (PUT /api/rois/{id})
    print(f"5. Testing ROI Update (PUT /api/rois/{test_roi['id']})")
    updated_roi = test_roi.copy()
    updated_roi["name"] = "Updated Main Entrance Zone"
    updated_roi["priority"] = 2
    updated_roi["polygon"] = [
        {"x": 120, "y": 120},
        {"x": 420, "y": 120},
        {"x": 420, "y": 320},
        {"x": 120, "y": 320}
    ]
    
    try:
        response = requests.put(f"{API_BASE_URL}/rois/{test_roi['id']}", 
                              json=updated_roi, 
                              headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ ROI updated successfully: {result['roi_id']}")
            print(f"   ✓ New name: {result['name']}")
            print(f"   ✓ New priority: {result['priority']}")
            print(f"   ✓ Polygon points: {result['polygon_points']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 6: Create second ROI for testing multiple ROIs
    print("6. Testing Multiple ROIs Creation")
    second_roi = {
        "id": "test_roi_002",
        "camera_id": CAMERA_ID,
        "name": "Parking Area Zone",
        "polygon": [
            {"x": 500, "y": 200},
            {"x": 700, "y": 200},
            {"x": 700, "y": 400},
            {"x": 500, "y": 400}
        ],
        "enabled": True,
        "priority": 3
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois", 
                               json=second_roi, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 201:
            result = response.json()
            print(f"   ✓ Second ROI created: {result['roi_id']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 7: Verify multiple ROIs
    print("7. Testing Multiple ROIs Retrieval")
    try:
        response = requests.get(f"{API_BASE_URL}/rois", params={"camera_id": CAMERA_ID})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Total ROIs for camera: {result['count']}")
            for i, roi in enumerate(result['rois']):
                print(f"   ✓ ROI {i+1}: {roi['id']} - {roi['name']} (Priority: {roi['priority']})")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 8: Delete ROI (DELETE /api/rois/{id})
    print(f"8. Testing ROI Deletion (DELETE /api/rois/{test_roi['id']})")
    try:
        response = requests.delete(f"{API_BASE_URL}/rois/{test_roi['id']}")
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ ROI deleted successfully: {result['roi_id']}")
            print(f"   ✓ Camera: {result['camera_id']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 9: Verify deletion
    print(f"9. Testing Deletion Verification (GET /api/rois/{test_roi['id']})")
    try:
        response = requests.get(f"{API_BASE_URL}/rois/{test_roi['id']}")
        print(f"   Status: {response.status_code}")
        if response.status_code == 404:
            print(f"   ✓ ROI correctly deleted (404 Not Found)")
        else:
            print(f"   ✗ ROI still exists: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 10: Clean up second ROI
    print(f"10. Cleaning up second ROI (DELETE /api/rois/{second_roi['id']})")
    try:
        response = requests.delete(f"{API_BASE_URL}/rois/{second_roi['id']}")
        print(f"    Status: {response.status_code}")
        if response.status_code == 200:
            print(f"    ✓ Second ROI deleted successfully")
        else:
            print(f"    ✗ Failed: {response.text}")
    except Exception as e:
        print(f"    ✗ Error: {e}")
    
    print("\n=== ROI Management API Test Complete ===")
    return True

def test_error_cases():
    """Test error handling and validation"""
    print("\n=== Testing Error Cases ===\n")
    
    # Test invalid polygon
    print("1. Testing Invalid Polygon (less than 3 points)")
    invalid_roi = {
        "id": "invalid_roi",
        "camera_id": CAMERA_ID,
        "name": "Invalid ROI",
        "polygon": [
            {"x": 100, "y": 100},
            {"x": 200, "y": 200}
        ],
        "enabled": True,
        "priority": 1
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois", 
                               json=invalid_roi, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 400:
            print(f"   ✓ Correctly rejected invalid polygon")
        else:
            print(f"   ✗ Should have rejected invalid polygon: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test missing camera_id
    print("2. Testing Missing camera_id")
    no_camera_roi = {
        "id": "no_camera_roi",
        "name": "No Camera ROI",
        "polygon": [
            {"x": 100, "y": 100},
            {"x": 200, "y": 100},
            {"x": 200, "y": 200},
            {"x": 100, "y": 200}
        ],
        "enabled": True,
        "priority": 1
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois", 
                               json=no_camera_roi, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 400:
            print(f"   ✓ Correctly rejected missing camera_id")
        else:
            print(f"   ✗ Should have rejected missing camera_id: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test non-existent ROI
    print("3. Testing Non-existent ROI Retrieval")
    try:
        response = requests.get(f"{API_BASE_URL}/rois/non_existent_roi")
        print(f"   Status: {response.status_code}")
        if response.status_code == 404:
            print(f"   ✓ Correctly returned 404 for non-existent ROI")
        else:
            print(f"   ✗ Should have returned 404: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")

if __name__ == "__main__":
    print("Starting ROI Management API Tests...")
    print("Make sure the AI Security Vision System is running on localhost:8080\n")
    
    # Wait a moment for user to read
    time.sleep(2)
    
    # Run main tests
    success = test_roi_api()
    
    # Run error case tests
    test_error_cases()
    
    print(f"\nTest completed {'successfully' if success else 'with errors'}")
