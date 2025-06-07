#!/bin/bash

# Test TensorRT InsightFace integration
# This script verifies that TensorRT InsightFace is properly configured

set -e

# Configuration
PROJECT_ROOT="/home/rogers/source/custom/AISecurityVision_byaugment"
INSIGHTFACE_DIR="$PROJECT_ROOT/third_party/insightface"
BINARY_PATH="$PROJECT_ROOT/build/AISecurityVision"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Testing TensorRT InsightFace Integration${NC}"
echo -e "${GREEN}========================================${NC}"

# Test 1: Check platform detection
echo -e "${BLUE}1. Platform Detection Test${NC}"
PLATFORM=$(uname -m)
echo -e "   Detected platform: ${YELLOW}$PLATFORM${NC}"

if [[ "$PLATFORM" == "x86_64" ]]; then
    echo -e "   ${GREEN}✅ x86_64 platform detected - TensorRT compatible${NC}"
else
    echo -e "   ${YELLOW}⚠️  Non-x86_64 platform - TensorRT may not be optimal${NC}"
fi

# Test 2: Check CUDA availability
echo -e "\n${BLUE}2. CUDA Availability Test${NC}"
if command -v nvcc &> /dev/null; then
    CUDA_VERSION=$(nvcc --version | grep "release" | awk '{print $6}' | cut -c2-)
    echo -e "   ${GREEN}✅ CUDA found - version: $CUDA_VERSION${NC}"
else
    echo -e "   ${RED}❌ CUDA not found${NC}"
fi

if command -v nvidia-smi &> /dev/null; then
    echo -e "   ${GREEN}✅ NVIDIA driver found${NC}"
    nvidia-smi --query-gpu=name,memory.total --format=csv,noheader,nounits | while read line; do
        echo -e "   GPU: ${YELLOW}$line${NC}"
    done
else
    echo -e "   ${RED}❌ NVIDIA driver not found${NC}"
fi

# Test 3: Check TensorRT availability
echo -e "\n${BLUE}3. TensorRT Availability Test${NC}"
TENSORRT_PATHS=(
    "/usr/local/TensorRT"
    "/usr"
    "/usr/lib/x86_64-linux-gnu"
    "/opt/tensorrt"
)

TENSORRT_FOUND=false
for path in "${TENSORRT_PATHS[@]}"; do
    if [ -f "$path/include/NvInfer.h" ] || [ -f "$path/NvInfer.h" ] || [ -f "$path/include/x86_64-linux-gnu/NvInfer.h" ]; then
        echo -e "   ${GREEN}✅ TensorRT found at: $path${NC}"
        TENSORRT_FOUND=true
        break
    fi
done

if [ "$TENSORRT_FOUND" = false ]; then
    echo -e "   ${RED}❌ TensorRT not found${NC}"
fi

# Test 4: Check InsightFace library
echo -e "\n${BLUE}4. InsightFace Library Test${NC}"
INSIGHTFACE_LIB="$INSIGHTFACE_DIR/lib/libInspireFace.so"

if [ -f "$INSIGHTFACE_LIB" ]; then
    echo -e "   ${GREEN}✅ InsightFace library found: $INSIGHTFACE_LIB${NC}"
    
    # Check library size and modification time
    LIB_SIZE=$(stat -c%s "$INSIGHTFACE_LIB")
    LIB_TIME=$(stat -c%y "$INSIGHTFACE_LIB")
    echo -e "   Library size: ${YELLOW}$LIB_SIZE bytes${NC}"
    echo -e "   Last modified: ${YELLOW}$LIB_TIME${NC}"
    
    # Check if it's linked with TensorRT/CUDA
    echo -e "   ${BLUE}Library dependencies:${NC}"
    if ldd "$INSIGHTFACE_LIB" | grep -q "libnvinfer"; then
        echo -e "     ${GREEN}✅ TensorRT libraries linked${NC}"
    else
        echo -e "     ${YELLOW}⚠️  TensorRT libraries not directly linked${NC}"
    fi
    
    if ldd "$INSIGHTFACE_LIB" | grep -q "libcuda"; then
        echo -e "     ${GREEN}✅ CUDA libraries linked${NC}"
    else
        echo -e "     ${YELLOW}⚠️  CUDA libraries not directly linked${NC}"
    fi
    
    # Check for backup
    if [ -f "$INSIGHTFACE_LIB.rk3588_backup" ]; then
        echo -e "   ${GREEN}✅ RK3588 backup found${NC}"
    else
        echo -e "   ${YELLOW}⚠️  No RK3588 backup found${NC}"
    fi
else
    echo -e "   ${RED}❌ InsightFace library not found${NC}"
fi

# Test 5: Check model packs
echo -e "\n${BLUE}5. Model Pack Test${NC}"

