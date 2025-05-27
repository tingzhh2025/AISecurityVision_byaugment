#!/usr/bin/env python3
"""
Test script for Bulk ROI Operations API - Task 72
Tests atomic transaction support for bulk ROI configuration updates.
"""

import requests
import json
import time
import sys

# API Configuration
API_BASE_URL = "http://localhost:8080/api"
CAMERA_ID = "test_camera_bulk"

def test_bulk_roi_operations():
    """Test bulk ROI operations with atomic transaction support"""
    print("=== Bulk ROI Operations API Test - Task 72 ===\n")
    
    # Test 1: Successful bulk operations
    print("1. Testing Successful Bulk Operations")
    bulk_operations = {
        "operations": [
            {
                "operation": "create",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_001",
                "name": "Bulk Test Zone 1",
                "polygon": [
                    {"x": 100, "y": 100},
                    {"x": 300, "y": 100},
                    {"x": 300, "y": 200},
                    {"x": 100, "y": 200}
                ],
                "enabled": True,
                "priority": 3,
                "start_time": "09:00",
                "end_time": "17:00"
            },
            {
                "operation": "create",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_002",
                "name": "Bulk Test Zone 2",
                "polygon": [
                    {"x": 400, "y": 150},
                    {"x": 600, "y": 150},
                    {"x": 600, "y": 250},
                    {"x": 400, "y": 250}
                ],
                "enabled": True,
                "priority": 2
            },
            {
                "operation": "create",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_003",
                "name": "Bulk Test Zone 3",
                "polygon": [
                    {"x": 200, "y": 300},
                    {"x": 500, "y": 300},
                    {"x": 500, "y": 400},
                    {"x": 200, "y": 400}
                ],
                "enabled": False,
                "priority": 1
            }
        ]
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                               json=bulk_operations, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Bulk operations successful")
            print(f"   ✓ Created: {result['created']}")
            print(f"   ✓ Updated: {result['updated']}")
            print(f"   ✓ Deleted: {result['deleted']}")
            print(f"   ✓ Total operations: {result['operations_executed']}")
        else:
            print(f"   ✗ Failed: {response.text}")
            return False
    except Exception as e:
        print(f"   ✗ Error: {e}")
        return False
    
    print()
    
    # Test 2: Verify ROIs were created
    print("2. Verifying ROIs were created")
    try:
        response = requests.get(f"{API_BASE_URL}/rois?camera_id={CAMERA_ID}")
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Found {result['count']} ROIs for camera {CAMERA_ID}")
            if result['count'] == 3:
                print("   ✓ All 3 ROIs created successfully")
            else:
                print(f"   ✗ Expected 3 ROIs, found {result['count']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 3: Bulk update operations
    print("3. Testing Bulk Update Operations")
    update_operations = {
        "operations": [
            {
                "operation": "update",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_001",
                "name": "Updated Bulk Test Zone 1",
                "polygon": [
                    {"x": 120, "y": 120},
                    {"x": 320, "y": 120},
                    {"x": 320, "y": 220},
                    {"x": 120, "y": 220}
                ],
                "enabled": True,
                "priority": 4,
                "start_time": "08:00",
                "end_time": "18:00"
            },
            {
                "operation": "update",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_002",
                "name": "Updated Bulk Test Zone 2",
                "polygon": [
                    {"x": 450, "y": 175},
                    {"x": 650, "y": 175},
                    {"x": 650, "y": 275},
                    {"x": 450, "y": 275}
                ],
                "enabled": False,
                "priority": 5
            }
        ]
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                               json=update_operations, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Bulk update successful")
            print(f"   ✓ Updated: {result['updated']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 4: Atomic transaction failure test
    print("4. Testing Atomic Transaction Failure (with invalid data)")
    invalid_operations = {
        "operations": [
            {
                "operation": "create",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_004",
                "name": "Valid ROI",
                "polygon": [
                    {"x": 700, "y": 100},
                    {"x": 800, "y": 100},
                    {"x": 800, "y": 200},
                    {"x": 700, "y": 200}
                ],
                "enabled": True,
                "priority": 3
            },
            {
                "operation": "create",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_005",
                "name": "Invalid ROI",
                "polygon": [
                    {"x": 100, "y": 100}  # Invalid: only 1 point
                ],
                "enabled": True,
                "priority": 10  # Invalid: priority > 5
            }
        ]
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                               json=invalid_operations, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 400:
            result = response.json()
            print(f"   ✓ Validation failed as expected")
            print(f"   ✓ Error code: {result.get('error_code', 'N/A')}")
            print(f"   ✓ Total errors: {result.get('total_errors', 0)}")
            if 'validation_errors' in result:
                print(f"   ✓ Validation errors: {len(result['validation_errors'])}")
        else:
            print(f"   ✗ Expected validation failure, got: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 5: Verify no partial updates occurred
    print("5. Verifying No Partial Updates Occurred")
    try:
        response = requests.get(f"{API_BASE_URL}/rois?camera_id={CAMERA_ID}")
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Found {result['count']} ROIs for camera {CAMERA_ID}")
            if result['count'] == 3:  # Should still be 3, no new ROIs added
                print("   ✓ No partial updates - transaction was properly rolled back")
            else:
                print(f"   ✗ Unexpected ROI count: {result['count']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 6: Mixed operations (create, update, delete)
    print("6. Testing Mixed Operations (Create, Update, Delete)")
    mixed_operations = {
        "operations": [
            {
                "operation": "create",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_006",
                "name": "New Mixed ROI",
                "polygon": [
                    {"x": 50, "y": 50},
                    {"x": 150, "y": 50},
                    {"x": 150, "y": 150},
                    {"x": 50, "y": 150}
                ],
                "enabled": True,
                "priority": 2
            },
            {
                "operation": "update",
                "camera_id": CAMERA_ID,
                "id": "bulk_roi_003",
                "name": "Updated Zone 3",
                "polygon": [
                    {"x": 250, "y": 350},
                    {"x": 550, "y": 350},
                    {"x": 550, "y": 450},
                    {"x": 250, "y": 450}
                ],
                "enabled": True,
                "priority": 3
            },
            {
                "operation": "delete",
                "roi_id": "bulk_roi_002"
            }
        ]
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                               json=mixed_operations, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Mixed operations successful")
            print(f"   ✓ Created: {result['created']}")
            print(f"   ✓ Updated: {result['updated']}")
            print(f"   ✓ Deleted: {result['deleted']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Test 7: Final verification
    print("7. Final ROI Count Verification")
    try:
        response = requests.get(f"{API_BASE_URL}/rois?camera_id={CAMERA_ID}")
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Final ROI count: {result['count']}")
            # Should be 3: bulk_roi_001 (updated), bulk_roi_003 (updated), bulk_roi_006 (new)
            # bulk_roi_002 was deleted
            if result['count'] == 3:
                print("   ✓ Expected final count matches")
            else:
                print(f"   ✗ Expected 3 ROIs, found {result['count']}")
        else:
            print(f"   ✗ Failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Error: {e}")
    
    print()
    
    # Cleanup: Delete all test ROIs
    print("8. Cleanup - Deleting All Test ROIs")
    cleanup_operations = {
        "operations": [
            {"operation": "delete", "roi_id": "bulk_roi_001"},
            {"operation": "delete", "roi_id": "bulk_roi_003"},
            {"operation": "delete", "roi_id": "bulk_roi_006"}
        ]
    }
    
    try:
        response = requests.post(f"{API_BASE_URL}/rois/bulk", 
                               json=cleanup_operations, 
                               headers={"Content-Type": "application/json"})
        print(f"   Status: {response.status_code}")
        if response.status_code == 200:
            result = response.json()
            print(f"   ✓ Cleanup successful - deleted {result['deleted']} ROIs")
        else:
            print(f"   ✗ Cleanup failed: {response.text}")
    except Exception as e:
        print(f"   ✗ Cleanup error: {e}")
    
    print("\n=== Bulk ROI Operations Test Complete ===")
    return True

if __name__ == "__main__":
    success = test_bulk_roi_operations()
    sys.exit(0 if success else 1)
