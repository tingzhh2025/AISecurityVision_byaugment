#!/bin/bash

# Task 49: Rule Configuration Integration Test Script
# Tests the integration between API rule management and VideoPipeline's BehaviorAnalyzer
# Validates end-to-end rule configuration workflow

API_BASE="http://localhost:8080"
TEMP_DIR="/tmp/rule_integration_tests"
mkdir -p "$TEMP_DIR"

echo "🧪 Task 49: Rule Configuration Integration Tests"
echo "================================================"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

# Function to wait for API to be ready
wait_for_api() {
    echo -e "${YELLOW}Waiting for API service to be ready...${NC}"
    for i in {1..30}; do
        if curl -s "$API_BASE/api/system/health" > /dev/null 2>&1; then
            echo -e "${GREEN}✅ API service is ready${NC}"
            return 0
        fi
        sleep 1
    done
    echo -e "${RED}❌ API service failed to start${NC}"
    return 1
}

# Function to test rule creation
test_rule_creation() {
    local test_name="$1"
    local rule_json="$2"
    local expected_success="$3"
    
    echo -e "\n${YELLOW}Testing: $test_name${NC}"
    
    # Create rule
    response=$(curl -s -X POST "$API_BASE/api/rules" \
        -H "Content-Type: application/json" \
        -d "$rule_json")
    
    if [ "$expected_success" = "true" ]; then
        if echo "$response" | grep -q '"status":"created"'; then
            echo -e "${GREEN}✅ Rule creation successful${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
            return 0
        else
            echo -e "${RED}❌ Rule creation failed${NC}"
            echo "Response: $response"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            return 1
        fi
    else
        if echo "$response" | grep -q '"error"'; then
            echo -e "${GREEN}✅ Rule creation correctly rejected${NC}"
            TESTS_PASSED=$((TESTS_PASSED + 1))
            return 0
        else
            echo -e "${RED}❌ Rule creation should have failed${NC}"
            echo "Response: $response"
            TESTS_FAILED=$((TESTS_FAILED + 1))
            return 1
        fi
    fi
}

# Function to test rule retrieval
test_rule_retrieval() {
    echo -e "\n${YELLOW}Testing: Rule Retrieval${NC}"
    
    response=$(curl -s -X GET "$API_BASE/api/rules")
    
    if echo "$response" | grep -q '"rules"'; then
        echo -e "${GREEN}✅ Rule retrieval successful${NC}"
        
        # Check if we have rules
        rule_count=$(echo "$response" | grep -o '"count":[0-9]*' | cut -d':' -f2)
        echo "Found $rule_count rules"
        
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}❌ Rule retrieval failed${NC}"
        echo "Response: $response"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Function to test rule update
test_rule_update() {
    local rule_id="$1"
    local updated_rule_json="$2"
    
    echo -e "\n${YELLOW}Testing: Rule Update for $rule_id${NC}"
    
    response=$(curl -s -X PUT "$API_BASE/api/rules/$rule_id" \
        -H "Content-Type: application/json" \
        -d "$updated_rule_json")
    
    if echo "$response" | grep -q '"status":"updated"'; then
        echo -e "${GREEN}✅ Rule update successful${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}❌ Rule update failed${NC}"
        echo "Response: $response"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Function to test rule deletion
test_rule_deletion() {
    local rule_id="$1"
    
    echo -e "\n${YELLOW}Testing: Rule Deletion for $rule_id${NC}"
    
    response=$(curl -s -X DELETE "$API_BASE/api/rules/$rule_id")
    
    if echo "$response" | grep -q '"status":"deleted"'; then
        echo -e "${GREEN}✅ Rule deletion successful${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}❌ Rule deletion failed${NC}"
        echo "Response: $response"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Function to test ROI management
test_roi_management() {
    echo -e "\n${YELLOW}Testing: ROI Management${NC}"
    
    roi_json='{
        "id": "test_roi_integration",
        "name": "Integration Test ROI",
        "polygon": [
            {"x": 100, "y": 100},
            {"x": 400, "y": 100},
            {"x": 400, "y": 400},
            {"x": 100, "y": 400}
        ],
        "enabled": true,
        "priority": 1
    }'
    
    response=$(curl -s -X POST "$API_BASE/api/rois" \
        -H "Content-Type: application/json" \
        -d "$roi_json")
    
    if echo "$response" | grep -q '"status":"created"'; then
        echo -e "${GREEN}✅ ROI creation successful${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
        return 0
    else
        echo -e "${RED}❌ ROI creation failed${NC}"
        echo "Response: $response"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        return 1
    fi
}

# Start the tests
echo -e "\n${BLUE}Starting Rule Configuration Integration Tests...${NC}"

# Wait for API to be ready
if ! wait_for_api; then
    echo -e "${RED}❌ Cannot proceed without API service${NC}"
    exit 1
fi

echo -e "\n📋 Test Suite: Rule Configuration Integration"
echo "=============================================="

# Test 1: Create valid rule
test_rule_creation \
    "Valid Rule Creation" \
    '{
        "id": "integration_test_rule_1",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "integration_test_roi_1",
            "name": "Integration Test Zone",
            "polygon": [
                {"x": 200, "y": 200},
                {"x": 600, "y": 200},
                {"x": 600, "y": 600},
                {"x": 200, "y": 600}
            ],
            "enabled": true,
            "priority": 1
        }
    }' \
    "true"

