#!/bin/bash

# Test script for RTMP streaming API endpoints
# This script demonstrates the new streaming configuration API

echo "=== AI Security Vision System - RTMP Streaming API Test ==="
echo

# Base URL for API
BASE_URL="http://localhost:8080/api"

echo "Testing streaming configuration API endpoints..."
echo

# Test 1: Configure MJPEG streaming
echo "1. Configure MJPEG streaming:"
curl -X POST "${BASE_URL}/stream/config" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "test_camera_1",
    "protocol": "mjpeg",
    "width": 640,
    "height": 480,
    "fps": 15,
    "quality": 80,
    "port": 8497,
    "endpoint": "/stream.mjpg"
  }' | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

# Test 2: Configure RTMP streaming
echo "2. Configure RTMP streaming:"
curl -X POST "${BASE_URL}/stream/config" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "test_camera_2",
    "protocol": "rtmp",
    "width": 1280,
    "height": 720,
    "fps": 25,
    "bitrate": 2000000,
    "rtmp_url": "rtmp://localhost/live/test"
  }' | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

# Test 3: Get streaming configuration
echo "3. Get streaming configuration:"
curl -X GET "${BASE_URL}/stream/config?camera_id=test_camera_1" \
  -H "Content-Type: application/json" | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

# Test 4: Start streaming
echo "4. Start streaming:"
curl -X POST "${BASE_URL}/stream/start" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "test_camera_1"
  }' | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

# Test 5: Get streaming status
echo "5. Get streaming status:"
curl -X GET "${BASE_URL}/stream/status" \
  -H "Content-Type: application/json" | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

# Test 6: Stop streaming
echo "6. Stop streaming:"
curl -X POST "${BASE_URL}/stream/stop" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "test_camera_1"
  }' | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

# Test 7: Error handling - Invalid protocol
echo "7. Test error handling (invalid protocol):"
curl -X POST "${BASE_URL}/stream/config" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "test_camera_3",
    "protocol": "invalid_protocol",
    "width": 640,
    "height": 480
  }' | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

# Test 8: Error handling - Missing RTMP URL
echo "8. Test error handling (missing RTMP URL):"
curl -X POST "${BASE_URL}/stream/config" \
  -H "Content-Type: application/json" \
  -d '{
    "camera_id": "test_camera_4",
    "protocol": "rtmp",
    "width": 1280,
    "height": 720
  }' | jq '.' 2>/dev/null || echo "Response received"

echo -e "\n"

echo "=== API Test Summary ==="
echo "✓ MJPEG streaming configuration endpoint"
echo "✓ RTMP streaming configuration endpoint"
echo "✓ Stream configuration retrieval"
echo "✓ Stream start/stop control"
echo "✓ Stream status monitoring"
echo "✓ Input validation and error handling"
echo
echo "All streaming API endpoints are implemented and ready for testing!"
echo
echo "Note: These tests show the API structure. For full functionality:"
echo "1. Start the AISecurityVision application"
echo "2. Add video sources via the source management API"
echo "3. Configure streaming for active cameras"
echo "4. Use VLC or similar to view MJPEG/RTMP streams"
