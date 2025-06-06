#!/bin/bash

# Complete test script for multi-camera setup with full system cleanup
# This script will:
# 1. Clean the system completely (stop processes, remove database)
# 2. Start the backend
# 3. Add three test cameras
# 4. Verify the setup

set -e  # Exit on any error

API_BASE="http://localhost:8080/api"
BUILD_DIR="build"
BACKEND_BINARY="AISecurityVision"

# Color output functions
print_header() {
    echo -e "\n\033[1;34m=== $1 ===\033[0m"
}

print_info() {
    echo -e "\033[1;36m[INFO]\033[0m $1"
}

print_success() {
    echo -e "\033[1;32m[SUCCESS]\033[0m $1"
}

print_warning() {
    echo -e "\033[1;33m[WARNING]\033[0m $1"
}

print_error() {
    echo -e "\033[1;31m[ERROR]\033[0m $1"
}

# Function to stop all processes
stop_processes() {
    print_header "Stopping All Processes"
    
    print_info "Stopping backend processes..."
    pkill -f "$BACKEND_BINARY" 2>/dev/null || true
    pkill -f "aibox" 2>/dev/null || true
    
    print_info "Stopping frontend processes..."
    pkill -f "vite" 2>/dev/null || true
    pkill -f "npm.*dev" 2>/dev/null || true
    
    sleep 3
    print_success "All processes stopped"
}