# Test 2: Create rule with invalid polygon
test_rule_creation \
    "Invalid Polygon Rule Creation" \
    '{
        "id": "integration_test_rule_invalid",
        "min_duration": 5.0,
        "confidence": 0.8,
        "enabled": true,
        "roi": {
            "id": "integration_test_roi_invalid",
            "name": "Invalid Test Zone",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 200, "y": 100}
            ],
            "enabled": true,
            "priority": 1
        }
    }' \
    "false"

# Test 3: Retrieve rules
test_rule_retrieval

# Test 4: Update existing rule
test_rule_update \
    "integration_test_rule_1" \
    '{
        "id": "integration_test_rule_1",
        "min_duration": 10.0,
        "confidence": 0.9,
        "enabled": false,
        "roi": {
            "id": "integration_test_roi_1",
            "name": "Updated Integration Test Zone",
            "polygon": [
                {"x": 150, "y": 150},
                {"x": 550, "y": 150},
                {"x": 550, "y": 550},
                {"x": 150, "y": 550}
            ],
            "enabled": true,
            "priority": 2
        }
    }'

# Test 5: ROI management
test_roi_management

# Test 6: Create another rule to test multiple rules
test_rule_creation \
    "Second Valid Rule Creation" \
    '{
        "id": "integration_test_rule_2",
        "min_duration": 3.0,
        "confidence": 0.7,
        "enabled": true,
        "roi": {
            "id": "integration_test_roi_2",
            "name": "Second Integration Test Zone",
            "polygon": [
                {"x": 300, "y": 300},
                {"x": 700, "y": 300},
                {"x": 700, "y": 700},
                {"x": 300, "y": 700}
            ],
            "enabled": true,
            "priority": 3
        }
    }' \
    "true"

# Test 7: Retrieve rules again to verify multiple rules
test_rule_retrieval

# Test 8: Delete rules
test_rule_deletion "integration_test_rule_1"
test_rule_deletion "integration_test_rule_2"

# Test 9: Verify rules are deleted
echo -e "\n${YELLOW}Testing: Verify Rules Deleted${NC}"
response=$(curl -s -X GET "$API_BASE/api/rules")
rule_count=$(echo "$response" | grep -o '"count":[0-9]*' | cut -d':' -f2)

if [ "$rule_count" = "0" ]; then
    echo -e "${GREEN}✅ All test rules successfully deleted${NC}"
    TESTS_PASSED=$((TESTS_PASSED + 1))
else
    echo -e "${RED}❌ Some test rules still exist (count: $rule_count)${NC}"
    TESTS_FAILED=$((TESTS_FAILED + 1))
fi

# Final results
echo -e "\n📊 Integration Test Results Summary"
echo "==================================="
echo -e "Tests Passed: ${GREEN}$TESTS_PASSED${NC}"
echo -e "Tests Failed: ${RED}$TESTS_FAILED${NC}"
echo -e "Total Tests: $((TESTS_PASSED + TESTS_FAILED))"

if [ $TESTS_FAILED -eq 0 ]; then
    echo -e "\n🎉 ${GREEN}All rule configuration integration tests passed!${NC}"
    echo "✅ Task 49: Rule Configuration Integration - COMPLETED"
    exit 0
else
    echo -e "\n❌ ${RED}Some integration tests failed. Please check the implementation.${NC}"
    exit 1
fi
