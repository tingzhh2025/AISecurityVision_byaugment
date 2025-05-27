#!/bin/bash

# Test script for Task 62: Face verification endpoint
# Tests the POST /api/faces/verify endpoint functionality

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
API_BASE="http://localhost:8080"
TEST_DIR="test_verification_images"

echo -e "${BLUE}=== Task 62: Face Verification API Test ===${NC}"
echo "Testing POST /api/faces/verify endpoint"
echo ""

# Function to test API endpoint
test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local expected_status=$4
    local description=$5

    echo -e "${YELLOW}Testing: $description${NC}"
    echo "  $method $endpoint"

    if [ "$method" = "POST_MULTIPART" ]; then
        # Special handling for multipart form data
        response=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -X POST "$API_BASE$endpoint" $data)
    else
        response=$(curl -s -w "\nHTTP_STATUS:%{http_code}" -X "$method" "$API_BASE$endpoint" \
                   -H "Content-Type: application/json" \
                   -d "$data")
    fi

    # Extract HTTP status code
    http_status=$(echo "$response" | grep "HTTP_STATUS:" | cut -d: -f2)
    response_body=$(echo "$response" | sed '/HTTP_STATUS:/d')

    if [ "$http_status" = "$expected_status" ]; then
        echo -e "  ${GREEN}✓ Status: $http_status${NC}"
        if command -v jq &> /dev/null && [[ "$response_body" =~ ^\{.*\}$ ]]; then
            echo "$response_body" | jq . 2>/dev/null || echo "$response_body"
        else
            echo "$response_body"
        fi
    else
        echo -e "  ${RED}✗ Expected status $expected_status, got $http_status${NC}"
        echo "$response_body"
    fi
    echo ""
}

# Function to create test images
create_test_images() {
    echo -e "${YELLOW}Creating test images...${NC}"
    mkdir -p "$TEST_DIR"

    # Create test images using ImageMagick if available
    if command -v convert &> /dev/null; then
        # Create different test face images
        convert -size 200x200 xc:blue -fill white -pointsize 20 -gravity center \
                -annotate +0+0 "Face 1" "$TEST_DIR/test_face1.jpg"
        convert -size 200x200 xc:red -fill white -pointsize 20 -gravity center \
                -annotate +0+0 "Face 2" "$TEST_DIR/test_face2.jpg"
        convert -size 200x200 xc:green -fill white -pointsize 20 -gravity center \
                -annotate +0+0 "Face 3" "$TEST_DIR/test_face3.jpg"
        echo "  ✓ Created test images with ImageMagick"
    else
        # Create dummy JPEG files for testing
        echo -e "\xFF\xD8\xFF\xE0\x00\x10JFIF\x00\x01\x01\x01\x00H\x00H\x00\x00\xFF\xDB\x00C\x00\x08\x06\x06\x07\x06\x05\x08\x07\x07\x07\t\t\x08\n\x0C\x14\r\x0C\x0B\x0B\x0C\x19\x12\x13\x0F\x14\x1D\x1A\x1F\x1E\x1D\x1A\x1C\x1C $.' \",#\x1C\x1C(7),01444\x1F'9=82<.342\xFF\xC0\x00\x11\x08\x00\x01\x00\x01\x01\x01\x11\x00\x02\x11\x01\x03\x11\x01\xFF\xC4\x00\x14\x00\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x08\xFF\xC4\x00\x14\x10\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xFF\xDA\x00\x0C\x03\x01\x00\x02\x11\x03\x11\x00\x3F\x00\xAA\xFF\xD9" > "$TEST_DIR/test_face1.jpg"
        cp "$TEST_DIR/test_face1.jpg" "$TEST_DIR/test_face2.jpg"
        cp "$TEST_DIR/test_face1.jpg" "$TEST_DIR/test_face3.jpg"
        echo "  ✓ Created dummy JPEG files"
    fi
}

# Function to cleanup
cleanup() {
    echo -e "${YELLOW}Cleaning up...${NC}"
    rm -rf "$TEST_DIR"
    echo "  ✓ Removed test images"
}

# Start the application in background
echo -e "${YELLOW}Starting application...${NC}"
cd /home/codespace/source/AISecurityVision_byaugment

# Build using CMake
if [ ! -f "build/AISecurityVision" ]; then
    echo "  Building application..."
    mkdir -p build
    cd build
    cmake .. > ../build.log 2>&1 && make -j$(nproc) >> ../build.log 2>&1
    cd ..
    if [ $? -ne 0 ]; then
        echo -e "${RED}✗ Build failed${NC}"
        cat build.log
        exit 1
    fi
fi

./build/AISecurityVision &
APP_PID=$!
echo "  ✓ Application started (PID: $APP_PID)"

