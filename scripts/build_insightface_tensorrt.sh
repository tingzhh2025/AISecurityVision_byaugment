#!/bin/bash

# Build InsightFace with CUDA TensorRT support
# This script compiles InsightFace library with TensorRT acceleration

set -e

# Configuration
INSIGHTFACE_SOURCE="/home/rogers/source/custom/insightface/cpp-package/inspireface"
BUILD_DIR="${INSIGHTFACE_SOURCE}/build_tensorrt"
INSTALL_DIR="/home/rogers/source/custom/AISecurityVision_byaugment/third_party/insightface"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Building InsightFace with CUDA TensorRT${NC}"
echo -e "${GREEN}========================================${NC}"

# Check if source directory exists
if [ ! -d "$INSIGHTFACE_SOURCE" ]; then
    echo -e "${RED}Error: InsightFace source directory not found: $INSIGHTFACE_SOURCE${NC}"
    exit 1
fi

# Check for CUDA installation
if ! command -v nvcc &> /dev/null; then
    echo -e "${RED}Error: CUDA not found. Please install CUDA toolkit first.${NC}"
    exit 1
fi

# Check for TensorRT installation
TENSORRT_PATHS=(
    "/usr/local/TensorRT"
    "/usr"
    "/usr/lib/x86_64-linux-gnu"
    "/usr/lib/aarch64-linux-gnu"
    "/opt/tensorrt"
)

TENSORRT_ROOT=""
for path in "${TENSORRT_PATHS[@]}"; do
    if [ -f "$path/include/NvInfer.h" ] || [ -f "$path/NvInfer.h" ] || [ -f "$path/include/x86_64-linux-gnu/NvInfer.h" ]; then
        TENSORRT_ROOT="$path"
        break
    fi
done

if [ -z "$TENSORRT_ROOT" ]; then
    echo -e "${RED}Error: TensorRT not found. Please install TensorRT first.${NC}"
    echo -e "${YELLOW}Checked paths: ${TENSORRT_PATHS[*]}${NC}"
    exit 1
fi

echo -e "${GREEN}Found TensorRT at: $TENSORRT_ROOT${NC}"

# Create build directory
echo -e "${YELLOW}Creating build directory...${NC}"
rm -rf "$BUILD_DIR"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake with TensorRT support
echo -e "${YELLOW}Configuring CMake with TensorRT support...${NC}"
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
    -DISF_ENABLE_TENSORRT=ON \
    -DTENSORRT_ROOT="$TENSORRT_ROOT" \
    -DISF_BUILD_WITH_TEST=OFF \
    -DISF_BUILD_WITH_SAMPLE=OFF \
    -DISF_ENABLE_OPENCV=OFF \
    -DISF_NEVER_USE_OPENCV=ON \
    -DISF_INSTALL_CPP_HEADER=ON \
    -DCMAKE_CXX_STANDARD=17

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: CMake configuration failed${NC}"
    exit 1
fi

# Build the library
echo -e "${YELLOW}Building InsightFace library...${NC}"
make -j$(nproc)

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Build failed${NC}"
    exit 1
fi

# Install the library
echo -e "${YELLOW}Installing InsightFace library...${NC}"
make install

if [ $? -ne 0 ]; then
    echo -e "${RED}Error: Installation failed${NC}"
    exit 1
fi

# Verify installation
echo -e "${YELLOW}Verifying installation...${NC}"
if [ -f "$INSTALL_DIR/lib/libInspireFace.a" ] || [ -f "$INSTALL_DIR/lib/libInspireFace.so" ]; then
    echo -e "${GREEN}✓ InsightFace library installed successfully${NC}"
else
    echo -e "${RED}✗ InsightFace library not found after installation${NC}"
    exit 1
fi

if [ -f "$INSTALL_DIR/include/inspireface.h" ]; then
    echo -e "${GREEN}✓ InsightFace headers installed successfully${NC}"
else
    echo -e "${RED}✗ InsightFace headers not found after installation${NC}"
    exit 1
fi

# Copy models if they exist
if [ -d "$INSIGHTFACE_SOURCE/models" ]; then
    echo -e "${YELLOW}Copying models...${NC}"
    cp -r "$INSIGHTFACE_SOURCE/models" "$INSTALL_DIR/"
    echo -e "${GREEN}✓ Models copied successfully${NC}"
fi

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}InsightFace with TensorRT build complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Installation directory: $INSTALL_DIR${NC}"
echo -e "${GREEN}Library: $INSTALL_DIR/lib/${NC}"
echo -e "${GREEN}Headers: $INSTALL_DIR/include/${NC}"
echo -e "${GREEN}Models: $INSTALL_DIR/models/${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo -e "1. Rebuild your main project with: ${GREEN}cd build && cmake .. -DENABLE_CUDA_TENSORRT=ON -DENABLE_RKNN_NPU=OFF && make${NC}"
echo -e "2. Test the TensorRT acceleration"