# Check RK3588 model pack
RK3588_MODEL="$PROJECT_ROOT/models/Pikachu.pack"
if [ -f "$RK3588_MODEL" ]; then
    echo -e "   ${GREEN}✅ RK3588 model pack found: $RK3588_MODEL${NC}"
    
    # Check if it contains RK3588-specific models
    if tar -tf "$RK3588_MODEL" | grep -q "_rk3588"; then
        echo -e "     ${YELLOW}⚠️  Contains RK3588-specific models (incompatible with TensorRT)${NC}"
    else
        echo -e "     ${GREEN}✅ Platform-neutral models${NC}"
    fi
else
    echo -e "   ${RED}❌ RK3588 model pack not found${NC}"
fi

# Check TensorRT model pack
TENSORRT_MODEL="$PROJECT_ROOT/models/Pikachu_x86_64.pack"
if [ -f "$TENSORRT_MODEL" ]; then
    echo -e "   ${GREEN}✅ TensorRT model pack found: $TENSORRT_MODEL${NC}"
    
    # Check if it's a placeholder
    if tar -tf "$TENSORRT_MODEL" | grep -q "README_TENSORRT.txt"; then
        echo -e "     ${YELLOW}⚠️  This is a placeholder - actual TensorRT models needed${NC}"
    else
        echo -e "     ${GREEN}✅ Contains actual model files${NC}"
    fi
else
    echo -e "   ${RED}❌ TensorRT model pack not found${NC}"
fi

# Test 6: Check binary compilation
echo -e "\n${BLUE}6. Binary Compilation Test${NC}"
if [ -f "$BINARY_PATH" ]; then
    echo -e "   ${GREEN}✅ Binary found: $BINARY_PATH${NC}"
    
    # Check if binary is linked with InsightFace
    if ldd "$BINARY_PATH" | grep -q "libInspireFace"; then
        echo -e "   ${GREEN}✅ Binary linked with InsightFace${NC}"
        LINKED_LIB=$(ldd "$BINARY_PATH" | grep libInspireFace | awk '{print $3}')
        echo -e "   Linked library: ${YELLOW}$LINKED_LIB${NC}"
    else
        echo -e "   ${RED}❌ Binary not linked with InsightFace${NC}"
    fi
    
    # Check compilation flags
    if strings "$BINARY_PATH" | grep -q "HAVE_INSIGHTFACE"; then
        echo -e "   ${GREEN}✅ Compiled with InsightFace support${NC}"
    else
        echo -e "   ${YELLOW}⚠️  InsightFace support flag not found${NC}"
    fi
    
    if strings "$BINARY_PATH" | grep -q "HAVE_TENSORRT"; then
        echo -e "   ${GREEN}✅ Compiled with TensorRT support${NC}"
    else
        echo -e "   ${YELLOW}⚠️  TensorRT support flag not found${NC}"
    fi
else
    echo -e "   ${RED}❌ Binary not found - please compile first${NC}"
fi

# Test 7: Runtime test (if binary exists)
if [ -f "$BINARY_PATH" ]; then
    echo -e "\n${BLUE}7. Runtime Test${NC}"
    echo -e "   ${YELLOW}Testing InsightFace initialization...${NC}"
    
    # Create a simple test to check if InsightFace can initialize
    # This is a basic test - actual model loading will depend on having proper models
    echo -e "   ${BLUE}Note: Full runtime test requires proper TensorRT-compatible models${NC}"
    echo -e "   ${BLUE}Check application logs for detailed model loading results${NC}"
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}TensorRT InsightFace Test Complete${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${YELLOW}Summary and Recommendations:${NC}"
echo ""

# Provide recommendations based on test results
if [ "$TENSORRT_FOUND" = true ] && [ -f "$INSIGHTFACE_LIB" ]; then
    echo -e "${GREEN}✅ Basic TensorRT InsightFace setup appears correct${NC}"
    
    if [ -f "$TENSORRT_MODEL" ] && tar -tf "$TENSORRT_MODEL" | grep -q "README_TENSORRT.txt"; then
        echo -e "${YELLOW}⚠️  Action needed: Replace placeholder model pack with actual TensorRT models${NC}"
        echo -e "   ${BLUE}1. Obtain TensorRT-compatible InsightFace models${NC}"
        echo -e "   ${BLUE}2. Create proper Pikachu_x86_64.pack with TensorRT models${NC}"
        echo -e "   ${BLUE}3. Test person statistics functionality${NC}"
    else
        echo -e "${GREEN}✅ Ready for testing - run person statistics tests${NC}"
    fi
else
    echo -e "${RED}❌ TensorRT InsightFace setup incomplete${NC}"
    echo -e "   ${BLUE}1. Install CUDA and TensorRT if missing${NC}"
    echo -e "   ${BLUE}2. Run: ./scripts/setup_tensorrt_insightface.sh${NC}"
    echo -e "   ${BLUE}3. Rebuild the project with TensorRT support${NC}"
fi
