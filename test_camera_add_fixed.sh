#!/bin/bash

# Simple test script for adding three RTSP cameras one by one
# Tests: 192.168.1.2, 192.168.1.3, 192.168.1.226

API_BASE="http://localhost:8080/api"

echo "=== Simple Multi-Camera Test ==="

# Function to check if backend is running
check_backend() {
    echo "Checking if backend is running..."
    if curl -s "${API_BASE}/system/status" > /dev/null 2>&1; then
        echo "✓ Backend is running"
        return 0
    else
        echo "✗ Backend is not running. Please start the backend first."
        echo "Run: cd build && ./AISecurityVision"
        return 1
    fi
}

# Function to wait for user input
wait_for_user() {
    echo ""
    read -p "Press Enter to continue or Ctrl+C to exit..."
    echo ""
}

# Function to add a camera and check result
add_camera_with_check() {
    local camera_id="$1"
    local camera_name="$2"
    local camera_url="$3"
    local username="$4"
    local password="$5"
    local port="$6"

    echo "Adding $camera_name..."
    echo "URL: $camera_url"
    echo "Port: $port"

    # Create JSON payload
    local json_payload="{
        \"id\": \"$camera_id\",
        \"name\": \"$camera_name\",
        \"url\": \"$camera_url\",
        \"protocol\": \"rtsp\",
        \"username\": \"$username\",
        \"password\": \"$password\",
        \"width\": 1920,
        \"height\": 1080,
        \"fps\": 25,
        \"mjpeg_port\": $port,
        \"enabled\": true
    }"

    # Add camera
    echo "Sending request..."
    RESPONSE=$(curl -s -w "HTTPSTATUS:%{http_code}" -X POST "${API_BASE}/cameras" \
        -H "Content-Type: application/json" \
        -d "$json_payload")

    HTTP_STATUS=$(echo $RESPONSE | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
    RESPONSE_BODY=$(echo $RESPONSE | sed -e 's/HTTPSTATUS:.*//g')

    echo "HTTP Status: $HTTP_STATUS"
    if [ "$HTTP_STATUS" -eq 200 ] || [ "$HTTP_STATUS" -eq 201 ]; then
        echo "✓ Camera added successfully"
    else
        echo "✗ Failed to add camera"
        echo "Response: $RESPONSE_BODY"
        return 1
    fi

    echo "Waiting 5 seconds for pipeline initialization..."
    sleep 5

    # Check if backend is still running
    if ! check_backend; then
        echo "✗ Backend crashed after adding camera!"
        return 1
    fi

    echo "✓ Camera $camera_name added successfully"
    return 0
}

# Check backend status
if ! check_backend; then
    exit 1
fi

echo ""
echo "=== Adding Three Test Cameras ==="

# Test 1: Add first camera (192.168.1.2)
echo "=== Test 1: Camera 192.168.1.2 ==="
wait_for_user
add_camera_with_check "camera_192_168_1_2" "Camera-192.168.1.2" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" "admin" "sharpi1688" "8161"
if [ $? -ne 0 ]; then
    echo "Failed to add Camera 1. Exiting."
    exit 1
fi

# Test 2: Add second camera (192.168.1.3)
echo "=== Test 2: Camera 192.168.1.3 ==="
wait_for_user
add_camera_with_check "camera_192_168_1_3" "Camera-192.168.1.3" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" "admin" "sharpi1688" "8162"
if [ $? -ne 0 ]; then
    echo "Failed to add Camera 2. Exiting."
    exit 1
fi

# Test 3: Add third camera (192.168.1.226)
echo "=== Test 3: Camera 192.168.1.226 ==="
wait_for_user
add_camera_with_check "camera_192_168_1_226" "Camera-192.168.1.226" "rtsp://192.168.1.226:8554/unicast" "" "" "8163"
if [ $? -ne 0 ]; then
    echo "Failed to add Camera 3. Exiting."
    exit 1
fi

echo ""
echo "=== Final Verification ==="
wait_for_user

# Check camera list
echo "Checking camera list..."
CAMERAS=$(curl -s "${API_BASE}/cameras")
CAMERA_COUNT=$(echo "$CAMERAS" | grep -o '"id"' | wc -l)
echo "Total cameras registered: $CAMERA_COUNT"

# Check system status
echo "Checking system status..."
STATUS=$(curl -s "${API_BASE}/system/status")
if echo "$STATUS" | grep -q "active_pipelines"; then
    PIPELINES=$(echo "$STATUS" | grep -o '"active_pipelines":[0-9]*' | cut -d':' -f2)
    echo "Active pipelines: $PIPELINES"
else
    echo "Could not get system status"
fi

echo ""
echo "=== Stream Access Information ==="
echo "Camera 1 (192.168.1.2): http://localhost:8161"
echo "Camera 2 (192.168.1.3): http://localhost:8162"
echo "Camera 3 (192.168.1.226): http://localhost:8163"
echo ""
echo "Web UI: http://localhost:3000"
echo ""
echo "=== Test Complete ==="
echo "All three cameras have been added successfully!"
