#!/bin/bash

# Build and Test RKNN YOLOv8 Support Script
# This script builds the project with RKNN support and runs basic tests

set -e  # Exit on any error

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

print_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

print_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check RKNN library
check_rknn_library() {
    print_status "Checking RKNN library..."
    
    # Check for library file
    if [ -f "/usr/lib/librknn_api.so" ] || [ -f "/usr/lib/aarch64-linux-gnu/librknn_api.so" ] || [ -f "/opt/rknn/lib/librknn_api.so" ]; then
        print_success "RKNN library found"
        return 0
    else
        print_warning "RKNN library not found in standard locations"
        return 1
    fi
}

# Function to check RKNN header
check_rknn_header() {
    print_status "Checking RKNN header..."
    
    # Check for header file
    if [ -f "/usr/include/rknn_api.h" ] || [ -f "/opt/rknn/include/rknn_api.h" ]; then
        print_success "RKNN header found"
        return 0
    else
        print_warning "RKNN header not found in standard locations"
        return 1
    fi
}

# Function to build the project
build_project() {
    print_status "Building project..."
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Configure with CMake
    print_status "Configuring with CMake..."
    cmake .. -DCMAKE_BUILD_TYPE=Release
    
    # Check if RKNN was detected
    if grep -q "RKNN found: TRUE" CMakeCache.txt 2>/dev/null; then
        print_success "RKNN support enabled"
    else
        print_warning "RKNN support not enabled - check library installation"
    fi
    
    # Build
    print_status "Compiling..."
    make -j$(nproc)
    
    cd ..
    print_success "Build completed"
}

# Function to download test model
download_test_model() {
    print_status "Checking for test models..."
    
    mkdir -p models
    
    # Check if ONNX model exists
    if [ ! -f "models/yolov8n.onnx" ]; then
        print_status "Downloading YOLOv8n ONNX model..."
        if command_exists wget; then
            wget -O models/yolov8n.onnx "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8n.onnx"
        elif command_exists curl; then
            curl -L -o models/yolov8n.onnx "https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8n.onnx"
        else
            print_error "Neither wget nor curl found. Please download yolov8n.onnx manually."
            return 1
        fi
        print_success "ONNX model downloaded"
    else
        print_success "ONNX model already exists"
    fi
}

# Function to convert model to RKNN
convert_to_rknn() {
    if [ -f "models/yolov8n.onnx" ] && command_exists python3; then
        print_status "Converting ONNX model to RKNN..."
        
        # Check if rknn-toolkit2 is available
        if python3 -c "import rknn.api" 2>/dev/null; then
            python3 scripts/convert_yolov8_to_rknn.py \
                --input models/yolov8n.onnx \
                --output models/yolov8n.rknn \
                --platform rk3588 \
                --quantize INT8
            
            if [ -f "models/yolov8n.rknn" ]; then
                print_success "RKNN model created successfully"
                return 0
            else
                print_error "RKNN model conversion failed"
                return 1
            fi
        else
            print_warning "RKNN-Toolkit2 not available, skipping RKNN model conversion"
            return 1
        fi
    else
        print_warning "Cannot convert model - missing ONNX file or Python3"
        return 1
    fi
}

# Function to create test image
create_test_image() {
    print_status "Creating test image..."
    
    if command_exists python3; then
        python3 -c "
import cv2
import numpy as np

# Create a simple test image with some shapes
img = np.zeros((480, 640, 3), dtype=np.uint8)
img.fill(50)  # Dark gray background

# Draw some rectangles (simulate objects)
cv2.rectangle(img, (100, 100), (200, 300), (0, 255, 0), -1)  # Green rectangle
cv2.rectangle(img, (300, 150), (450, 250), (255, 0, 0), -1)  # Blue rectangle
cv2.rectangle(img, (500, 200), (600, 400), (0, 0, 255), -1)  # Red rectangle

# Add some text
cv2.putText(img, 'Test Image', (50, 50), cv2.FONT_HERSHEY_SIMPLEX, 1, (255, 255, 255), 2)

cv2.imwrite('test_image.jpg', img)
print('Test image created: test_image.jpg')
"
        print_success "Test image created"
    else
        print_warning "Python3 not available, cannot create test image"
        return 1
    fi
}

