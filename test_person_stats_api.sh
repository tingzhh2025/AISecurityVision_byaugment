#!/bin/bash

# Test Person Statistics API
echo "=== Testing Person Statistics API ==="

API_BASE="http://localhost:8080/api"
CAMERA_ID="test_camera"

# Function to test API endpoint
test_api() {
    local method="$1"
    local endpoint="$2"
    local data="$3"
    local description="$4"
    
    echo ""
    echo "Testing: $description"
    echo "Endpoint: $method $endpoint"
    
    if [ "$method" = "GET" ]; then
        response=$(curl -s -w "\nHTTP_CODE:%{http_code}" "$endpoint" 2>/dev/null)
    else
        response=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X "$method" -H "Content-Type: application/json" -d "$data" "$endpoint" 2>/dev/null)
    fi
    
    if [ $? -eq 0 ]; then
        http_code=$(echo "$response" | tail -n1 | sed 's/HTTP_CODE://')
        json_response=$(echo "$response" | sed '$d')
        
        echo "HTTP Code: $http_code"
        echo "Response: $json_response"
        
        if [[ "$http_code" =~ ^2[0-9][0-9]$ ]]; then
            echo "✓ Success"
        else
            echo "✗ Failed (HTTP $http_code)"
        fi
    else
        echo "✗ Connection failed"
    fi
}

# First, let's add a test camera
echo "=== Step 1: Adding Test Camera ==="
camera_config='{
    "id": "test_camera",
    "name": "Test Camera",
    "url": "rtsp://admin:sharpi1688@192.168.1.3:554/1/1",
    "protocol": "rtsp",
    "username": "admin",
    "password": "sharpi1688",
    "width": 1920,
    "height": 1080,
    "fps": 25,
    "mjpeg_port": 8163,
    "enabled": true
}'

test_api "POST" "$API_BASE/cameras" "$camera_config" "Add test camera"

# Test person statistics endpoints
echo ""
echo "=== Step 2: Testing Person Statistics Endpoints ==="

test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats" "" "Get current person statistics"

test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats/config" "" "Get person statistics configuration"

test_api "POST" "$API_BASE/cameras/$CAMERA_ID/person-stats/enable" "{}" "Enable person statistics"

test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats" "" "Get person statistics after enabling"

config_data='{"enabled": true, "gender_threshold": 0.8, "age_threshold": 0.7}'
test_api "POST" "$API_BASE/cameras/$CAMERA_ID/person-stats/config" "$config_data" "Update person statistics configuration"

test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats/config" "" "Get updated configuration"

test_api "POST" "$API_BASE/cameras/$CAMERA_ID/person-stats/disable" "{}" "Disable person statistics"

test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats" "" "Get person statistics after disabling"

echo ""
echo "=== API Testing Complete ==="
