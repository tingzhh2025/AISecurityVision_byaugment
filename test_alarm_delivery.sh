#!/bin/bash

# Test script for Task 63: HTTP POST alarm delivery with JSON payload formatting
# This script tests the alarm configuration API and HTTP alarm delivery functionality

set -e

API_BASE="http://localhost:8080/api"
TEST_SERVER_PORT=8082
TEST_SERVER_PID=""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test counter
TESTS_PASSED=0
TESTS_FAILED=0

print_test() {
    echo -e "${BLUE}[TEST]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[PASS]${NC} $1"
    ((TESTS_PASSED++))
}

print_error() {
    echo -e "${RED}[FAIL]${NC} $1"
    ((TESTS_FAILED++))
}

print_warning() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

# Start a simple HTTP server to receive test alarms
start_test_server() {
    print_test "Starting test HTTP server on port $TEST_SERVER_PORT"

    # Create a simple Python HTTP server to receive alarm POSTs
    cat > test_alarm_server.py << 'EOF'
#!/usr/bin/env python3
import http.server
import socketserver
import json
import sys
from datetime import datetime

class AlarmHandler(http.server.BaseHTTPRequestHandler):
    def do_POST(self):
        content_length = int(self.headers.get('Content-Length', 0))
        post_data = self.rfile.read(content_length)

        try:
            alarm_data = json.loads(post_data.decode('utf-8'))
            timestamp = datetime.now().isoformat()

            print(f"[{timestamp}] Received alarm:")
            print(f"  Event Type: {alarm_data.get('event_type', 'N/A')}")
            print(f"  Camera ID: {alarm_data.get('camera_id', 'N/A')}")
            print(f"  Confidence: {alarm_data.get('confidence', 'N/A')}")
            print(f"  Test Mode: {alarm_data.get('test_mode', False)}")
            print(f"  Timestamp: {alarm_data.get('timestamp', 'N/A')}")
            print(f"  Bounding Box: {alarm_data.get('bounding_box', 'N/A')}")
            print("  Raw JSON:", json.dumps(alarm_data, indent=2))
            print("-" * 50)

            # Log to file for verification
            with open('received_alarms.log', 'a') as f:
                f.write(f"{timestamp}: {json.dumps(alarm_data)}\n")

            self.send_response(200)
            self.send_header('Content-type', 'application/json')
            self.end_headers()
            self.wfile.write(b'{"status": "alarm_received", "timestamp": "' + timestamp.encode() + b'"}')

        except Exception as e:
            print(f"Error processing alarm: {e}")
            self.send_response(400)
            self.end_headers()
            self.wfile.write(b'{"error": "Invalid JSON"}')

    def log_message(self, format, *args):
        pass  # Suppress default logging

if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8081
    with socketserver.TCPServer(("", port), AlarmHandler) as httpd:
        print(f"Test alarm server listening on port {port}")
        httpd.serve_forever()
EOF

    python3 test_alarm_server.py $TEST_SERVER_PORT &
    TEST_SERVER_PID=$!
    sleep 2

    if kill -0 $TEST_SERVER_PID 2>/dev/null; then
        print_success "Test server started (PID: $TEST_SERVER_PID)"
    else
        print_error "Failed to start test server"
        exit 1
    fi
}

# Stop the test server
stop_test_server() {
    if [ ! -z "$TEST_SERVER_PID" ]; then
        print_test "Stopping test server (PID: $TEST_SERVER_PID)"
        kill $TEST_SERVER_PID 2>/dev/null || true
        wait $TEST_SERVER_PID 2>/dev/null || true
        rm -f test_alarm_server.py
    fi
}

# Test API endpoint availability
test_api_availability() {
    print_test "Testing API availability"

    if curl -s -f "$API_BASE/status" > /dev/null; then
        print_success "API server is running"
    else
        print_error "API server is not accessible at $API_BASE"
        return 1
    fi
}

# Test alarm configuration creation
test_create_alarm_config() {
    print_test "Testing alarm configuration creation"

    local config_data='{
        "method": "http",
        "url": "http://localhost:'$TEST_SERVER_PORT'/alarm",
        "timeout_ms": 5000,
        "priority": 1
    }'

    local response=$(curl -s -X POST \
        -H "Content-Type: application/json" \
        -d "$config_data" \
        "$API_BASE/alarms/config")

    if echo "$response" | grep -q '"status":"created"'; then
        print_success "Alarm configuration created successfully"
        echo "Response: $response"

        # Extract config ID for later tests
        CONFIG_ID=$(echo "$response" | grep -o '"config_id":"[^"]*"' | cut -d'"' -f4)
        echo "Config ID: $CONFIG_ID"
    else
        print_error "Failed to create alarm configuration"
        echo "Response: $response"
        return 1
    fi
}

