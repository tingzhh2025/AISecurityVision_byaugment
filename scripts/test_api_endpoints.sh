#!/bin/bash

# API端点完整性测试脚本
# 用于验证前后端API的同步性

BASE_URL="http://localhost:8080/api"
CAMERA_ID="camera_01"

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 测试函数
test_endpoint() {
    local method=$1
    local endpoint=$2
    local data=$3
    local description=$4
    
    echo -n "Testing: $description... "
    
    if [ "$method" == "GET" ]; then
        response=$(curl -s -o /dev/null -w "%{http_code}" -X GET "$BASE_URL$endpoint")
    elif [ "$method" == "POST" ]; then
        response=$(curl -s -o /dev/null -w "%{http_code}" -X POST "$BASE_URL$endpoint" \
            -H "Content-Type: application/json" \
            -d "$data")
    elif [ "$method" == "PUT" ]; then
        response=$(curl -s -o /dev/null -w "%{http_code}" -X PUT "$BASE_URL$endpoint" \
            -H "Content-Type: application/json" \
            -d "$data")
    elif [ "$method" == "DELETE" ]; then
        response=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE "$BASE_URL$endpoint")
    fi
    
    if [ "$response" == "200" ] || [ "$response" == "201" ] || [ "$response" == "204" ]; then
        echo -e "${GREEN}✓ OK${NC} (HTTP $response)"
        return 0
    elif [ "$response" == "404" ]; then
        echo -e "${RED}✗ NOT FOUND${NC} (HTTP $response)"
        return 1
    elif [ "$response" == "501" ]; then
        echo -e "${YELLOW}⚠ NOT IMPLEMENTED${NC} (HTTP $response)"
        return 2
    elif [ "$response" == "000" ]; then
        echo -e "${RED}✗ CONNECTION FAILED${NC}"
        return 1
    else
        echo -e "${YELLOW}⚠ UNEXPECTED${NC} (HTTP $response)"
        return 1
    fi
}

echo "=== AI Security Vision API Endpoint Test ==="
echo "Base URL: $BASE_URL"
echo ""

# 统计变量
total_tests=0
passed_tests=0
failed_tests=0
not_implemented=0

# 系统端点测试
echo "--- System Endpoints ---"
test_endpoint "GET" "/system/status" "" "System Status"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/system/info" "" "System Info"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/system/config" "" "System Config"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/system/metrics" "" "System Metrics"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

echo ""

# 摄像头管理端点测试
echo "--- Camera Management Endpoints ---"
test_endpoint "GET" "/cameras" "" "List Cameras"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/cameras/$CAMERA_ID" "" "Get Camera Details"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "POST" "/cameras/test-connection" '{"url":"rtsp://test"}' "Test Camera Connection"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

echo ""

# 人员统计端点测试
echo "--- Person Statistics Endpoints ---"
test_endpoint "GET" "/cameras/$CAMERA_ID/person-stats" "" "Get Person Stats"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "POST" "/cameras/$CAMERA_ID/person-stats/enable" "" "Enable Person Stats"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "POST" "/cameras/$CAMERA_ID/person-stats/disable" "" "Disable Person Stats"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/cameras/$CAMERA_ID/person-stats/config" "" "Get Person Stats Config"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

echo ""

# 检测配置端点测试
echo "--- Detection Configuration Endpoints ---"
test_endpoint "GET" "/detection/categories" "" "Get Detection Categories"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/detection/categories/available" "" "Get Available Categories"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/detection/config" "" "Get Detection Config"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/detection/stats" "" "Get Detection Stats"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

echo ""

# 网络管理端点测试
echo "--- Network Management Endpoints ---"
test_endpoint "GET" "/network/interfaces" "" "List Network Interfaces"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/network/stats" "" "Get Network Stats"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

echo ""

# 警报管理端点测试
echo "--- Alert Management Endpoints ---"
test_endpoint "GET" "/alerts" "" "List Alerts"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/alarms/config" "" "Get Alarm Config"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/alarms/status" "" "Get Alarm Status"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

echo ""

# 其他端点测试（这些可能还未实现）
echo "--- Other Endpoints (May Not Be Implemented) ---"
test_endpoint "GET" "/recordings" "" "List Recordings"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/logs" "" "Get Logs"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

test_endpoint "GET" "/statistics" "" "Get Statistics"
((total_tests++)); result=$?; [ $result -eq 0 ] && ((passed_tests++)) || ([ $result -eq 2 ] && ((not_implemented++)) || ((failed_tests++)))

echo ""
echo "=== Test Summary ==="
echo "Total Tests: $total_tests"
echo -e "Passed: ${GREEN}$passed_tests${NC}"
echo -e "Failed: ${RED}$failed_tests${NC}"
echo -e "Not Implemented: ${YELLOW}$not_implemented${NC}"

if [ $failed_tests -eq 0 ]; then
    echo -e "\n${GREEN}All implemented endpoints are working!${NC}"
    exit 0
else
    echo -e "\n${RED}Some endpoints failed. Please check the API implementation.${NC}"
    exit 1
fi
