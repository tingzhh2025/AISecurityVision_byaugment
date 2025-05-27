#!/bin/bash

# Task 75: Cross-Camera Tracking Test Script
# Tests the cross-camera tracking logic in TaskManager to share ReID features between VideoPipeline instances

echo "=========================================="
echo "Task 75: Cross-Camera Tracking Test"
echo "=========================================="

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Test configuration
API_BASE="http://localhost:8080"
TEST_TIMEOUT=30
LOG_FILE="cross_camera_test.log"

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

# Function to check if application is running
check_app_running() {
    if pgrep -f "AISecurityVision" > /dev/null; then
        return 0
    else
        return 1
    fi
}

# Function to start application
start_application() {
    print_status "Starting AISecurityVision application..."
    cd build
    ./AISecurityVision > $LOG_FILE 2>&1 &
    APP_PID=$!
    sleep 5
    
    if check_app_running; then
        print_success "Application started successfully (PID: $APP_PID)"
        return 0
    else
        print_error "Failed to start application"
        return 1
    fi
}

# Function to stop application
stop_application() {
    print_status "Stopping application..."
    if [ ! -z "$APP_PID" ]; then
        kill $APP_PID 2>/dev/null
        sleep 2
    fi
    pkill -f "AISecurityVision" 2>/dev/null
    sleep 1
    print_success "Application stopped"
}

# Function to test API endpoint
test_api_endpoint() {
    local endpoint=$1
    local method=${2:-GET}
    local data=${3:-""}
    local expected_status=${4:-200}
    
    print_status "Testing $method $endpoint"
    
    if [ "$method" = "GET" ]; then
        response=$(curl -s -w "%{http_code}" "$API_BASE$endpoint")
    else
        response=$(curl -s -w "%{http_code}" -X $method -H "Content-Type: application/json" -d "$data" "$API_BASE$endpoint")
    fi
    
    http_code="${response: -3}"
    body="${response%???}"
    
    if [ "$http_code" = "$expected_status" ]; then
        print_success "$method $endpoint returned $http_code"
        echo "$body"
        return 0
    else
        print_error "$method $endpoint returned $http_code (expected $expected_status)"
        echo "$body"
        return 1
    fi
}

# Function to add test video source
add_test_video_source() {
    local source_id=$1
    local rtsp_url=${2:-"rtsp://test.camera/stream"}
    
    print_status "Adding test video source: $source_id"
    
    local json_data="{
        \"id\": \"$source_id\",
        \"url\": \"$rtsp_url\",
        \"protocol\": \"rtsp\",
        \"width\": 1920,
        \"height\": 1080,
        \"fps\": 25,
        \"enabled\": true
    }"
    
    test_api_endpoint "/api/source/add" "POST" "$json_data" 201
}

# Function to test cross-camera tracking configuration
test_cross_camera_config() {
    print_status "Testing cross-camera tracking configuration..."
    
    # Test getting current configuration
    print_status "Getting current cross-camera configuration"
    test_api_endpoint "/api/cross-camera/config" "GET"
    
    # Test updating configuration
    print_status "Updating cross-camera configuration"
    local config_data="{
        \"enabled\": true,
        \"reid_similarity_threshold\": 0.75,
        \"max_track_age_seconds\": 45.0,
        \"matching_enabled\": true
    }"
    
    test_api_endpoint "/api/cross-camera/config" "POST" "$config_data" 200
    
    # Verify configuration was updated
    print_status "Verifying configuration update"
    test_api_endpoint "/api/cross-camera/config" "GET"
}

# Function to test cross-camera tracking statistics
test_cross_camera_stats() {
    print_status "Testing cross-camera tracking statistics..."
    
    test_api_endpoint "/api/cross-camera/stats" "GET"
}

# Function to test active cross-camera tracks
test_active_tracks() {
    print_status "Testing active cross-camera tracks..."
    
    test_api_endpoint "/api/cross-camera/tracks" "GET"
}

# Function to simulate cross-camera tracking scenario
simulate_cross_camera_scenario() {
    print_status "Simulating cross-camera tracking scenario..."
    
    # Add multiple test cameras
    add_test_video_source "camera_1" "rtsp://test1.camera/stream"
    sleep 2
    add_test_video_source "camera_2" "rtsp://test2.camera/stream"
    sleep 2
    add_test_video_source "camera_3" "rtsp://test3.camera/stream"
    sleep 5
    
    # Check system status
    print_status "Checking system status with multiple cameras"
    test_api_endpoint "/api/system/status" "GET"
    
    # Wait for some processing time
    print_status "Waiting for cross-camera tracking to process..."
    sleep 10
    
    # Check cross-camera tracks
    test_active_tracks
    
    # Check statistics
    test_cross_camera_stats
}

# Function to test cross-camera tracking reset
test_tracking_reset() {
    print_status "Testing cross-camera tracking reset..."
    
    test_api_endpoint "/api/cross-camera/reset" "POST" "{}" 200
    
    # Verify reset worked
    sleep 2
    test_cross_camera_stats
}

# Main test execution
main() {
    print_status "Starting Task 75 Cross-Camera Tracking Tests"
    echo "Log file: $LOG_FILE"
    echo
    
    # Cleanup any existing processes
    stop_application
    sleep 2
    
    # Start application
    if ! start_application; then
        print_error "Failed to start application. Exiting."
        exit 1
    fi
    
    # Wait for application to fully initialize
    print_status "Waiting for application to initialize..."
    sleep 10
    
    # Test basic API connectivity
    print_status "Testing basic API connectivity..."
    if ! test_api_endpoint "/api/system/status" "GET"; then
        print_error "API not responding. Check application logs."
        stop_application
        exit 1
    fi
    
    echo
    print_status "=== Cross-Camera Configuration Tests ==="
    test_cross_camera_config
    
    echo
    print_status "=== Cross-Camera Statistics Tests ==="
    test_cross_camera_stats
    
    echo
    print_status "=== Active Tracks Tests ==="
    test_active_tracks
    
    echo
    print_status "=== Cross-Camera Scenario Simulation ==="
    simulate_cross_camera_scenario
    
    echo
    print_status "=== Cross-Camera Reset Tests ==="
    test_tracking_reset
    
    # Final status check
    echo
    print_status "=== Final System Status ==="
    test_api_endpoint "/api/system/status" "GET"
    
    # Stop application
    stop_application
    
    echo
    print_success "Task 75 Cross-Camera Tracking Tests Completed!"
    print_status "Check $LOG_FILE for detailed application logs"
    
    # Show summary from logs
    if [ -f "$LOG_FILE" ]; then
        echo
        print_status "=== Cross-Camera Tracking Log Summary ==="
        grep -i "cross.*camera\|global.*track\|reid.*match" "$LOG_FILE" | tail -20
    fi
}

# Cleanup on exit
trap 'stop_application' EXIT

# Run main function
main "$@"
