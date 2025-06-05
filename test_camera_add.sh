#!/bin/bash

# Test script for camera addition functionality
# This script tests the camera addition API and verifies video preview

echo "=== Camera Addition Test ==="

# API base URL
API_URL="http://localhost:8080/api"

# Test camera configuration
CAMERA_CONFIG='{
  "id": "test_camera_new",
  "name": "Test Camera New",
  "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
  "protocol": "rtsp",
  "username": "admin",
  "password": "sharpi1688",
  "width": 1920,
  "height": 1080,
  "fps": 25,
  "mjpeg_port": 8169,
  "enabled": true
}'

echo "1. Testing camera addition..."
curl -X POST \
  -H "Content-Type: application/json" \
  -d "$CAMERA_CONFIG" \
  "$API_URL/cameras" \
  -w "\nHTTP Status: %{http_code}\n"

echo -e "\n2. Waiting for pipeline to start..."
sleep 5

echo "3. Getting camera list..."
curl -X GET "$API_URL/cameras" | jq '.'

echo -e "\n4. Testing MJPEG stream availability..."
curl -I "http://localhost:8169/stream.mjpg" -w "\nHTTP Status: %{http_code}\n"

echo -e "\n5. Testing camera configuration save..."
CAMERA_DB_CONFIG='{
  "camera_id": "test_camera_new",
  "name": "Test Camera New",
  "rtsp_url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
  "protocol": "rtsp",
  "username": "admin",
  "password": "sharpi1688",
  "width": 1920,
  "height": 1080,
  "fps": 25,
  "mjpeg_port": 8169,
  "enabled": true,
  "detection_enabled": true,
  "recording_enabled": false,
  "detection_config": {
    "confidence_threshold": 0.5,
    "nms_threshold": 0.4,
    "backend": "RKNN",
    "model_path": "models/yolov8n.rknn"
  },
  "stream_config": {
    "fps": 25,
    "quality": 80,
    "max_width": 1920,
    "max_height": 1080
  }
}'

curl -X POST \
  -H "Content-Type: application/json" \
  -d "$CAMERA_DB_CONFIG" \
  "$API_URL/cameras/config" \
  -w "\nHTTP Status: %{http_code}\n"

echo -e "\n6. Getting camera configurations from database..."
curl -X GET "$API_URL/cameras/config" | jq '.'

echo -e "\n=== Test Complete ==="
echo "Check the above output for:"
echo "- Camera addition should return HTTP 200"
echo "- Camera should appear in camera list with 'online' or 'configured' status"
echo "- MJPEG stream should be available (HTTP 200)"
echo "- Camera config should be saved to database (HTTP 200)"
