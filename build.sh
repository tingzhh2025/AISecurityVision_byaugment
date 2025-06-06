#!/bin/bash
# Build script for AISecurityVision with TensorRT support

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}AISecurityVision Build Script${NC}"
echo "=============================="

# Parse arguments
BUILD_TYPE="Release"
ENABLE_TENSORRT="ON"
ENABLE_RKNN="ON"
CLEAN_BUILD=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --no-tensorrt)
            ENABLE_TENSORRT="OFF"
            shift
            ;;
        --no-rknn)
            ENABLE_RKNN="OFF"
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [--debug] [--no-tensorrt] [--no-rknn] [--clean]"
            exit 1
            ;;
    esac
done

# Get script directory
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$SCRIPT_DIR"
BUILD_DIR="$PROJECT_ROOT/build"

# Clean build if requested
if [ "$CLEAN_BUILD" = true ]; then
    echo -e "${YELLOW}Cleaning build directory...${NC}"
    rm -rf "$BUILD_DIR"
fi

# Create build directory
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure with CMake
echo -e "\n${GREEN}Configuring CMake...${NC}"
echo "Build type: $BUILD_TYPE"
echo "TensorRT support: $ENABLE_TENSORRT"
echo "RKNN support: $ENABLE_RKNN"

cmake .. \
    -DCMAKE_BUILD_TYPE=$BUILD_TYPE \
    -DENABLE_CUDA_TENSORRT=$ENABLE_TENSORRT \
    -DENABLE_RKNN_NPU=$ENABLE_RKNN \
    -DBUILD_TESTS=ON

# Check if configuration was successful
if [ $? -ne 0 ]; then
    echo -e "${RED}CMake configuration failed!${NC}"
    exit 1
fi

# Build
echo -e "\n${GREEN}Building...${NC}"
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Build failed!${NC}"
    exit 1
fi

echo -e "\n${GREEN}Build completed successfully!${NC}"

# Print summary
echo -e "\n${GREEN}Build Summary:${NC}"
echo "=============="
echo "Binary location: $BUILD_DIR/AISecurityVision"
echo "Test program: $BUILD_DIR/tests/test_yolov8_backends"

# Check for model files
echo -e "\n${YELLOW}Checking for model files...${NC}"
MODEL_DIR="$PROJECT_ROOT/models"

if [ ! -d "$MODEL_DIR" ]; then
    echo -e "${YELLOW}Creating models directory...${NC}"
    mkdir -p "$MODEL_DIR"
fi

# Check for RKNN model
if [ "$ENABLE_RKNN" = "ON" ] && [ ! -f "$MODEL_DIR/yolov8n.rknn" ]; then
    echo -e "${YELLOW}Warning: RKNN model not found at $MODEL_DIR/yolov8n.rknn${NC}"
    echo "Please convert your YOLOv8 model to RKNN format using:"
    echo "  python3 scripts/convert_yolov8_to_rknn.py"
fi

# Check for TensorRT model
if [ "$ENABLE_TENSORRT" = "ON" ] && [ ! -f "$MODEL_DIR/yolov8n_fp16.engine" ]; then
    echo -e "${YELLOW}Warning: TensorRT engine not found at $MODEL_DIR/yolov8n_fp16.engine${NC}"
    echo "Please convert your YOLOv8 model to TensorRT format using:"
    echo "  python3 scripts/convert_yolov8_to_tensorrt.py models/yolov8n.onnx"
fi

echo -e "\n${GREEN}Ready to run!${NC}"
echo "To test the backends, run:"
echo "  ./build/tests/test_yolov8_backends <image_path>"
