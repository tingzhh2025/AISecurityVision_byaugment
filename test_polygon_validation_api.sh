#!/bin/bash

# Enhanced ROI Polygon Validation API Test Script
# Tests various invalid polygon scenarios through REST API endpoints
# Validates detailed error reporting and validation codes

API_BASE="http://localhost:8080"
TEMP_DIR="/tmp/polygon_validation_tests"
mkdir -p "$TEMP_DIR"

echo "🧪 Enhanced ROI Polygon Validation API Tests"
echo "============================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to test API endpoint with expected failure
test_invalid_polygon() {
    local test_name="$1"
    local endpoint="$2"
    local json_payload="$3"
    local expected_error_code="$4"
    
    echo -e "\n${YELLOW}Testing: $test_name${NC}"
    echo "Endpoint: $endpoint"
    echo "Payload: $json_payload"
    
    # Make API request
    response=$(curl -s -X POST "$API_BASE$endpoint" \
        -H "Content-Type: application/json" \
        -d "$json_payload")
    
    # Check if response contains error
    if echo "$response" | grep -q '"error"'; then
        echo -e "${GREEN}✅ API correctly rejected invalid polygon${NC}"
        
        # Check for specific error code if provided
        if [ -n "$expected_error_code" ]; then
            if echo "$response" | grep -q "\"error_code\":\"$expected_error_code\""; then
                echo -e "${GREEN}✅ Correct error code: $expected_error_code${NC}"
                TESTS_PASSED=$((TESTS_PASSED + 1))
            else
                echo -e "${RED}❌ Expected error code '$expected_error_code' not found${NC}"
                echo "Response: $response"
                TESTS_FAILED=$((TESTS_FAILED + 1))
            fi
        else
            TESTS_PASSED=$((TESTS_PASSED + 1))
        fi
        
        # Show validation details if present
        if echo "$response" | grep -q '"validation_details"'; then
            echo "Validation details found in response"
        fi
        
    else
        echo -e "${RED}❌ API should have rejected invalid polygon${NC}"
        echo "Response: $response"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

# Function to test valid polygon
test_valid_polygon() {
    local test_name="$1"
    local endpoint="$2"
    local json_payload="$3"
    
    echo -e "\n${YELLOW}Testing: $test_name${NC}"
    echo "Endpoint: $endpoint"
    
    # Make API request
    response=$(curl -s -X POST "$API_BASE$endpoint" \
        -H "Content-Type: application/json" \
        -d "$json_payload")
    
    # Check if response indicates success
    if echo "$response" | grep -q '"status":"created"' || echo "$response" | grep -q '"status":"updated"'; then
        echo -e "${GREEN}✅ API correctly accepted valid polygon${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
    else
        echo -e "${RED}❌ API should have accepted valid polygon${NC}"
        echo "Response: $response"
        TESTS_FAILED=$((TESTS_FAILED + 1))
    fi
}

echo -e "\n📋 Starting API Tests..."

# Test 1: Insufficient points (< 3 points)
test_invalid_polygon \
    "Insufficient Points" \
    "/api/rules" \
    '{
        "id": "test_rule_1",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "test_roi_1",
            "name": "Test ROI",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 200, "y": 100}
            ],
            "enabled": true,
            "priority": 1
        }
    }' \
    "INSUFFICIENT_POINTS"

# Test 2: Coordinates out of range
test_invalid_polygon \
    "Coordinates Out of Range" \
    "/api/rules" \
    '{
        "id": "test_rule_2",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "test_roi_2",
            "name": "Test ROI",
            "polygon": [
                {"x": -10, "y": 100},
                {"x": 200, "y": 100},
                {"x": 150, "y": 200}
            ],
            "enabled": true,
            "priority": 1
        }
    }' \
    "COORDINATE_OUT_OF_RANGE"

# Test 3: Area too small (collinear points)
test_invalid_polygon \
    "Area Too Small" \
    "/api/rules" \
    '{
        "id": "test_rule_3",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "test_roi_3",
            "name": "Test ROI",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 101, "y": 100},
                {"x": 100, "y": 101}
            ],
            "enabled": true,
            "priority": 1
        }
    }' \
    "AREA_TOO_SMALL"

# Test 4: Self-intersecting polygon (bowtie)
test_invalid_polygon \
    "Self-Intersecting Polygon" \
    "/api/rules" \
    '{
        "id": "test_rule_4",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "test_roi_4",
            "name": "Test ROI",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 200, "y": 200},
                {"x": 200, "y": 100},
                {"x": 100, "y": 200}
            ],
            "enabled": true,
            "priority": 1
        }
    }' \
    "SELF_INTERSECTION"

# Test 5: Valid polygon (should succeed)
test_valid_polygon \
    "Valid Rectangle" \
    "/api/rules" \
    '{
        "id": "test_rule_5",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "test_roi_5",
            "name": "Valid Test ROI",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 300, "y": 100},
                {"x": 300, "y": 300},
                {"x": 100, "y": 300}
            ],
            "enabled": true,
            "priority": 1
        }
    }'

# Test 6: ROI endpoint with invalid polygon
test_invalid_polygon \
    "ROI Endpoint - Invalid Polygon" \
    "/api/rois" \
    '{
        "id": "test_roi_6",
        "name": "Invalid ROI",
        "polygon": [
            {"x": 100, "y": 100}
        ],
        "enabled": true,
        "priority": 1
    }' \
    "INSUFFICIENT_POINTS"

# Test 7: PUT endpoint with invalid polygon
test_invalid_polygon \
    "PUT Rule - Invalid Polygon" \
    "/api/rules/test_rule_7" \
    '{
        "id": "test_rule_7",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "test_roi_7",
            "name": "Test ROI",
            "polygon": [
                {"x": 15000, "y": 100},
                {"x": 200, "y": 100},
                {"x": 150, "y": 200}
            ],
            "enabled": true,
            "priority": 1
        }
    }' \
    "COORDINATE_OUT_OF_RANGE"

echo -e "\n📊 Test Results Summary"
echo "======================="
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
echo -e "Total Tests: $((TESTS_PASSED + TESTS_FAILED))"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n🎉 ${GREEN}All polygon validation API tests passed!${NC}"
    echo "✅ Task 48: Enhanced ROI Polygon Validation - API Integration Complete"
    exit 0
else
    echo -e "\n❌ ${RED}Some tests failed. Please check the implementation.${NC}"
    exit 1
fi
