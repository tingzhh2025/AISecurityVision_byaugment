#!/usr/bin/env python3
"""
Test script for ROI Priority Validation - Task 69
Tests priority level field validation (1-5 scale) and ordering in API responses.
"""

import requests
import json
import time
import sys

# API Configuration
API_BASE_URL = "http://localhost:8080/api"
CAMERA_ID = "test_camera_priority"

def test_priority_validation():
    """Test ROI priority validation with 1-5 scale"""
    print("=== ROI Priority Validation Test - Task 69 ===\n")
    
    # Test data for different priority levels
    test_rois = [
        {
            "id": "roi_priority_1",
            "camera_id": CAMERA_ID,
            "name": "Low Priority Zone",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 200, "y": 100},
                {"x": 200, "y": 200},
                {"x": 100, "y": 200}
            ],
            "enabled": True,
            "priority": 1  # Valid: Low
        },
        {
            "id": "roi_priority_3",
            "camera_id": CAMERA_ID,
            "name": "Medium Priority Zone",
            "polygon": [
                {"x": 300, "y": 100},
                {"x": 400, "y": 100},
                {"x": 400, "y": 200},
                {"x": 300, "y": 200}
            ],
            "enabled": True,
            "priority": 3  # Valid: Medium
        },
        {
            "id": "roi_priority_5",
            "camera_id": CAMERA_ID,
            "name": "Critical Priority Zone",
            "polygon": [
                {"x": 500, "y": 100},
                {"x": 600, "y": 100},
                {"x": 600, "y": 200},
                {"x": 500, "y": 200}
            ],
            "enabled": True,
            "priority": 5  # Valid: Critical
        }
    ]
    
    # Test invalid priority values
    invalid_priority_tests = [
        {"priority": 0, "description": "Below minimum (0)"},
        {"priority": 6, "description": "Above maximum (6)"},
        {"priority": -1, "description": "Negative value (-1)"},
        {"priority": 10, "description": "Way above maximum (10)"}
    ]
    
    print("1. Testing Valid Priority Levels (1-5)")
    print("=" * 50)
    
    created_rois = []
    
    # Test valid priority levels
    for roi in test_rois:
        print(f"Creating ROI with priority {roi['priority']} ({roi['name']})...")
        
        try:
            response = requests.post(f"{API_BASE_URL}/rois", json=roi, timeout=10)
            
            if response.status_code == 201:
                result = response.json()
                print(f"✅ SUCCESS: ROI created with priority {roi['priority']}")
                print(f"   ROI ID: {result.get('roi_id')}")
                print(f"   Priority: {result.get('priority')}")
                created_rois.append(roi['id'])
            else:
                print(f"❌ FAILED: {response.status_code} - {response.text}")
                
        except requests.exceptions.RequestException as e:
            print(f"❌ REQUEST ERROR: {e}")
        
        print()
    
    print("\n2. Testing Invalid Priority Levels")
    print("=" * 50)
    
    # Test invalid priority levels
    for test_case in invalid_priority_tests:
        invalid_roi = {
            "id": f"roi_invalid_{test_case['priority']}",
            "camera_id": CAMERA_ID,
            "name": f"Invalid Priority Zone ({test_case['priority']})",
            "polygon": [
                {"x": 700, "y": 100},
                {"x": 800, "y": 100},
                {"x": 800, "y": 200},
                {"x": 700, "y": 200}
            ],
            "enabled": True,
            "priority": test_case['priority']
        }
        
        print(f"Testing {test_case['description']}...")
        
        try:
            response = requests.post(f"{API_BASE_URL}/rois", json=invalid_roi, timeout=10)
            
            if response.status_code == 400:
                result = response.json()
                print(f"✅ VALIDATION SUCCESS: Priority {test_case['priority']} correctly rejected")
                print(f"   Error: {result.get('error', 'No error message')}")
                print(f"   Error Code: {result.get('error_code', 'No error code')}")
                if 'provided_priority' in result:
                    print(f"   Provided Priority: {result['provided_priority']}")
                if 'valid_range' in result:
                    print(f"   Valid Range: {result['valid_range']}")
            else:
                print(f"❌ VALIDATION FAILED: Expected 400, got {response.status_code}")
                print(f"   Response: {response.text}")
                
        except requests.exceptions.RequestException as e:
            print(f"❌ REQUEST ERROR: {e}")
        
        print()
    
    print("\n3. Testing Priority Ordering in Responses")
    print("=" * 50)
    
    # Get all ROIs and verify they are ordered by priority (highest first)
    try:
        response = requests.get(f"{API_BASE_URL}/rois?camera_id={CAMERA_ID}", timeout=10)
        
        if response.status_code == 200:
            result = response.json()
            rois = result.get('rois', [])
            
            print(f"Retrieved {len(rois)} ROIs for camera {CAMERA_ID}")
            print("\nROI Priority Ordering:")
            
            previous_priority = 6  # Start with value higher than max
            ordering_correct = True
            
            for i, roi in enumerate(rois):
                priority = roi.get('priority', 0)
                name = roi.get('name', 'Unknown')
                roi_id = roi.get('id', 'Unknown')
                
                print(f"  {i+1}. {name} (ID: {roi_id}) - Priority: {priority}")
                
                if priority > previous_priority:
                    ordering_correct = False
                    print(f"     ❌ ORDERING ERROR: Priority {priority} should not come after {previous_priority}")
                
                previous_priority = priority
            
            if ordering_correct:
                print(f"\n✅ PRIORITY ORDERING CORRECT: ROIs ordered by priority DESC")
            else:
                print(f"\n❌ PRIORITY ORDERING INCORRECT: ROIs not properly ordered")
                
        else:
            print(f"❌ FAILED to retrieve ROIs: {response.status_code} - {response.text}")
            
    except requests.exceptions.RequestException as e:
        print(f"❌ REQUEST ERROR: {e}")
    
    print("\n4. Testing Priority Update Validation")
    print("=" * 50)
    
    # Test updating ROI with invalid priority
    if created_rois:
        test_roi_id = created_rois[0]
        
        update_data = {
            "id": test_roi_id,
            "camera_id": CAMERA_ID,
            "name": "Updated ROI with Invalid Priority",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 200, "y": 100},
                {"x": 200, "y": 200},
                {"x": 100, "y": 200}
            ],
            "enabled": True,
            "priority": 7  # Invalid priority
        }
        
        print(f"Updating ROI {test_roi_id} with invalid priority 7...")
        
        try:
            response = requests.put(f"{API_BASE_URL}/rois/{test_roi_id}", json=update_data, timeout=10)
            
            if response.status_code == 400:
                result = response.json()
                print(f"✅ UPDATE VALIDATION SUCCESS: Invalid priority correctly rejected")
                print(f"   Error: {result.get('error', 'No error message')}")
            else:
                print(f"❌ UPDATE VALIDATION FAILED: Expected 400, got {response.status_code}")
                
        except requests.exceptions.RequestException as e:
            print(f"❌ REQUEST ERROR: {e}")
    
    print("\n5. Cleanup - Deleting Test ROIs")
    print("=" * 50)
    
    # Clean up created ROIs
    for roi_id in created_rois:
        try:
            response = requests.delete(f"{API_BASE_URL}/rois/{roi_id}", timeout=10)
            if response.status_code == 200:
                print(f"✅ Deleted ROI: {roi_id}")
            else:
                print(f"❌ Failed to delete ROI {roi_id}: {response.status_code}")
        except requests.exceptions.RequestException as e:
            print(f"❌ DELETE ERROR for {roi_id}: {e}")
    
    print("\n=== ROI Priority Validation Test Complete ===")

if __name__ == "__main__":
    test_priority_validation()
