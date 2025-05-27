#!/bin/bash

# Test script for enhanced pipeline statistics API endpoints
# This script tests the new API endpoints for detailed pipeline and system statistics

API_BASE="http://localhost:8080"
CURL_OPTS="-s -w \n%{http_code}\n"

echo "=== Enhanced Pipeline Statistics API Test ==="
echo "Testing enhanced API endpoints for Task 44 implementation"
echo

# Function to make API call and display results
test_endpoint() {
    local endpoint=$1
    local description=$2
    local method=${3:-GET}
    local data=${4:-}
    
    echo "Testing: $description"
    echo "Endpoint: $method $endpoint"
    echo "----------------------------------------"
    
    if [ "$method" = "GET" ]; then
        response=$(curl $CURL_OPTS "$API_BASE$endpoint")
    else
        response=$(curl $CURL_OPTS -X "$method" -H "Content-Type: application/json" -d "$data" "$API_BASE$endpoint")
    fi
    
    # Extract HTTP status code (last line)
    status_code=$(echo "$response" | tail -n1)
    # Extract response body (all but last line)
    body=$(echo "$response" | head -n -1)
    
    echo "Status Code: $status_code"
    echo "Response:"
    echo "$body" | python3 -m json.tool 2>/dev/null || echo "$body"
    echo
    echo "========================================"
    echo
}

# Test 1: Basic system status (existing endpoint)
test_endpoint "/api/system/status" "Basic System Status"

# Test 2: Enhanced system metrics (existing endpoint)
test_endpoint "/api/system/metrics" "Enhanced System Metrics"

# Test 3: NEW - Pipeline statistics endpoint
test_endpoint "/api/system/pipeline-stats" "Individual Pipeline Statistics"

# Test 4: NEW - System statistics endpoint
test_endpoint "/api/system/stats" "Comprehensive System Statistics"

# Test 5: Add a test video source first
echo "Adding test video source for statistics testing..."
test_data='{
    "id": "test_stats_camera",
    "protocol": "rtsp",
    "url": "rtsp://test.example.com/stream",
    "width": 1920,
    "height": 1080,
    "fps": 30,
    "enabled": true
}'

test_endpoint "/api/source/add" "Add Test Video Source" "POST" "$test_data"

# Wait a moment for the pipeline to initialize
echo "Waiting 3 seconds for pipeline initialization..."
sleep 3

# Test 6: Get pipeline stats after adding source
test_endpoint "/api/system/pipeline-stats" "Pipeline Stats After Adding Source"

# Test 7: Get system stats after adding source
test_endpoint "/api/system/stats" "System Stats After Adding Source"

# Test 8: Get video sources list
test_endpoint "/api/source/list" "List Video Sources"

# Test 9: Monitor statistics over time
echo "=== Monitoring Statistics Over Time ==="
for i in {1..3}; do
    echo "Measurement $i:"
    echo "Time: $(date)"
    
    # Get current system stats
    response=$(curl -s "$API_BASE/api/system/stats")
    echo "System Stats:"
    echo "$response" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    system = data.get('system', {})
    resources = data.get('resources', {})
    performance = data.get('performance', {})
    
    print(f\"  Total Pipelines: {system.get('total_pipelines', 0)}\")
    print(f\"  Running Pipelines: {system.get('running_pipelines', 0)}\")
    print(f\"  Healthy Pipelines: {system.get('healthy_pipelines', 0)}\")
    print(f\"  Total Frame Rate: {system.get('total_frame_rate', 0):.2f} fps\")
    print(f\"  CPU Usage: {resources.get('cpu_usage', 0):.1f}%\")
    print(f\"  GPU Utilization: {resources.get('gpu_utilization', 0):.1f}%\")
    print(f\"  Average Frame Rate: {performance.get('avg_frame_rate', 0):.2f} fps\")
    print(f\"  Drop Rate: {performance.get('drop_rate', 0):.2f}%\")
    print(f\"  Health Ratio: {performance.get('health_ratio', 0):.1f}%\")
except:
    print('  Error parsing JSON response')
" 2>/dev/null || echo "  Could not parse response"
    
    echo
    
    if [ $i -lt 3 ]; then
        echo "Waiting 2 seconds..."
        sleep 2
    fi
done

# Test 10: Remove test video source
echo "Removing test video source..."
test_endpoint "/api/source/remove/test_stats_camera" "Remove Test Video Source" "DELETE"

# Test 11: Final statistics check
test_endpoint "/api/system/stats" "Final System Stats After Cleanup"

echo "=== Test Summary ==="
echo "✅ Tested basic system status endpoint"
echo "✅ Tested enhanced system metrics endpoint"
echo "✅ Tested new pipeline statistics endpoint"
echo "✅ Tested new comprehensive system statistics endpoint"
echo "✅ Tested statistics with active pipeline"
echo "✅ Tested statistics monitoring over time"
echo "✅ Tested cleanup and final statistics"
echo
echo "Enhanced Pipeline Statistics API test completed!"
echo "Task 44 implementation verification finished."