# Function to clean data
clean_data() {
    print_header "Cleaning System Data"
    
    # Remove database
    if [ -f "aibox.db" ]; then
        print_info "Removing database file: aibox.db"
        rm -f aibox.db
        print_success "Database removed"
    else
        print_info "Database file not found (already clean)"
    fi
    
    # Remove log files
    print_info "Removing log files..."
    rm -f *.log 2>/dev/null || true
    rm -f logs/*.log 2>/dev/null || true
    
    # Remove temporary files
    rm -f /tmp/aibox_* 2>/dev/null || true
    
    print_success "System data cleaned"
}

# Function to start backend
start_backend() {
    print_header "Starting Backend"
    
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory not found. Please build the project first."
        exit 1
    fi
    
    if [ ! -f "$BUILD_DIR/$BACKEND_BINARY" ]; then
        print_error "Backend binary not found. Please build the project first."
        exit 1
    fi
    
    cd "$BUILD_DIR"
    print_info "Starting backend in background..."
    nohup ./$BACKEND_BINARY > ../backend.log 2>&1 &
    BACKEND_PID=$!
    cd ..
    
    print_info "Backend started with PID: $BACKEND_PID"
    print_info "Waiting for backend to initialize..."
    
    # Wait for backend to be ready
    for i in {1..30}; do
        if curl -s "$API_BASE/system/status" > /dev/null 2>&1; then
            print_success "Backend is ready!"
            return 0
        fi
        echo -n "."
        sleep 1
    done
    
    print_error "Backend failed to start within 30 seconds"
    exit 1
}

# Function to add cameras
add_cameras() {
    print_header "Adding Test Cameras"
    
    # Camera 1: 192.168.1.2
    print_info "Adding Camera 1 (192.168.1.2)..."
    RESPONSE=$(curl -s -w "HTTPSTATUS:%{http_code}" -X POST "$API_BASE/cameras" \
        -H "Content-Type: application/json" \
        -d '{
            "id": "camera_192_168_1_2",
            "name": "Camera-192.168.1.2",
            "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
            "protocol": "rtsp",
            "username": "admin",
            "password": "sharpi1688",
            "width": 1920,
            "height": 1080,
            "fps": 25,
            "mjpeg_port": 8161,
            "enabled": true
        }')
    
    HTTP_STATUS=$(echo $RESPONSE | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
    if [ "$HTTP_STATUS" -eq 200 ] || [ "$HTTP_STATUS" -eq 201 ]; then
        print_success "Camera 1 added successfully"
    else
        print_warning "Camera 1 add returned status: $HTTP_STATUS"
    fi
    sleep 3
    
    # Camera 2: 192.168.1.3
    print_info "Adding Camera 2 (192.168.1.3)..."
    RESPONSE=$(curl -s -w "HTTPSTATUS:%{http_code}" -X POST "$API_BASE/cameras" \
        -H "Content-Type: application/json" \
        -d '{
            "id": "camera_192_168_1_3",
            "name": "Camera-192.168.1.3",
            "url": "rtsp://admin:sharpi1688@192.168.1.3:554/1/1",
            "protocol": "rtsp",
            "username": "admin",
            "password": "sharpi1688",
            "width": 1920,
            "height": 1080,
            "fps": 25,
            "mjpeg_port": 8162,
            "enabled": true
        }')
    
    HTTP_STATUS=$(echo $RESPONSE | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
    if [ "$HTTP_STATUS" -eq 200 ] || [ "$HTTP_STATUS" -eq 201 ]; then
        print_success "Camera 2 added successfully"
    else
        print_warning "Camera 2 add returned status: $HTTP_STATUS"
    fi
    sleep 3
    
    # Camera 3: 192.168.1.226
    print_info "Adding Camera 3 (192.168.1.226)..."
    RESPONSE=$(curl -s -w "HTTPSTATUS:%{http_code}" -X POST "$API_BASE/cameras" \
        -H "Content-Type: application/json" \
        -d '{
            "id": "camera_192_168_1_226",
            "name": "Camera-192.168.1.226",
            "url": "rtsp://192.168.1.226:8554/unicast",
            "protocol": "rtsp",
            "username": "",
            "password": "",
            "width": 1920,
            "height": 1080,
            "fps": 25,
            "mjpeg_port": 8163,
            "enabled": true
        }')
    
    HTTP_STATUS=$(echo $RESPONSE | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
    if [ "$HTTP_STATUS" -eq 200 ] || [ "$HTTP_STATUS" -eq 201 ]; then
        print_success "Camera 3 added successfully"
    else
        print_warning "Camera 3 add returned status: $HTTP_STATUS"
    fi
    sleep 3
}

# Function to verify setup
verify_setup() {
    print_header "Verifying Setup"
    
    print_info "Checking camera list..."
    CAMERAS=$(curl -s "$API_BASE/cameras")
    CAMERA_COUNT=$(echo "$CAMERAS" | grep -o '"id"' | wc -l)
    
    if [ "$CAMERA_COUNT" -eq 3 ]; then
        print_success "All 3 cameras are registered"
    else
        print_warning "Expected 3 cameras, found $CAMERA_COUNT"
    fi
    
    print_info "Checking system status..."
    STATUS=$(curl -s "$API_BASE/system/status")
    if echo "$STATUS" | grep -q "active_pipelines"; then
        PIPELINES=$(echo "$STATUS" | grep -o '"active_pipelines":[0-9]*' | cut -d':' -f2)
        print_success "System status OK, active pipelines: $PIPELINES"
    else
        print_warning "Could not get system status"
    fi
}

# Function to show access information
show_access_info() {
    print_header "Access Information"
    
    echo "MJPEG Streams:"
    echo "  Camera 1 (192.168.1.2):   http://localhost:8161"
    echo "  Camera 2 (192.168.1.3):   http://localhost:8162"
    echo "  Camera 3 (192.168.1.226): http://localhost:8163"
    echo ""
    echo "Web UI: http://localhost:3000"
    echo ""
    echo "API Endpoints:"
    echo "  Cameras: $API_BASE/cameras"
    echo "  Status:  $API_BASE/system/status"
    echo ""
    echo "Backend Log: tail -f backend.log"
}

# Main execution
main() {
    print_header "Multi-Camera Test with Complete System Cleanup"
    print_warning "This will stop all processes and clear all data!"
    
    read -p "Continue? (y/N): " -n 1 -r
    echo
    if [[ ! $REPLY =~ ^[Yy]$ ]]; then
        print_info "Test cancelled"
        exit 0
    fi
    
    stop_processes
    clean_data
    start_backend
    add_cameras
    verify_setup
    show_access_info
    
    print_header "Test Complete"
    print_success "Multi-camera setup is ready for testing!"
}

# Run main function
main "$@"
