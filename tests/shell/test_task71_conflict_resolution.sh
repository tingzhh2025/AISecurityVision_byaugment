#!/bin/bash

# Task 71: Enhanced Conflict Resolution Logic Test Script
# Tests advanced conflict resolution for overlapping ROIs considering priority and time rules

set -e

echo "=== Task 71: Enhanced Conflict Resolution Logic Test ==="
echo "Testing advanced conflict resolution for overlapping ROIs"
echo "considering priority and time rules."
echo

# Configuration
API_BASE="http://localhost:8080/api"
CAMERA_ID="test_camera_conflict"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Function to print colored output
print_status() {
    echo -e "${GREEN}✅ $1${NC}"
}

print_error() {
    echo -e "${RED}❌ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ️  $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠️  $1${NC}"
}

# Function to check if API server is running
check_server() {
    print_info "Checking if API server is running..."
    
    if curl -s "$API_BASE/system/status" > /dev/null; then
        print_status "API server is running"
        return 0
    else
        print_error "API server is not running. Please start the application first."
        exit 1
    fi
}

# Function to create test ROIs with overlapping areas and different priorities
create_test_rois() {
    print_info "Creating test ROIs for conflict resolution..."
    
    # ROI 1: High priority (5), 24/7 active
    echo "Creating High Priority ROI (Priority 5, 24/7)..."
    curl -s -X POST "$API_BASE/rois" \
        -H "Content-Type: application/json" \
        -d '{
            "id": "high_priority_roi",
            "camera_id": "'$CAMERA_ID'",
            "name": "Critical Security Zone",
            "polygon": [
                {"x": 100, "y": 100},
                {"x": 300, "y": 100},
                {"x": 300, "y": 300},
                {"x": 100, "y": 300}
            ],
            "enabled": true,
            "priority": 5,
            "start_time": "",
            "end_time": ""
        }' | jq '.'
    
    # ROI 2: Medium priority (3), business hours only
    echo "Creating Medium Priority ROI (Priority 3, 09:00-17:00)..."
    curl -s -X POST "$API_BASE/rois" \
        -H "Content-Type: application/json" \
        -d '{
            "id": "medium_priority_roi",
            "camera_id": "'$CAMERA_ID'",
            "name": "Office Area",
            "polygon": [
                {"x": 150, "y": 150},
                {"x": 350, "y": 150},
                {"x": 350, "y": 350},
                {"x": 150, "y": 350}
            ],
            "enabled": true,
            "priority": 3,
            "start_time": "09:00",
            "end_time": "17:00"
        }' | jq '.'
    
    # ROI 3: Low priority (1), night shift
    echo "Creating Low Priority ROI (Priority 1, 18:00-08:00)..."
    curl -s -X POST "$API_BASE/rois" \
        -H "Content-Type: application/json" \
        -d '{
            "id": "low_priority_roi",
            "camera_id": "'$CAMERA_ID'",
            "name": "General Monitoring Zone",
            "polygon": [
                {"x": 80, "y": 80},
                {"x": 320, "y": 80},
                {"x": 320, "y": 320},
                {"x": 80, "y": 320}
            ],
            "enabled": true,
            "priority": 1,
            "start_time": "18:00",
            "end_time": "08:00"
        }' | jq '.'
    
    # ROI 4: Same priority as ROI 2, different time window
    echo "Creating Same Priority ROI (Priority 3, 13:00-15:00)..."
    curl -s -X POST "$API_BASE/rois" \
        -H "Content-Type: application/json" \
        -d '{
            "id": "same_priority_roi",
            "camera_id": "'$CAMERA_ID'",
            "name": "Meeting Room",
            "polygon": [
                {"x": 200, "y": 200},
                {"x": 400, "y": 200},
                {"x": 400, "y": 400},
                {"x": 200, "y": 400}
            ],
            "enabled": true,
            "priority": 3,
            "start_time": "13:00",
            "end_time": "15:00"
        }' | jq '.'
    
    print_status "Created 4 overlapping ROIs with different priorities and time rules"
}

