#!/bin/bash

# Quick API Test Script for AI Security Vision
# ============================================
# This script performs basic API endpoint testing using curl

set -e

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
CYAN='\033[0;36m'
BOLD='\033[1m'
NC='\033[0m'

# Configuration
BASE_URL="http://localhost:8080"
TIMEOUT=10

# Counters
TOTAL_TESTS=0
PASSED_TESTS=0
FAILED_TESTS=0
NOT_IMPLEMENTED=0

echo -e "${BOLD}${BLUE}Quick API Test for AI Security Vision${NC}"
echo -e "${BLUE}====================================${NC}"
echo ""

# Test function
test_endpoint() {
    local method="$1"
    local endpoint="$2"
    local description="$3"
    local data="$4"
    
    TOTAL_TESTS=$((TOTAL_TESTS + 1))
    
    printf "%-8s %-40s " "$method" "$endpoint"
    
    local url="$BASE_URL$endpoint"
    local response_code
    local response_time
    
    if [ "$method" = "GET" ]; then
        response_code=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout $TIMEOUT "$url" 2>/dev/null || echo "000")
        response_time=$(curl -s -o /dev/null -w "%{time_total}" --connect-timeout $TIMEOUT "$url" 2>/dev/null || echo "0")
    elif [ "$method" = "POST" ]; then
        if [ -n "$data" ]; then
            response_code=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout $TIMEOUT -X POST -H "Content-Type: application/json" -d "$data" "$url" 2>/dev/null || echo "000")
            response_time=$(curl -s -o /dev/null -w "%{time_total}" --connect-timeout $TIMEOUT -X POST -H "Content-Type: application/json" -d "$data" "$url" 2>/dev/null || echo "0")
        else
            response_code=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout $TIMEOUT -X POST "$url" 2>/dev/null || echo "000")
            response_time=$(curl -s -o /dev/null -w "%{time_total}" --connect-timeout $TIMEOUT -X POST "$url" 2>/dev/null || echo "0")
        fi
    elif [ "$method" = "PUT" ]; then
        response_code=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout $TIMEOUT -X PUT -H "Content-Type: application/json" -d "${data:-{}}" "$url" 2>/dev/null || echo "000")
        response_time=$(curl -s -o /dev/null -w "%{time_total}" --connect-timeout $TIMEOUT -X PUT -H "Content-Type: application/json" -d "${data:-{}}" "$url" 2>/dev/null || echo "0")
    elif [ "$method" = "DELETE" ]; then
        response_code=$(curl -s -o /dev/null -w "%{http_code}" --connect-timeout $TIMEOUT -X DELETE "$url" 2>/dev/null || echo "000")
        response_time=$(curl -s -o /dev/null -w "%{time_total}" --connect-timeout $TIMEOUT -X DELETE "$url" 2>/dev/null || echo "0")
    fi
    
    # Format response time
    response_time_ms=$(echo "$response_time * 1000" | bc -l 2>/dev/null | cut -d. -f1 2>/dev/null || echo "0")
    
    # Determine status
    case $response_code in
        200|201|202)
            echo -e "${GREEN}✅ PASS${NC} ($response_code) ${response_time_ms}ms"
            PASSED_TESTS=$((PASSED_TESTS + 1))
            ;;
        501)
            echo -e "${YELLOW}⚠️  NOT IMPL${NC} ($response_code) ${response_time_ms}ms"
            NOT_IMPLEMENTED=$((NOT_IMPLEMENTED + 1))
            ;;
        404)
            echo -e "${RED}❌ NOT FOUND${NC} ($response_code) ${response_time_ms}ms"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            ;;
        000)
            echo -e "${RED}❌ NO RESPONSE${NC} (timeout/error)"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            ;;
        *)
            echo -e "${YELLOW}⚠️  UNKNOWN${NC} ($response_code) ${response_time_ms}ms"
            FAILED_TESTS=$((FAILED_TESTS + 1))
            ;;
    esac
}

# Check if backend is running
echo -e "${CYAN}🔍 Checking backend service...${NC}"
if ! curl -s --connect-timeout 5 "$BASE_URL/api/system/status" > /dev/null 2>&1; then
    echo -e "${RED}❌ Backend service is not accessible at $BASE_URL${NC}"
    echo -e "${YELLOW}Please ensure the AISecurityVision service is running${NC}"
    exit 1
fi
echo -e "${GREEN}✅ Backend service is running${NC}"
echo ""

echo -e "${CYAN}🧪 Testing API endpoints...${NC}"
echo ""

# System Management
echo -e "${BOLD}System Management:${NC}"
test_endpoint "GET" "/api/system/status" "System status"
test_endpoint "GET" "/api/system/info" "System information"
test_endpoint "GET" "/api/system/config" "System configuration"
test_endpoint "GET" "/api/system/metrics" "System metrics"
test_endpoint "GET" "/api/system/stats" "System statistics"
test_endpoint "GET" "/api/system/pipeline-stats" "Pipeline statistics"
echo ""

