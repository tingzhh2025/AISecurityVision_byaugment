#!/bin/bash

# Test script for adding cameras one by one with system monitoring
# This script helps identify which camera causes issues

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

# Function to check if backend is running
check_backend() {
    if curl -s "$API_BASE/system/status" > /dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# Function to wait for backend to be ready
wait_for_backend() {
    print_info "Waiting for backend to be ready..."
    for i in {1..30}; do
        if check_backend; then
            print_success "Backend is ready!"
            return 0
        fi
        echo -n "."
        sleep 1
    done
    print_error "Backend failed to start within 30 seconds"
    return 1
}

# Function to start backend if not running
start_backend_if_needed() {
    if check_backend; then
        print_info "Backend is already running"
        return 0
    fi
    
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
    wait_for_backend
}

# Function to add a single camera
add_camera() {
    local camera_id="$1"
    local camera_name="$2"
    local camera_url="$3"
    local username="$4"
    local password="$5"
    local port="$6"
    
    print_header "Adding Camera: $camera_name"
    print_info "URL: $camera_url"
    print_info "MJPEG Port: $port"
    
    # Check backend before adding camera
    if ! check_backend; then
        print_error "Backend is not running!"
        return 1
    fi
    
    # Prepare JSON payload
    local json_payload="{
        \"id\": \"$camera_id\",
        \"name\": \"$camera_name\",
        \"url\": \"$camera_url\",
        \"protocol\": \"rtsp\",
        \"username\": \"$username\",
        \"password\": \"$password\",
        \"width\": 1920,
        \"height\": 1080,
        \"fps\": 25,
        \"mjpeg_port\": $port,
        \"enabled\": true
    }"
    
    # Add camera
    RESPONSE=$(curl -s -w "HTTPSTATUS:%{http_code}" -X POST "$API_BASE/cameras" \
        -H "Content-Type: application/json" \
        -d "$json_payload")
    
    HTTP_STATUS=$(echo $RESPONSE | tr -d '\n' | sed -e 's/.*HTTPSTATUS://')
    RESPONSE_BODY=$(echo $RESPONSE | sed -e 's/HTTPSTATUS:.*//g')
    
    if [ "$HTTP_STATUS" -eq 200 ] || [ "$HTTP_STATUS" -eq 201 ]; then
        print_success "Camera added successfully (HTTP $HTTP_STATUS)"
    else
        print_error "Failed to add camera (HTTP $HTTP_STATUS)"
        echo "Response: $RESPONSE_BODY"
        return 1
    fi
    
    # Wait for pipeline initialization
    print_info "Waiting for pipeline initialization..."
    sleep 5
    
    # Check if backend is still running
    if ! check_backend; then
        print_error "Backend crashed after adding camera!"
        return 1
    fi
    
    # Check system status
    print_info "Checking system status..."
    STATUS=$(curl -s "$API_BASE/system/status")
    if echo "$STATUS" | grep -q "active_pipelines"; then
        PIPELINES=$(echo "$STATUS" | grep -o '"active_pipelines":[0-9]*' | cut -d':' -f2)
        print_success "System status OK, active pipelines: $PIPELINES"
    else
        print_warning "Could not get system status"
    fi
    
    print_success "Camera $camera_name added and verified"
    return 0
}

# Function to show current status
show_status() {
    print_header "Current System Status"
    
    if ! check_backend; then
        print_error "Backend is not running"
        return 1
    fi
    
    # Get camera list
    print_info "Checking camera list..."
    CAMERAS=$(curl -s "$API_BASE/cameras")
    CAMERA_COUNT=$(echo "$CAMERAS" | grep -o '"id"' | wc -l)
    print_info "Total cameras: $CAMERA_COUNT"
    
    # Get system status
    print_info "Checking system status..."
    STATUS=$(curl -s "$API_BASE/system/status")
    if echo "$STATUS" | grep -q "active_pipelines"; then
        PIPELINES=$(echo "$STATUS" | grep -o '"active_pipelines":[0-9]*' | cut -d':' -f2)
        print_info "Active pipelines: $PIPELINES"
    fi
}

# Function to clear all cameras
clear_cameras() {
    print_header "Clearing All Cameras"
    
    if ! check_backend; then
        print_warning "Backend is not running, cannot clear cameras"
        return 1
    fi
    
    # Get current camera list
    CAMERAS=$(curl -s "$API_BASE/cameras" | grep -o '"id":"[^"]*"' | sed 's/"id":"//g' | sed 's/"//g')
    
    if [ -n "$CAMERAS" ]; then
        print_info "Found existing cameras, removing them..."
        for camera_id in $CAMERAS; do
            print_info "  Removing camera: $camera_id"
            curl -s -X DELETE "$API_BASE/cameras/$camera_id" > /dev/null
        done
        print_success "All existing cameras removed"
        sleep 2
    else
        print_info "No existing cameras found"
    fi
}

# Main menu function
show_menu() {
    echo ""
    echo "=== Camera Test Menu ==="
    echo "1. Start backend (if not running)"
    echo "2. Clear all cameras"
    echo "3. Add Camera 1 (192.168.1.2)"
    echo "4. Add Camera 2 (192.168.1.3)" 
    echo "5. Add Camera 3 (192.168.1.226)"
    echo "6. Show current status"
    echo "7. Add all cameras sequentially"
    echo "8. Exit"
    echo ""
}

# Main execution
main() {
    print_header "Single Camera Test Tool"
    
    while true; do
        show_menu
        read -p "Select option (1-8): " choice
        
        case $choice in
            1)
                start_backend_if_needed
                ;;
            2)
                clear_cameras
                ;;
            3)
                add_camera "camera_192_168_1_2" "Camera-192.168.1.2" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" "admin" "sharpi1688" "8161"
                ;;
            4)
                add_camera "camera_192_168_1_3" "Camera-192.168.1.3" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" "admin" "sharpi1688" "8162"
                ;;
            5)
                add_camera "camera_192_168_1_226" "Camera-192.168.1.226" "rtsp://192.168.1.226:8554/unicast" "" "" "8163"
                ;;
            6)
                show_status
                ;;
            7)
                print_header "Adding All Cameras Sequentially"
                start_backend_if_needed || continue
                clear_cameras || continue
                
                add_camera "camera_192_168_1_2" "Camera-192.168.1.2" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" "admin" "sharpi1688" "8161" || continue
                add_camera "camera_192_168_1_3" "Camera-192.168.1.3" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" "admin" "sharpi1688" "8162" || continue
                add_camera "camera_192_168_1_226" "Camera-192.168.1.226" "rtsp://192.168.1.226:8554/unicast" "" "" "8163" || continue
                
                print_success "All cameras added successfully!"
                show_status
                ;;
            8)
                print_info "Exiting..."
                exit 0
                ;;
            *)
                print_error "Invalid option. Please select 1-8."
                ;;
        esac
    done
}

# Run main function
main "$@"