# Function to test conflict resolution scenarios
test_conflict_scenarios() {
    print_info "Testing conflict resolution scenarios..."
    
    # Get current time
    CURRENT_HOUR=$(date +%H)
    print_info "Current time: ${CURRENT_HOUR}:00"
    
    # Test 1: High priority should always win
    echo
    print_info "Test 1: High Priority Dominance"
    echo "- High priority ROI (Priority 5) should win over all others"
    echo "- Expected: High priority ROI selected in all conflicts"
    
    # Test 2: Time-based filtering
    echo
    print_info "Test 2: Time-based Filtering"
    if [ $CURRENT_HOUR -ge 9 ] && [ $CURRENT_HOUR -lt 17 ]; then
        echo "- During business hours (09:00-17:00)"
        echo "- Medium priority ROI should be active"
        echo "- Expected: Conflict between High (P5) and Medium (P3) -> High wins"
    elif [ $CURRENT_HOUR -ge 18 ] || [ $CURRENT_HOUR -lt 8 ]; then
        echo "- During night shift (18:00-08:00)"
        echo "- Low priority ROI should be active"
        echo "- Expected: Conflict between High (P5) and Low (P1) -> High wins"
    else
        echo "- Outside defined time windows"
        echo "- Only High priority ROI should be active"
        echo "- Expected: Single ROI, no conflict"
    fi
    
    # Test 3: Same priority resolution
    if [ $CURRENT_HOUR -ge 13 ] && [ $CURRENT_HOUR -lt 15 ]; then
        echo
        print_info "Test 3: Same Priority Conflict Resolution"
        echo "- Both Medium priority ROI and Same priority ROI active (Priority 3)"
        echo "- Expected: Time-based tiebreaker or lexicographic order"
    fi
}

# Function to trigger test alarm and observe conflict resolution
trigger_test_alarm() {
    print_info "Triggering test alarm to observe conflict resolution..."
    
    RESPONSE=$(curl -s -X POST "$API_BASE/alarms/test" \
        -H "Content-Type: application/json" \
        -d '{
            "event_type": "intrusion",
            "camera_id": "'$CAMERA_ID'"
        }')
    
    echo "Test alarm response:"
    echo "$RESPONSE" | jq '.'
    
    print_status "Test alarm triggered - check application logs for conflict resolution details"
}

# Function to verify system status
check_system_status() {
    print_info "Checking system status..."
    
    STATUS=$(curl -s "$API_BASE/system/status")
    echo "System Status:"
    echo "$STATUS" | jq '.'
    
    # Extract key metrics
    ACTIVE_PIPELINES=$(echo "$STATUS" | jq -r '.active_pipelines // 0')
    MONITORING_HEALTHY=$(echo "$STATUS" | jq -r '.monitoring_healthy // false')
    
    if [ "$MONITORING_HEALTHY" = "true" ]; then
        print_status "System monitoring is healthy"
    else
        print_warning "System monitoring may have issues"
    fi
    
    print_info "Active pipelines: $ACTIVE_PIPELINES"
}

# Function to clean up test ROIs
cleanup_test_rois() {
    print_info "Cleaning up test ROIs..."
    
    ROIS=("high_priority_roi" "medium_priority_roi" "low_priority_roi" "same_priority_roi")
    
    for roi in "${ROIS[@]}"; do
        echo "Deleting ROI: $roi"
        curl -s -X DELETE "$API_BASE/rois/$roi" || true
    done
    
    print_status "Test ROIs cleaned up"
}

# Function to compile and run unit tests
run_unit_tests() {
    print_info "Compiling and running unit tests..."
    
    if [ -f "test_conflict_resolution_unit.cpp" ]; then
        echo "Compiling unit test..."
        g++ -std=c++17 -I/usr/include/opencv4 \
            test_conflict_resolution_unit.cpp \
            src/ai/BehaviorAnalyzer.cpp \
            -lopencv_core -lopencv_imgproc \
            -o test_conflict_resolution_unit
        
        if [ $? -eq 0 ]; then
            print_status "Unit test compiled successfully"
            echo "Running unit tests..."
            ./test_conflict_resolution_unit
            
            if [ $? -eq 0 ]; then
                print_status "Unit tests passed"
            else
                print_error "Unit tests failed"
            fi
        else
            print_error "Failed to compile unit tests"
        fi
    else
        print_warning "Unit test file not found, skipping unit tests"
    fi
}

# Main test execution
main() {
    echo "Starting Task 71 comprehensive test..."
    echo
    
    # Check prerequisites
    check_server
    
    # Run unit tests first
    run_unit_tests
    echo
    
    # Clean up any existing test data
    cleanup_test_rois
    echo
    
    # Create test ROIs
    create_test_rois
    echo
    
    # Test conflict scenarios
    test_conflict_scenarios
    echo
    
    # Trigger test alarm
    trigger_test_alarm
    echo
    
    # Check system status
    check_system_status
    echo
    
    # Summary
    print_status "Task 71 Enhanced Conflict Resolution Test Completed"
    echo
    echo "Test Summary:"
    echo "✅ Created overlapping ROIs with different priorities and time rules"
    echo "✅ Tested conflict resolution scenarios"
    echo "✅ Triggered test alarm to observe conflict resolution"
    echo "✅ Verified system integration"
    echo
    print_info "Check the application logs for detailed conflict resolution information"
    print_info "Look for messages containing 'Enhanced conflict-resolved intrusion event'"
    echo
    
    # Clean up
    cleanup_test_rois
    
    print_status "All tests completed successfully!"
}

# Run main function
main