# Camera Management
echo -e "${BOLD}Camera Management:${NC}"
test_endpoint "GET" "/api/cameras" "List cameras"
test_endpoint "POST" "/api/cameras" "Add camera" '{"id":"test_api","name":"Test Camera","rtsp_url":"rtsp://test:test@192.168.1.100:554/stream","enabled":true}'
test_endpoint "GET" "/api/cameras/camera_ch2" "Get specific camera"
test_endpoint "PUT" "/api/cameras/camera_ch2" "Update camera" '{"name":"Updated Camera"}'
test_endpoint "DELETE" "/api/cameras/test_api" "Delete camera"
test_endpoint "POST" "/api/cameras/test-connection" "Test connection" '{"url":"rtsp://test:test@192.168.1.100:554/stream"}'
echo ""

# Person Statistics
echo -e "${BOLD}Person Statistics:${NC}"
test_endpoint "GET" "/api/cameras/camera_ch2/person-stats" "Get person stats"
test_endpoint "POST" "/api/cameras/camera_ch2/person-stats/enable" "Enable person stats"
test_endpoint "POST" "/api/cameras/camera_ch2/person-stats/disable" "Disable person stats"
test_endpoint "GET" "/api/cameras/camera_ch2/person-stats/config" "Get person stats config"
test_endpoint "POST" "/api/cameras/camera_ch2/person-stats/config" "Update person stats config" '{"enabled":true}'
echo ""

# AI Detection
echo -e "${BOLD}AI Detection:${NC}"
test_endpoint "GET" "/api/detection/categories" "Get detection categories"
test_endpoint "POST" "/api/detection/categories" "Update detection categories" '{"enabled_categories":["person","car"]}'
test_endpoint "GET" "/api/detection/categories/available" "Get available categories"
test_endpoint "GET" "/api/detection/config" "Get detection config"
test_endpoint "PUT" "/api/detection/config" "Update detection config" '{"confidence_threshold":0.5}'
test_endpoint "GET" "/api/detection/stats" "Get detection stats"
echo ""

# Network Management
echo -e "${BOLD}Network Management:${NC}"
test_endpoint "GET" "/api/network/interfaces" "Get network interfaces"
test_endpoint "GET" "/api/network/stats" "Get network stats"
test_endpoint "POST" "/api/network/test" "Test network" '{"host":"8.8.8.8","timeout":5}'
echo ""

# ONVIF Discovery
echo -e "${BOLD}ONVIF Discovery:${NC}"
test_endpoint "GET" "/api/source/discover" "Discover devices"
test_endpoint "POST" "/api/source/add-discovered" "Add discovered device" '{"ip":"192.168.1.100","port":80}'
echo ""

# Legacy Endpoints
echo -e "${BOLD}Legacy Endpoints:${NC}"
test_endpoint "GET" "/api/source/list" "List sources (legacy)"
test_endpoint "POST" "/api/source/add" "Add source (legacy)" '{"url":"rtsp://test:test@192.168.1.100:554/stream"}'
echo ""

# Placeholder Endpoints
echo -e "${BOLD}Placeholder Endpoints:${NC}"
test_endpoint "GET" "/api/recordings" "Get recordings"
test_endpoint "GET" "/api/logs" "Get logs"
test_endpoint "GET" "/api/statistics" "Get statistics"
test_endpoint "POST" "/api/auth/login" "User login" '{"username":"admin","password":"admin"}'
test_endpoint "POST" "/api/auth/logout" "User logout"
test_endpoint "GET" "/api/auth/user" "Get current user"
echo ""

# Summary
echo -e "${BOLD}${CYAN}📊 Test Summary:${NC}"
echo -e "${CYAN}===============${NC}"
echo -e "Total Tests: ${BOLD}$TOTAL_TESTS${NC}"
echo -e "✅ Passed: ${GREEN}$PASSED_TESTS${NC} ($(( PASSED_TESTS * 100 / TOTAL_TESTS ))%)"
echo -e "❌ Failed: ${RED}$FAILED_TESTS${NC} ($(( FAILED_TESTS * 100 / TOTAL_TESTS ))%)"
echo -e "⚠️  Not Implemented: ${YELLOW}$NOT_IMPLEMENTED${NC} ($(( NOT_IMPLEMENTED * 100 / TOTAL_TESTS ))%)"

# Calculate implementation rate
IMPLEMENTED_RATE=$(( (PASSED_TESTS * 100) / TOTAL_TESTS ))
echo ""
if [ $IMPLEMENTED_RATE -ge 80 ]; then
    echo -e "${GREEN}🎉 Excellent! ${IMPLEMENTED_RATE}% of endpoints are implemented${NC}"
elif [ $IMPLEMENTED_RATE -ge 60 ]; then
    echo -e "${YELLOW}👍 Good! ${IMPLEMENTED_RATE}% of endpoints are implemented${NC}"
else
    echo -e "${RED}⚠️  Needs improvement: Only ${IMPLEMENTED_RATE}% of endpoints are implemented${NC}"
fi

echo ""
echo -e "${BOLD}${GREEN}✅ Quick API test completed!${NC}"
