#!/usr/bin/env python3
"""
Quick API endpoint checker for AI Security Vision System
"""

import requests
import json
import sys

BASE_URL = "http://localhost:8080"

# Test endpoints with expected status
ENDPOINTS = [
    # Authentication (should work)
    ("POST", "/api/auth/login", {"username": "admin", "password": "admin123"}, 200),
    ("GET", "/api/auth/user", None, 200),  # Requires token
    ("POST", "/api/auth/logout", {"token": "test"}, 200),
    
    # System (should work)
    ("GET", "/api/system/status", None, 200),
    ("GET", "/api/system/info", None, 200),
    
    # Cameras (should work)
    ("GET", "/api/cameras", None, 200),
    ("GET", "/api/cameras/config", None, 200),
    
    # Detection (should work)
    ("GET", "/api/detection/categories", None, 200),
    ("GET", "/api/detection/config", None, 200),
    
    # Unimplemented (should return 501)
    ("GET", "/api/recordings", None, 501),
    ("GET", "/api/logs", None, 501),
    ("GET", "/api/statistics", None, 501),
    
    # Camera CRUD (need to check)
    ("GET", "/api/cameras/test_id", None, None),  # Check status
    ("PUT", "/api/cameras/test_id", {"name": "test"}, None),
    ("DELETE", "/api/cameras/test_id", None, None),
]

def test_endpoint(method, path, data, expected_status):
    """Test a single endpoint"""
    url = BASE_URL + path
    headers = {"Content-Type": "application/json"}
    
    # Get token for authenticated endpoints
    token = None
    if path.startswith("/api/auth/user"):
        # Login first to get token
        login_response = requests.post(
            BASE_URL + "/api/auth/login",
            json={"username": "admin", "password": "admin123"},
            headers=headers
        )
        if login_response.status_code == 200:
            token = login_response.json()["data"]["token"]
            headers["Authorization"] = f"Bearer {token}"
    
    try:
        if method == "GET":
            response = requests.get(url, headers=headers, timeout=5)
        elif method == "POST":
            response = requests.post(url, json=data, headers=headers, timeout=5)
        elif method == "PUT":
            response = requests.put(url, json=data, headers=headers, timeout=5)
        elif method == "DELETE":
            response = requests.delete(url, headers=headers, timeout=5)
        else:
            return "UNKNOWN", f"Unknown method: {method}"
        
        status = response.status_code
        
        if expected_status is None:
            return "CHECK", f"Status: {status}"
        elif status == expected_status:
            return "PASS", f"Status: {status}"
        else:
            return "FAIL", f"Expected: {expected_status}, Got: {status}"
            
    except Exception as e:
        return "ERROR", str(e)

def main():
    print("🔍 Quick API Endpoint Check")
    print("=" * 50)
    
    # Check if service is running
    try:
        response = requests.get(BASE_URL + "/api/system/status", timeout=5)
        if response.status_code != 200:
            print("❌ Service not accessible")
            sys.exit(1)
    except:
        print("❌ Service not running")
        sys.exit(1)
    
    print("✅ Service is running")
    print()
    
    # Test endpoints
    results = {"PASS": 0, "FAIL": 0, "ERROR": 0, "CHECK": 0}
    
    for method, path, data, expected_status in ENDPOINTS:
        result, message = test_endpoint(method, path, data, expected_status)
        results[result] += 1
        
        emoji = {"PASS": "✅", "FAIL": "❌", "ERROR": "⚠️", "CHECK": "🔍"}[result]
        print(f"{emoji} {method:6} {path:30} {message}")
    
    print()
    print("📊 Summary:")
    print(f"  ✅ Passed: {results['PASS']}")
    print(f"  ❌ Failed: {results['FAIL']}")
    print(f"  ⚠️  Errors: {results['ERROR']}")
    print(f"  🔍 Check:  {results['CHECK']}")
    
    total = sum(results.values())
    success_rate = (results['PASS'] / total * 100) if total > 0 else 0
    print(f"  📈 Success Rate: {success_rate:.1f}%")

if __name__ == "__main__":
    main()
