#!/bin/bash

# Test Face Manager UI - Comprehensive Testing Script
# Tests the complete face management web interface functionality

# Configuration
API_BASE="http://localhost:8080"
WEB_BASE="http://localhost:8080"
TIMEOUT=10

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test results tracking
TESTS_PASSED=0
TESTS_FAILED=0
TOTAL_TESTS=0

# Function to print colored status messages
print_status() {
    local status=$1
    local message=$2
    case $status in
        "PASS")
            echo -e "${GREEN}✓ PASS${NC}: $message"
            ((TESTS_PASSED++))
            ;;
        "FAIL")
            echo -e "${RED}✗ FAIL${NC}: $message"
            ((TESTS_FAILED++))
            ;;
        "INFO")
            echo -e "${BLUE}ℹ INFO${NC}: $message"
            ;;
        "WARN")
            echo -e "${YELLOW}⚠ WARN${NC}: $message"
            ;;
        "TEST")
            echo -e "${BLUE}🧪 TEST${NC}: $message"
            ((TOTAL_TESTS++))
            ;;
    esac
}

# Function to test web page accessibility
test_face_manager_page() {
    print_status "TEST" "Testing Face Manager web page accessibility..."
    
    response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT "$WEB_BASE/face-manager" 2>/dev/null || echo "000")
    status_code=$(echo "$response" | tail -c 4)
    
    if [ "$status_code" = "200" ]; then
        print_status "PASS" "Face Manager page accessible (status: $status_code)"
        
        # Check HTML content
        content=$(echo "$response" | head -n -1)
        if echo "$content" | grep -q "Face Manager"; then
            print_status "PASS" "Page contains Face Manager title"
        else
            print_status "FAIL" "Page missing Face Manager title"
        fi
        
        if echo "$content" | grep -q "face-upload-form"; then
            print_status "PASS" "Page contains upload form"
        else
            print_status "FAIL" "Page missing upload form"
        fi
        
        if echo "$content" | grep -q "faces-grid"; then
            print_status "PASS" "Page contains faces grid"
        else
            print_status "FAIL" "Page missing faces grid"
        fi
        
    else
        print_status "FAIL" "Face Manager page not accessible (status: $status_code)"
    fi
}

# Function to test static file serving
test_static_files() {
    print_status "TEST" "Testing static file serving..."
    
    # Test CSS file
    css_response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT "$WEB_BASE/static/css/face_manager.css" 2>/dev/null || echo "000")
    status_code=$(echo "$css_response" | tail -c 4)
    
    if [ "$status_code" = "200" ]; then
        print_status "PASS" "Face Manager CSS file accessible"
        
        # Check CSS content
        content=$(echo "$css_response" | head -n -1)
        if echo "$content" | grep -q "upload-panel"; then
            print_status "PASS" "CSS contains upload panel styles"
        else
            print_status "FAIL" "CSS missing upload panel styles"
        fi
        
        if echo "$content" | grep -q "face-card"; then
            print_status "PASS" "CSS contains face card styles"
        else
            print_status "FAIL" "CSS missing face card styles"
        fi
        
    else
        print_status "FAIL" "Face Manager CSS not accessible (status: $status_code)"
    fi
    
    # Test JavaScript file
    js_response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT "$WEB_BASE/static/js/face_manager.js" 2>/dev/null || echo "000")
    status_code=$(echo "$js_response" | tail -c 4)
    
    if [ "$status_code" = "200" ]; then
        print_status "PASS" "Face Manager JavaScript file accessible"
        
        # Check JS content
        content=$(echo "$js_response" | head -n -1)
        if echo "$content" | grep -q "FaceManager"; then
            print_status "PASS" "JavaScript contains FaceManager class"
        else
            print_status "FAIL" "JavaScript missing FaceManager class"
        fi
        
        if echo "$content" | grep -q "handleFormSubmit"; then
            print_status "PASS" "JavaScript contains form submission handler"
        else
            print_status "FAIL" "JavaScript missing form submission handler"
        fi
        
    else
        print_status "FAIL" "Face Manager JavaScript not accessible (status: $status_code)"
    fi
}

# Function to test API endpoints
test_api_endpoints() {
    print_status "TEST" "Testing Face Management API endpoints..."
    
    # Test GET /api/faces
    faces_response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT "$API_BASE/api/faces" 2>/dev/null || echo "000")
    status_code=$(echo "$faces_response" | tail -c 4)
    
    if [ "$status_code" = "200" ]; then
        print_status "PASS" "GET /api/faces endpoint accessible"
        
        # Check JSON response
        content=$(echo "$faces_response" | head -n -1)
        if echo "$content" | grep -q '"faces"'; then
            print_status "PASS" "API response contains faces array"
        else
            print_status "FAIL" "API response missing faces array"
        fi
        
        if echo "$content" | grep -q '"count"'; then
            print_status "PASS" "API response contains count field"
        else
            print_status "FAIL" "API response missing count field"
        fi
        
    else
        print_status "FAIL" "GET /api/faces endpoint not accessible (status: $status_code)"
    fi
}

