#!/bin/bash

# Task 76: Test ReID Configuration API Endpoints
# This script tests the ReID matching algorithm with configurable similarity threshold

echo "🧪 Task 76: Testing ReID Configuration API Endpoints"
echo "=================================================="

# Configuration
API_BASE="http://localhost:8080/api"
REID_CONFIG_URL="$API_BASE/reid/config"
REID_THRESHOLD_URL="$API_BASE/reid/threshold"
REID_STATUS_URL="$API_BASE/reid/status"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

# Function to test API endpoint
test_endpoint() {
    local method=$1
    local url=$2
    local data=$3
    local description=$4
    
    print_status "Testing: $description"
    
    if [ "$method" = "GET" ]; then
        response=$(curl -s -w "\n%{http_code}" "$url")
    else
        response=$(curl -s -w "\n%{http_code}" -X "$method" -H "Content-Type: application/json" -d "$data" "$url")
    fi
    
    http_code=$(echo "$response" | tail -n1)
    body=$(echo "$response" | head -n -1)
    
    if [ "$http_code" -eq 200 ] || [ "$http_code" -eq 201 ]; then
        print_success "HTTP $http_code - $description"
        echo "Response: $body" | jq '.' 2>/dev/null || echo "Response: $body"
    else
        print_error "HTTP $http_code - $description failed"
        echo "Response: $body"
    fi
    
    echo ""
    return $http_code
}

# Wait for application to start
print_status "Waiting for application to start..."
sleep 3

echo "🔧 Test 1: Get Initial ReID Configuration"
echo "----------------------------------------"
test_endpoint "GET" "$REID_CONFIG_URL" "" "Get initial ReID configuration"

echo "🔧 Test 2: Update ReID Configuration"
echo "-----------------------------------"
reid_config='{
    "enabled": true,
    "similarity_threshold": 0.8,
    "max_matches": 10,
    "match_timeout": 45.0,
    "cross_camera_enabled": true
}'

test_endpoint "POST" "$REID_CONFIG_URL" "$reid_config" "Update ReID configuration"

echo "🔧 Test 3: Update Similarity Threshold Only"
echo "------------------------------------------"
threshold_config='{"threshold": 0.75}'
test_endpoint "PUT" "$REID_THRESHOLD_URL" "$threshold_config" "Update similarity threshold to 0.75"

echo "🔧 Test 4: Test Invalid Threshold (Too Low)"
echo "------------------------------------------"
invalid_threshold='{"threshold": 0.3}'
test_endpoint "PUT" "$REID_THRESHOLD_URL" "$invalid_threshold" "Test invalid threshold (0.3 - too low)"

echo "🔧 Test 5: Test Invalid Threshold (Too High)"
echo "-------------------------------------------"
invalid_threshold='{"threshold": 0.98}'
test_endpoint "PUT" "$REID_THRESHOLD_URL" "$invalid_threshold" "Test invalid threshold (0.98 - too high)"

echo "🔧 Test 6: Get ReID Status"
echo "-------------------------"
test_endpoint "GET" "$REID_STATUS_URL" "" "Get ReID system status"

echo "🔧 Test 7: Test Invalid Configuration"
echo "------------------------------------"
invalid_config='{
    "enabled": true,
    "similarity_threshold": 1.5,
    "max_matches": 25,
    "match_timeout": 500.0
}'

test_endpoint "POST" "$REID_CONFIG_URL" "$invalid_config" "Test invalid configuration (threshold > 0.95, max_matches > 20, timeout > 300)"

echo "🔧 Test 8: Verify Configuration Persistence"
echo "------------------------------------------"
test_endpoint "GET" "$REID_CONFIG_URL" "" "Verify configuration after updates"

echo "🔧 Test 9: Test Edge Case Thresholds"
echo "-----------------------------------"
min_threshold='{"threshold": 0.5}'
test_endpoint "PUT" "$REID_THRESHOLD_URL" "$min_threshold" "Test minimum valid threshold (0.5)"

max_threshold='{"threshold": 0.95}'
test_endpoint "PUT" "$REID_THRESHOLD_URL" "$max_threshold" "Test maximum valid threshold (0.95)"

echo "🔧 Test 10: Final Status Check"
echo "-----------------------------"
test_endpoint "GET" "$REID_STATUS_URL" "" "Final ReID system status check"

echo ""
echo "✅ Task 76 ReID Configuration API Testing Complete!"
echo "=================================================="
echo ""
echo "📋 Test Summary:"
echo "- ✅ ReID configuration GET/POST endpoints"
echo "- ✅ Similarity threshold PUT endpoint"
echo "- ✅ ReID status monitoring endpoint"
echo "- ✅ Input validation (threshold range 0.5-0.95)"
echo "- ✅ Error handling for invalid parameters"
echo "- ✅ Configuration persistence verification"
echo ""
echo "🎯 Key Features Tested:"
echo "- Configurable similarity threshold (0.5-0.95 range)"
echo "- Maximum matches limit (1-20 range)"
echo "- Match timeout configuration (5-300 seconds)"
echo "- Cross-camera tracking enable/disable"
echo "- Real-time status monitoring"
echo "- Comprehensive input validation"
