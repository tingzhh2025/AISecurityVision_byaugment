#!/usr/bin/env python3
"""
Test script for newly implemented API endpoints
"""

import requests
import json
import sys

BASE_URL = "http://localhost:8080"

def test_endpoint(method, path, data=None, expected_status=200):
    """Test a single endpoint"""
    url = BASE_URL + path
    headers = {"Content-Type": "application/json"}
    
    try:
        if method == "GET":
            response = requests.get(url, headers=headers, timeout=10)
        elif method == "POST":
            response = requests.post(url, json=data, headers=headers, timeout=10)
        elif method == "PUT":
            response = requests.put(url, json=data, headers=headers, timeout=10)
        elif method == "DELETE":
            response = requests.delete(url, headers=headers, timeout=10)
        else:
            return False, f"Unknown method: {method}"
        
        success = response.status_code == expected_status
        return success, f"Status: {response.status_code}, Expected: {expected_status}"
        
    except Exception as e:
        return False, f"Error: {str(e)}"

def main():
    print("🔍 Testing New API Endpoints")
    print("=" * 50)
    
    # Test new endpoints
    tests = [
        # Recording Management
        ("GET", "/api/recordings", None, 200),
        ("GET", "/api/recordings/rec_001", None, 200),
        ("GET", "/api/recordings/rec_001/download", None, 200),
        ("DELETE", "/api/recordings/rec_001", None, 200),
        
        # Log Management
        ("GET", "/api/logs", None, 200),
        
        # Statistics
        ("GET", "/api/statistics", None, 200),
        
        # Alert Management (new endpoints)
        ("GET", "/api/alerts/1", None, 200),
        ("PUT", "/api/alerts/1/read", {}, 200),
        ("DELETE", "/api/alerts/1", None, 200),
        
        # Detection endpoints that should now work
        ("PUT", "/api/detection/config", {"confidence_threshold": 0.6}, 200),
        ("GET", "/api/detection/stats", None, 200),
        
        # Camera CRUD that should work
        ("GET", "/api/cameras/test_id", None, 200),
        ("PUT", "/api/cameras/test_id", {"name": "Updated Camera"}, 200),
        ("DELETE", "/api/cameras/test_id", None, 200),
    ]
    
    passed = 0
    failed = 0
    
    for method, path, data, expected_status in tests:
        success, message = test_endpoint(method, path, data, expected_status)
        
        emoji = "✅" if success else "❌"
        print(f"{emoji} {method:6} {path:35} {message}")
        
        if success:
            passed += 1
        else:
            failed += 1
    
    print()
    print("📊 Summary:")
    print(f"  ✅ Passed: {passed}")
    print(f"  ❌ Failed: {failed}")
    print(f"  📈 Success Rate: {passed/(passed+failed)*100:.1f}%")

if __name__ == "__main__":
    main()