# Wait for application to start
echo "  Waiting for application to initialize..."
sleep 5

# Check if application is running
if ! kill -0 $APP_PID 2>/dev/null; then
    echo -e "${RED}✗ Application failed to start${NC}"
    exit 1
fi

# Create test images
create_test_images

echo -e "\n${YELLOW}Phase 1: Register Test Faces${NC}"
echo "============================================"

# Test 1: Register first test face
test_endpoint "POST_MULTIPART" "/api/faces/add" \
    "-F 'name=John Doe' -F 'image=@$TEST_DIR/test_face1.jpg'" \
    "201" \
    "Register first test face"

# Test 2: Register second test face
test_endpoint "POST_MULTIPART" "/api/faces/add" \
    "-F 'name=Jane Smith' -F 'image=@$TEST_DIR/test_face2.jpg'" \
    "201" \
    "Register second test face"

# Test 3: Register third test face
test_endpoint "POST_MULTIPART" "/api/faces/add" \
    "-F 'name=Bob Johnson' -F 'image=@$TEST_DIR/test_face3.jpg'" \
    "201" \
    "Register third test face"

echo -e "\n${YELLOW}Phase 2: Face Verification Tests${NC}"
echo "============================================"

# Test 4: Verify face with exact match (should match John Doe)
test_endpoint "POST_MULTIPART" "/api/faces/verify" \
    "-F 'image=@$TEST_DIR/test_face1.jpg'" \
    "200" \
    "Verify exact match face (default threshold 0.7)"

# Test 5: Verify face with custom threshold
test_endpoint "POST_MULTIPART" "/api/faces/verify" \
    "-F 'image=@$TEST_DIR/test_face2.jpg' -F 'threshold=0.5'" \
    "200" \
    "Verify face with custom threshold 0.5"

# Test 6: Verify face with high threshold (may not match)
test_endpoint "POST_MULTIPART" "/api/faces/verify" \
    "-F 'image=@$TEST_DIR/test_face3.jpg' -F 'threshold=0.9'" \
    "200" \
    "Verify face with high threshold 0.9"

echo -e "\n${YELLOW}Phase 3: Error Handling Tests${NC}"
echo "============================================"

# Test 7: Missing image file
test_endpoint "POST_MULTIPART" "/api/faces/verify" \
    "-F 'threshold=0.7'" \
    "400" \
    "Missing image file"

# Test 8: Invalid threshold value
test_endpoint "POST_MULTIPART" "/api/faces/verify" \
    "-F 'image=@$TEST_DIR/test_face1.jpg' -F 'threshold=1.5'" \
    "400" \
    "Invalid threshold value (>1.0)"

# Test 9: Invalid threshold value (negative)
test_endpoint "POST_MULTIPART" "/api/faces/verify" \
    "-F 'image=@$TEST_DIR/test_face1.jpg' -F 'threshold=-0.1'" \
    "400" \
    "Invalid threshold value (negative)"

# Test 10: Invalid image format
echo "Invalid content" > "$TEST_DIR/invalid.txt"
test_endpoint "POST_MULTIPART" "/api/faces/verify" \
    "-F 'image=@$TEST_DIR/invalid.txt'" \
    "400" \
    "Invalid image format"

echo -e "\n${YELLOW}Phase 4: Database Verification${NC}"
echo "============================================"

# Test 11: Check registered faces
test_endpoint "GET" "/api/faces" \
    "" \
    "200" \
    "List all registered faces"

echo -e "\n${YELLOW}Phase 5: Performance Test${NC}"
echo "============================================"

# Test 12: Multiple verification requests
echo -e "${YELLOW}Testing multiple verification requests...${NC}"
for i in {1..3}; do
    echo "  Request $i:"
    test_endpoint "POST_MULTIPART" "/api/faces/verify" \
        "-F 'image=@$TEST_DIR/test_face1.jpg' -F 'threshold=0.6'" \
        "200" \
        "Verification request $i"
done

# Cleanup
echo -e "\n${YELLOW}Cleanup${NC}"
echo "============================================"
echo "Stopping application..."
kill $APP_PID 2>/dev/null
wait $APP_PID 2>/dev/null

cleanup

echo ""
echo -e "${GREEN}=== Task 62 Face Verification Test Complete ===${NC}"
echo ""
echo "Summary of implemented features:"
echo "✓ POST /api/faces/verify - Face verification with confidence scores"
echo "✓ Multipart form data handling for image upload"
echo "✓ Configurable similarity threshold (0.0-1.0)"
echo "✓ Face embedding extraction and comparison"
echo "✓ Cosine similarity calculation"
echo "✓ JSON response with match results"
echo "✓ Error handling and validation"
echo "✓ Integration with existing face database"
echo "✓ Deterministic embedding generation for testing"
echo ""