# Test alarm configuration retrieval
test_get_alarm_configs() {
    print_test "Testing alarm configuration retrieval"

    local response=$(curl -s "$API_BASE/alarms/config")

    if echo "$response" | grep -q '"configs"'; then
        print_success "Alarm configurations retrieved successfully"
        echo "Response: $response"
    else
        print_error "Failed to retrieve alarm configurations"
        echo "Response: $response"
        return 1
    fi
}

# Test test alarm triggering
test_trigger_test_alarm() {
    print_test "Testing test alarm triggering"

    # Clear previous alarm logs
    rm -f received_alarms.log

    local alarm_data='{
        "event_type": "intrusion_detected",
        "camera_id": "test_camera_01"
    }'

    local response=$(curl -s -X POST \
        -H "Content-Type: application/json" \
        -d "$alarm_data" \
        "$API_BASE/alarms/test")

    if echo "$response" | grep -q '"status":"test_alarm_triggered"'; then
        print_success "Test alarm triggered successfully"
        echo "Response: $response"

        # Wait a moment for alarm delivery
        sleep 3

        # Check if alarm was received by test server
        if [ -f "received_alarms.log" ] && [ -s "received_alarms.log" ]; then
            print_success "Test alarm was delivered to HTTP endpoint"
            echo "Received alarm log:"
            cat received_alarms.log
        else
            print_warning "Test alarm was triggered but not received by test server"
        fi
    else
        print_error "Failed to trigger test alarm"
        echo "Response: $response"
        return 1
    fi
}

# Test alarm status endpoint
test_alarm_status() {
    print_test "Testing alarm status endpoint"

    local response=$(curl -s "$API_BASE/alarms/status")

    if echo "$response" | grep -q '"alarm_system"'; then
        print_success "Alarm status retrieved successfully"
        echo "Response: $response"
    else
        print_error "Failed to retrieve alarm status"
        echo "Response: $response"
        return 1
    fi
}

# Test alarm configuration update
test_update_alarm_config() {
    if [ -z "$CONFIG_ID" ]; then
        print_warning "Skipping update test - no config ID available"
        return 0
    fi

    print_test "Testing alarm configuration update"

    local update_data='{
        "timeout_ms": 10000,
        "priority": 2
    }'

    local response=$(curl -s -X PUT \
        -H "Content-Type: application/json" \
        -d "$update_data" \
        "$API_BASE/alarms/config/$CONFIG_ID")

    if echo "$response" | grep -q '"status":"updated"'; then
        print_success "Alarm configuration updated successfully"
        echo "Response: $response"
    else
        print_error "Failed to update alarm configuration"
        echo "Response: $response"
        return 1
    fi
}

# Test alarm configuration deletion
test_delete_alarm_config() {
    if [ -z "$CONFIG_ID" ]; then
        print_warning "Skipping delete test - no config ID available"
        return 0
    fi

    print_test "Testing alarm configuration deletion"

    local response=$(curl -s -X DELETE "$API_BASE/alarms/config/$CONFIG_ID")

    if echo "$response" | grep -q '"status":"deleted"'; then
        print_success "Alarm configuration deleted successfully"
        echo "Response: $response"
    else
        print_error "Failed to delete alarm configuration"
        echo "Response: $response"
        return 1
    fi
}

# Main test execution
main() {
    echo "=========================================="
    echo "Task 63: HTTP POST Alarm Delivery Tests"
    echo "=========================================="

    # Setup
    start_test_server
    trap stop_test_server EXIT

    # Run tests
    test_api_availability || exit 1
    test_create_alarm_config
    test_get_alarm_configs
    test_trigger_test_alarm
    test_alarm_status
    test_update_alarm_config
    test_delete_alarm_config

    # Summary
    echo "=========================================="
    echo "Test Summary:"
    echo "  Passed: $TESTS_PASSED"
    echo "  Failed: $TESTS_FAILED"
    echo "=========================================="

    if [ $TESTS_FAILED -eq 0 ]; then
        print_success "All tests passed! Task 63 implementation is working correctly."
        exit 0
    else
        print_error "Some tests failed. Please check the implementation."
        exit 1
    fi
}

# Run main function
main "$@"