# Function to test file upload simulation
test_upload_simulation() {
    print_status "TEST" "Testing file upload simulation..."
    
    # Create a test image file
    test_image="test_face.jpg"
    
    # Create a simple test image using ImageMagick if available
    if command -v convert >/dev/null 2>&1; then
        convert -size 200x200 xc:lightblue -fill blue -pointsize 30 -gravity center -annotate +0+0 "TEST" "$test_image" 2>/dev/null
        
        if [ -f "$test_image" ]; then
            print_status "PASS" "Test image created successfully"
            
            # Test upload
            upload_response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT \
                -F "name=Test Person" \
                -F "image=@$test_image" \
                "$API_BASE/api/faces/add" 2>/dev/null || echo "000")
            
            status_code=$(echo "$upload_response" | tail -c 4)
            
            if [ "$status_code" = "201" ]; then
                print_status "PASS" "Face upload successful (status: $status_code)"
                
                # Check response content
                content=$(echo "$upload_response" | head -n -1)
                if echo "$content" | grep -q '"face_id"'; then
                    print_status "PASS" "Upload response contains face_id"
                    
                    # Extract face_id for cleanup
                    face_id=$(echo "$content" | grep -o '"face_id":[0-9]*' | grep -o '[0-9]*')
                    if [ -n "$face_id" ]; then
                        print_status "INFO" "Created face with ID: $face_id"
                        
                        # Test deletion
                        delete_response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT \
                            -X DELETE "$API_BASE/api/faces/$face_id" 2>/dev/null || echo "000")
                        
                        delete_status=$(echo "$delete_response" | tail -c 4)
                        if [ "$delete_status" = "204" ]; then
                            print_status "PASS" "Face deletion successful"
                        else
                            print_status "WARN" "Face deletion failed (status: $delete_status)"
                        fi
                    fi
                else
                    print_status "FAIL" "Upload response missing face_id"
                fi
                
            else
                print_status "FAIL" "Face upload failed (status: $status_code)"
                content=$(echo "$upload_response" | head -n -1)
                print_status "INFO" "Response: $content"
            fi
            
            # Cleanup test image
            rm -f "$test_image"
            
        else
            print_status "WARN" "Could not create test image"
        fi
    else
        print_status "WARN" "ImageMagick not available, skipping upload test"
    fi
}

# Function to test navigation links
test_navigation() {
    print_status "TEST" "Testing navigation links..."
    
    # Test dashboard link
    dashboard_response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT "$WEB_BASE/dashboard" 2>/dev/null || echo "000")
    status_code=$(echo "$dashboard_response" | tail -c 4)
    
    if [ "$status_code" = "200" ]; then
        print_status "PASS" "Dashboard navigation link works"
    else
        print_status "FAIL" "Dashboard navigation link broken (status: $status_code)"
    fi
    
    # Test ONVIF discovery link
    onvif_response=$(curl -s -w "%{http_code}" --connect-timeout $TIMEOUT "$WEB_BASE/onvif-discovery" 2>/dev/null || echo "000")
    status_code=$(echo "$onvif_response" | tail -c 4)
    
    if [ "$status_code" = "200" ]; then
        print_status "PASS" "ONVIF discovery navigation link works"
    else
        print_status "FAIL" "ONVIF discovery navigation link broken (status: $status_code)"
    fi
}

# Function to test responsive design elements
test_responsive_design() {
    print_status "TEST" "Testing responsive design elements..."
    
    # Test mobile viewport meta tag
    page_response=$(curl -s --connect-timeout $TIMEOUT "$WEB_BASE/face-manager" 2>/dev/null)
    
    if echo "$page_response" | grep -q 'viewport.*width=device-width'; then
        print_status "PASS" "Page includes mobile viewport meta tag"
    else
        print_status "FAIL" "Page missing mobile viewport meta tag"
    fi
    
    # Test CSS media queries
    css_response=$(curl -s --connect-timeout $TIMEOUT "$WEB_BASE/static/css/face_manager.css" 2>/dev/null)
    
    if echo "$css_response" | grep -q '@media.*max-width'; then
        print_status "PASS" "CSS includes responsive media queries"
    else
        print_status "FAIL" "CSS missing responsive media queries"
    fi
}

# Main test execution
main() {
    echo "=================================================="
    echo "🧪 Face Manager UI Test Suite"
    echo "=================================================="
    echo "Testing Face Manager web interface functionality"
    echo "API Base: $API_BASE"
    echo "Web Base: $WEB_BASE"
    echo "Timeout: ${TIMEOUT}s"
    echo ""
    
    # Run all tests
    test_face_manager_page
    echo ""
    
    test_static_files
    echo ""
    
    test_api_endpoints
    echo ""
    
    test_upload_simulation
    echo ""
    
    test_navigation
    echo ""
    
    test_responsive_design
    echo ""
    
    # Print summary
    echo "=================================================="
    echo "📊 Test Results Summary"
    echo "=================================================="
    echo "Total Tests: $TOTAL_TESTS"
    echo -e "Passed: ${GREEN}$TESTS_PASSED${NC}"
    echo -e "Failed: ${RED}$TESTS_FAILED${NC}"
    
    if [ $TESTS_FAILED -eq 0 ]; then
        echo -e "\n${GREEN}🎉 All tests passed! Face Manager UI is working correctly.${NC}"
        exit 0
    else
        echo -e "\n${RED}❌ Some tests failed. Please check the implementation.${NC}"
        exit 1
    fi
}

# Run main function
main "$@"