# Function to run tests
run_tests() {
    print_status "Running tests..."
    
    if [ ! -f "build/test_rknn_yolov8" ]; then
        print_error "Test executable not found"
        return 1
    fi
    
    # Test with RKNN model if available
    if [ -f "models/yolov8n.rknn" ] && [ -f "test_image.jpg" ]; then
        print_status "Testing RKNN backend..."
        ./build/test_rknn_yolov8 -m models/yolov8n.rknn -i test_image.jpg -b rknn -o result_rknn.jpg -v
        print_success "RKNN test completed"
    fi
    
    # Test with OpenCV backend
    if [ -f "models/yolov8n.onnx" ] && [ -f "test_image.jpg" ]; then
        print_status "Testing OpenCV backend..."
        ./build/test_rknn_yolov8 -m models/yolov8n.onnx -i test_image.jpg -b opencv -o result_opencv.jpg -v
        print_success "OpenCV test completed"
    fi
    
    # Test auto backend selection
    if [ -f "test_image.jpg" ]; then
        print_status "Testing auto backend selection..."
        if [ -f "models/yolov8n.rknn" ]; then
            ./build/test_rknn_yolov8 -m models/yolov8n.rknn -i test_image.jpg -b auto -o result_auto.jpg -v
        elif [ -f "models/yolov8n.onnx" ]; then
            ./build/test_rknn_yolov8 -m models/yolov8n.onnx -i test_image.jpg -b auto -o result_auto.jpg -v
        fi
        print_success "Auto backend test completed"
    fi
}

# Main execution
main() {
    echo "======================================"
    echo "RKNN YOLOv8 Build and Test Script"
    echo "======================================"
    echo
    
    # Check system requirements
    print_status "Checking system requirements..."
    
    if ! command_exists cmake; then
        print_error "CMake not found. Please install cmake."
        exit 1
    fi
    
    if ! command_exists make; then
        print_error "Make not found. Please install build-essential."
        exit 1
    fi
    
    if ! command_exists pkg-config; then
        print_error "pkg-config not found. Please install pkg-config."
        exit 1
    fi
    
    print_success "Basic build tools found"
    
    # Check RKNN availability
    rknn_available=false
    if check_rknn_library && check_rknn_header; then
        rknn_available=true
        print_success "RKNN support available"
    else
        print_warning "RKNN support not available - will use OpenCV backend"
    fi
    
    # Build project
    build_project
    
    # Download/prepare models
    download_test_model
    
    # Convert to RKNN if possible
    if [ "$rknn_available" = true ]; then
        convert_to_rknn
    fi
    
    # Create test image
    create_test_image
    
    # Run tests
    run_tests
    
    echo
    print_success "Build and test completed!"
    echo
    echo "Available executables:"
    echo "  - ./build/AISecurityVision (main application)"
    echo "  - ./build/test_rknn_yolov8 (RKNN test program)"
    echo
    echo "Available models:"
    if [ -f "models/yolov8n.onnx" ]; then
        echo "  - models/yolov8n.onnx (OpenCV backend)"
    fi
    if [ -f "models/yolov8n.rknn" ]; then
        echo "  - models/yolov8n.rknn (RKNN backend)"
    fi
    echo
    echo "Test results:"
    if [ -f "result_rknn.jpg" ]; then
        echo "  - result_rknn.jpg (RKNN detection result)"
    fi
    if [ -f "result_opencv.jpg" ]; then
        echo "  - result_opencv.jpg (OpenCV detection result)"
    fi
    if [ -f "result_auto.jpg" ]; then
        echo "  - result_auto.jpg (Auto backend detection result)"
    fi
    echo
    echo "For more information, see docs/RKNN_SETUP_GUIDE.md"
}

# Run main function
main "$@"
