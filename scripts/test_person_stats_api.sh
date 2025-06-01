#!/bin/bash

# Person Statistics API Testing Script
# ====================================
# 
# This script demonstrates how to use the new person statistics API endpoints
# for the AI Security Vision system.
#
# Prerequisites:
# - AI Security Vision system running on localhost:8080
# - At least one camera configured (e.g., camera1)
# - curl command available
#
# Usage:
#   ./test_person_stats_api.sh [camera_id]
#
# Example:
#   ./test_person_stats_api.sh camera1

set -e  # Exit on any error

# Configuration
API_BASE="http://localhost:8080/api"
CAMERA_ID="${1:-camera1}"  # Default to camera1 if not specified

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Helper functions
print_header() {
    echo -e "\n${BLUE}=== $1 ===${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

# Test API endpoint with error handling
test_api() {
    local method="$1"
    local endpoint="$2"
    local data="$3"
    local description="$4"
    
    echo -e "\n${YELLOW}Testing: $description${NC}"
    echo "Endpoint: $method $endpoint"
    
    if [ "$method" = "GET" ]; then
        response=$(curl -s -w "\nHTTP_CODE:%{http_code}" "$endpoint" || echo "CURL_ERROR")
    else
        response=$(curl -s -w "\nHTTP_CODE:%{http_code}" -X "$method" -H "Content-Type: application/json" -d "$data" "$endpoint" || echo "CURL_ERROR")
    fi
    
    if [[ "$response" == *"CURL_ERROR"* ]]; then
        print_error "Failed to connect to API"
        return 1
    fi
    
    http_code=$(echo "$response" | tail -n1 | sed 's/HTTP_CODE://')
    json_response=$(echo "$response" | sed '$d')
    
    echo "HTTP Code: $http_code"
    echo "Response: $json_response" | python3 -m json.tool 2>/dev/null || echo "$json_response"
    
    if [[ "$http_code" =~ ^2[0-9][0-9]$ ]]; then
        print_success "Success"
        return 0
    else
        print_error "Failed (HTTP $http_code)"
        return 1
    fi
}

# Main testing sequence
main() {
    print_header "Person Statistics API Testing"
    echo "Camera ID: $CAMERA_ID"
    echo "API Base: $API_BASE"
    
    # Test 1: Check current person statistics (should be disabled initially)
    print_header "1. Get Current Person Statistics"
    test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats" "" "Get current person statistics"
    
    # Test 2: Get person statistics configuration
    print_header "2. Get Person Statistics Configuration"
    test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats/config" "" "Get person statistics configuration"
    
    # Test 3: Enable person statistics
    print_header "3. Enable Person Statistics"
    test_api "POST" "$API_BASE/cameras/$CAMERA_ID/person-stats/enable" "{}" "Enable person statistics"
    
    # Test 4: Check statistics after enabling
    print_header "4. Get Statistics After Enabling"
    test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats" "" "Get person statistics after enabling"
    
    # Test 5: Update configuration
    print_header "5. Update Configuration"
    config_data='{"enabled": true, "gender_threshold": 0.8, "age_threshold": 0.7}'
    test_api "POST" "$API_BASE/cameras/$CAMERA_ID/person-stats/config" "$config_data" "Update person statistics configuration"
    
    # Test 6: Get updated configuration
    print_header "6. Get Updated Configuration"
    test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats/config" "" "Get updated configuration"
    
    # Test 7: Disable person statistics
    print_header "7. Disable Person Statistics"
    test_api "POST" "$API_BASE/cameras/$CAMERA_ID/person-stats/disable" "{}" "Disable person statistics"
    
    # Test 8: Check statistics after disabling
    print_header "8. Get Statistics After Disabling"
    test_api "GET" "$API_BASE/cameras/$CAMERA_ID/person-stats" "" "Get person statistics after disabling"
    
    # Test 9: Test with invalid camera ID
    print_header "9. Test Invalid Camera ID"
    test_api "GET" "$API_BASE/cameras/invalid_camera/person-stats" "" "Test with invalid camera ID (should fail)"
    
    print_header "Testing Complete"
    print_success "All person statistics API tests completed!"
    
    echo -e "\n${BLUE}Usage Examples:${NC}"
    echo "# Enable person statistics for a camera"
    echo "curl -X POST $API_BASE/cameras/$CAMERA_ID/person-stats/enable"
    echo ""
    echo "# Get real-time person statistics"
    echo "curl $API_BASE/cameras/$CAMERA_ID/person-stats"
    echo ""
    echo "# Update configuration"
    echo "curl -X POST -H 'Content-Type: application/json' \\"
    echo "  -d '{\"enabled\": true, \"gender_threshold\": 0.8}' \\"
    echo "  $API_BASE/cameras/$CAMERA_ID/person-stats/config"
    echo ""
    echo "# Disable person statistics"
    echo "curl -X POST $API_BASE/cameras/$CAMERA_ID/person-stats/disable"
}

# Check dependencies
check_dependencies() {
    if ! command -v curl &> /dev/null; then
        print_error "curl is required but not installed"
        exit 1
    fi
    
    if ! command -v python3 &> /dev/null; then
        print_warning "python3 not found, JSON formatting will be disabled"
    fi
}

# Check if API is accessible
check_api_accessibility() {
    print_header "Checking API Accessibility"
    
    if ! curl -s --connect-timeout 5 "$API_BASE/system/status" > /dev/null; then
        print_error "Cannot connect to API at $API_BASE"
        print_error "Please ensure the AI Security Vision system is running"
        exit 1
    fi
    
    print_success "API is accessible"
}

# Script entry point
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    # Check if help is requested
    if [[ "$1" == "-h" || "$1" == "--help" ]]; then
        echo "Person Statistics API Testing Script"
        echo ""
        echo "Usage: $0 [camera_id]"
        echo ""
        echo "Arguments:"
        echo "  camera_id    Camera ID to test (default: camera1)"
        echo ""
        echo "Options:"
        echo "  -h, --help   Show this help message"
        echo ""
        echo "Examples:"
        echo "  $0                    # Test with default camera (camera1)"
        echo "  $0 camera2           # Test with camera2"
        echo "  $0 test_camera       # Test with test_camera"
        exit 0
    fi
    
    # Run the tests
    check_dependencies
    check_api_accessibility
    main
fi
