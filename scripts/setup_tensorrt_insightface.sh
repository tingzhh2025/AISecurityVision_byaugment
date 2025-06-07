#!/bin/bash

# Setup TensorRT-compatible InsightFace for x86_64 platforms
# This script replaces the RK3588-specific InsightFace with TensorRT-compatible version

set -e

# Configuration
INSIGHTFACE_TENSORRT_BUILD="/home/rogers/source/custom/insightface/cpp-package/inspireface/build_cuda_x86_64"
PROJECT_ROOT="/home/rogers/source/custom/AISecurityVision_byaugment"
INSIGHTFACE_DIR="$PROJECT_ROOT/third_party/insightface"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Setting up TensorRT InsightFace for x86_64${NC}"
echo -e "${GREEN}========================================${NC}"

# Check if TensorRT build exists
if [ ! -d "$INSIGHTFACE_TENSORRT_BUILD" ]; then
    echo -e "${RED}Error: TensorRT InsightFace build not found at: $INSIGHTFACE_TENSORRT_BUILD${NC}"
    echo -e "${YELLOW}Please run the TensorRT build script first:${NC}"
    echo -e "${BLUE}  ./scripts/build_insightface_tensorrt.sh${NC}"
    exit 1
fi

# Check if TensorRT library exists
TENSORRT_LIB="$INSIGHTFACE_TENSORRT_BUILD/lib/libInspireFace.so"
if [ ! -f "$TENSORRT_LIB" ]; then
    echo -e "${RED}Error: TensorRT InsightFace library not found at: $TENSORRT_LIB${NC}"
    exit 1
fi

echo -e "${GREEN}Found TensorRT InsightFace library: $TENSORRT_LIB${NC}"

# Backup current library
echo -e "${YELLOW}Backing up current InsightFace library...${NC}"
CURRENT_LIB="$INSIGHTFACE_DIR/lib/libInspireFace.so"
if [ -f "$CURRENT_LIB" ]; then
    cp "$CURRENT_LIB" "$CURRENT_LIB.rk3588_backup"
    echo -e "${GREEN}Backed up RK3588 library to: $CURRENT_LIB.rk3588_backup${NC}"
fi

# Copy TensorRT library
echo -e "${YELLOW}Installing TensorRT InsightFace library...${NC}"
mkdir -p "$INSIGHTFACE_DIR/lib"
cp "$TENSORRT_LIB" "$CURRENT_LIB"
echo -e "${GREEN}Installed TensorRT library to: $CURRENT_LIB${NC}"

# Check if we need to create a TensorRT-compatible model pack
CURRENT_MODEL="$PROJECT_ROOT/models/Pikachu.pack"
TENSORRT_MODEL="$PROJECT_ROOT/models/Pikachu_x86_64.pack"

if [ -f "$CURRENT_MODEL" ]; then
    echo -e "${YELLOW}Analyzing current model pack...${NC}"
    
    # Check if current model contains RK3588-specific models
    if tar -tf "$CURRENT_MODEL" | grep -q "_rk3588"; then
        echo -e "${YELLOW}Current model pack contains RK3588-specific models${NC}"
        echo -e "${YELLOW}This will cause compatibility issues on x86_64 TensorRT${NC}"
        
        # For now, we'll create a placeholder that indicates the issue
        echo -e "${YELLOW}Creating placeholder for TensorRT-compatible model pack...${NC}"
        
        # Create a temporary directory for model pack creation
        TEMP_DIR=$(mktemp -d)
        cd "$TEMP_DIR"
        
        # Create a README explaining the situation
        cat > README_TENSORRT.txt << 'EOF'
TensorRT Model Pack Required
============================

This placeholder indicates that a TensorRT-compatible model pack is needed
for x86_64 platforms with NVIDIA GPU acceleration.

The current Pikachu.pack contains RK3588-specific models that are incompatible
with TensorRT backend on x86_64 systems.

To resolve this issue:

1. Obtain TensorRT-compatible InsightFace models
2. Create a proper model pack with TensorRT-optimized models
3. Replace this placeholder with the actual model pack

Model Requirements:
- Face detection models compatible with TensorRT
- Age/gender recognition models for TensorRT
- Quality assessment models for TensorRT
- All models should be optimized for NVIDIA GPU inference

Platform: x86_64 with CUDA/TensorRT support
Backend: TensorRT (NN_INFERENCE_TENSORRT_CUDA)
EOF
        
        # Create the placeholder pack
        tar -cf "$TENSORRT_MODEL" README_TENSORRT.txt
        cd - > /dev/null
        rm -rf "$TEMP_DIR"
        
        echo -e "${GREEN}Created placeholder model pack: $TENSORRT_MODEL${NC}"
        echo -e "${YELLOW}Note: This is a placeholder. You need to obtain actual TensorRT-compatible models.${NC}"
    else
        echo -e "${GREEN}Current model pack appears to be platform-neutral${NC}"
        echo -e "${YELLOW}Creating copy for TensorRT use...${NC}"
        cp "$CURRENT_MODEL" "$TENSORRT_MODEL"
    fi
else
    echo -e "${YELLOW}No current model pack found at: $CURRENT_MODEL${NC}"
fi

# Verify the installation
echo -e "${YELLOW}Verifying TensorRT InsightFace installation...${NC}"

# Check library dependencies
echo -e "${BLUE}Checking library dependencies:${NC}"
if ldd "$CURRENT_LIB" | grep -q "libnvinfer"; then
    echo -e "${GREEN}  ✅ TensorRT libraries linked${NC}"
else
    echo -e "${YELLOW}  ⚠️  TensorRT libraries not found in dependencies${NC}"
    echo -e "${YELLOW}     This may be normal if TensorRT is dynamically loaded${NC}"
fi

if ldd "$CURRENT_LIB" | grep -q "libcuda"; then
    echo -e "${GREEN}  ✅ CUDA libraries linked${NC}"
else
    echo -e "${YELLOW}  ⚠️  CUDA libraries not found in dependencies${NC}"
fi

# Check file size (TensorRT version should be different from RK3588 version)
CURRENT_SIZE=$(stat -c%s "$CURRENT_LIB" 2>/dev/null || echo "0")
echo -e "${BLUE}Library size: ${CURRENT_SIZE} bytes${NC}"

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}TensorRT InsightFace setup complete!${NC}"
echo -e "${GREEN}========================================${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo -e "1. ${GREEN}Rebuild the project:${NC}"
echo -e "   ${BLUE}cd build && make${NC}"
echo -e "2. ${GREEN}Test person statistics:${NC}"
echo -e "   ${BLUE}./test_person_stats_x86.sh${NC}"
echo -e "3. ${GREEN}If you get model compatibility errors:${NC}"
echo -e "   ${BLUE}Obtain proper TensorRT-compatible InsightFace models${NC}"
echo -e "   ${BLUE}Replace the placeholder Pikachu_x86_64.pack with actual models${NC}"
echo ""
echo -e "${YELLOW}Important Notes:${NC}"
echo -e "- The current setup uses a placeholder model pack"
echo -e "- You may need actual TensorRT-optimized InsightFace models"
echo -e "- Check the application logs for model loading errors"
echo -e "- Person statistics will be disabled if model loading fails"
