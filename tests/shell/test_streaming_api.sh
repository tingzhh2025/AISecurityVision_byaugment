#!/bin/bash

# Test script for Task 38: Streaming Configuration API Endpoints
# Tests the newly implemented streaming configuration functionality

set -e

API_BASE="http://localhost:8080"
CAMERA_ID="test_camera_001"

echo "🧪 Testing Task 38: Streaming Configuration API Endpoints"
echo "=========================================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print test results
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✅ PASS${NC}: $2"
    else
        echo -e "${RED}❌ FAIL${NC}: $2"
        return 1
    fi
}

# Function to test API endpoint
test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local expected_status=$4
    local description=$5

    echo -e "\n${BLUE}Testing:${NC} $description"
    echo "Request: $method $endpoint"

    if [ -n "$data" ]; then
        echo "Data: $data"
        response=$(curl -s -w "\n%{http_code}" -X "$method" \
            -H "Content-Type: application/json" \
            -d "$data" \
            "$API_BASE$endpoint" 2>/dev/null || echo -e "\n000")
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" \
            "$API_BASE$endpoint" 2>/dev/null || echo -e "\n000")
    fi

    # Split response and status code
    status_code=$(echo "$response" | tail -n1)
    response_body=$(echo "$response" | head -n -1)

    echo "Response Status: $status_code"
    echo "Response Body: $response_body"

    if [ "$status_code" = "$expected_status" ]; then
        print_result 0 "$description"
        return 0
    else
        print_result 1 "$description (Expected: $expected_status, Got: $status_code)"
        return 1
    fi
}

echo -e "\n${YELLOW}Phase 1: Testing MJPEG Streaming Configuration${NC}"
echo "=============================================="

# Test 1: Configure MJPEG streaming
test_endpoint "POST" "/api/stream/config" \
    '{"camera_id":"'$CAMERA_ID'","protocol":"mjpeg","width":640,"height":480,"fps":15,"quality":80,"port":8001,"endpoint":"/test.mjpg"}' \
    "200" \
    "Configure MJPEG streaming"

# Test 2: Get streaming configuration
test_endpoint "GET" "/api/stream/config" \
    '{"camera_id":"'$CAMERA_ID'"}' \
    "200" \
    "Get streaming configuration"

# Test 3: Start streaming
test_endpoint "POST" "/api/stream/start" \
    '{"camera_id":"'$CAMERA_ID'"}' \
    "200" \
    "Start streaming"

# Test 4: Get stream status
test_endpoint "GET" "/api/stream/status" \
    "" \
    "200" \
    "Get stream status"

echo -e "\n${YELLOW}Phase 2: Testing RTMP Streaming Configuration${NC}"
echo "============================================="

# Test 5: Configure RTMP streaming
test_endpoint "POST" "/api/stream/config" \
    '{"camera_id":"'$CAMERA_ID'","protocol":"rtmp","width":1280,"height":720,"fps":25,"bitrate":2000000,"rtmp_url":"rtmp://localhost/live/test"}' \
    "200" \
    "Configure RTMP streaming"

# Test 6: Get updated streaming configuration
test_endpoint "GET" "/api/stream/config" \
    '{"camera_id":"'$CAMERA_ID'"}' \
    "200" \
    "Get updated streaming configuration"

echo -e "\n${YELLOW}Phase 3: Testing Error Handling${NC}"
echo "================================="

# Test 7: Invalid camera ID
test_endpoint "POST" "/api/stream/config" \
    '{"camera_id":"nonexistent_camera","protocol":"mjpeg","width":640,"height":480,"fps":15}' \
    "404" \
    "Configure streaming for nonexistent camera"

# Test 8: Invalid protocol
test_endpoint "POST" "/api/stream/config" \
    '{"camera_id":"'$CAMERA_ID'","protocol":"invalid","width":640,"height":480,"fps":15}' \
    "400" \
    "Configure streaming with invalid protocol"

# Test 9: Missing required fields
test_endpoint "POST" "/api/stream/config" \
    '{"camera_id":"'$CAMERA_ID'","protocol":"mjpeg"}' \
    "400" \
    "Configure streaming with missing fields"

# Test 10: Invalid resolution
test_endpoint "POST" "/api/stream/config" \
    '{"camera_id":"'$CAMERA_ID'","protocol":"mjpeg","width":100,"height":100,"fps":15}' \
    "400" \
    "Configure streaming with invalid resolution"

# Test 11: Stop streaming
test_endpoint "POST" "/api/stream/stop" \
    '{"camera_id":"'$CAMERA_ID'"}' \
    "200" \
    "Stop streaming"

echo -e "\n${GREEN}🎉 Task 38 Streaming Configuration API Testing Complete!${NC}"
echo "========================================================"

echo -e "\n${BLUE}Summary:${NC}"
echo "✅ MJPEG streaming configuration"
echo "✅ RTMP streaming configuration"
echo "✅ Stream start/stop control"
echo "✅ Stream status monitoring"
echo "✅ Parameter validation"
echo "✅ Error handling"

echo -e "\n${YELLOW}Next Steps:${NC}"
echo "1. Task 39: Integrate BBOX overlays into streaming outputs"
echo "2. Task 50: Create web interface component for ROI polygon drawing"
echo "3. Test streaming with actual video sources"

echo -e "\n${GREEN}Task 38 Implementation Status: ✅ COMPLETED${NC}"
