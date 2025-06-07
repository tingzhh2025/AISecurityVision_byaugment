#!/bin/bash

# Test model compatibility for TensorRT InsightFace
# This script tests if the current model packs are compatible with x86_64 TensorRT

set -e

PROJECT_ROOT="/home/rogers/source/custom/AISecurityVision_byaugment"
cd "$PROJECT_ROOT"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Testing Model Compatibility for TensorRT${NC}"
echo -e "${GREEN}========================================${NC}"

# Test 1: Check current model packs
echo -e "${BLUE}1. Analyzing Model Packs${NC}"

RK3588_MODEL="models/Pikachu.pack"
TENSORRT_MODEL="models/Pikachu_x86_64.pack"

if [ -f "$RK3588_MODEL" ]; then
    echo -e "   ${GREEN}✅ RK3588 model pack found: $RK3588_MODEL${NC}"
    
    echo -e "   ${BLUE}Analyzing RK3588 model contents:${NC}"
    tar -tf "$RK3588_MODEL" | head -10 | while read line; do
        if [[ "$line" == *"_rk3588"* ]]; then
            echo -e "     ${RED}❌ RK3588-specific: $line${NC}"
        else
            echo -e "     ${YELLOW}⚠️  Generic: $line${NC}"
        fi
    done
    
    # Count RK3588-specific models
    RK3588_COUNT=$(tar -tf "$RK3588_MODEL" | grep -c "_rk3588" || echo "0")
    TOTAL_COUNT=$(tar -tf "$RK3588_MODEL" | wc -l)
    echo -e "   ${YELLOW}RK3588-specific models: $RK3588_COUNT / $TOTAL_COUNT${NC}"
    
    if [ "$RK3588_COUNT" -gt 0 ]; then
        echo -e "   ${RED}❌ This model pack is incompatible with x86_64 TensorRT${NC}"
    else
        echo -e "   ${GREEN}✅ This model pack may be compatible with x86_64 TensorRT${NC}"
    fi
else
    echo -e "   ${RED}❌ RK3588 model pack not found${NC}"
fi

echo ""

if [ -f "$TENSORRT_MODEL" ]; then
    echo -e "   ${GREEN}✅ TensorRT model pack found: $TENSORRT_MODEL${NC}"
    
    # Check if it's a placeholder
    if tar -tf "$TENSORRT_MODEL" | grep -q "README_TENSORRT.txt"; then
        echo -e "   ${YELLOW}⚠️  This is a placeholder model pack${NC}"
        echo -e "   ${BLUE}Placeholder contents:${NC}"
        tar -xOf "$TENSORRT_MODEL" README_TENSORRT.txt | head -5 | sed 's/^/     /'
    else
        echo -e "   ${GREEN}✅ Contains actual model files${NC}"
        echo -e "   ${BLUE}TensorRT model contents:${NC}"
        tar -tf "$TENSORRT_MODEL" | head -10 | while read line; do
            echo -e "     ${GREEN}✅ $line${NC}"
        done
    fi
else
    echo -e "   ${RED}❌ TensorRT model pack not found${NC}"
fi

# Test 2: Check library compatibility
echo -e "\n${BLUE}2. Testing Library Compatibility${NC}"

INSIGHTFACE_LIB="third_party/insightface/lib/libInspireFace.so"
if [ -f "$INSIGHTFACE_LIB" ]; then
    echo -e "   ${GREEN}✅ InsightFace library found${NC}"
    
    # Check TensorRT linking
    if ldd "$INSIGHTFACE_LIB" | grep -q "libnvinfer"; then
        echo -e "   ${GREEN}✅ TensorRT libraries linked${NC}"
        TENSORRT_VERSION=$(ldd "$INSIGHTFACE_LIB" | grep libnvinfer | head -1 | awk '{print $1}')
        echo -e "   TensorRT library: ${YELLOW}$TENSORRT_VERSION${NC}"
    else
        echo -e "   ${RED}❌ TensorRT libraries not linked${NC}"
    fi
    
    # Check CUDA linking
    if ldd "$INSIGHTFACE_LIB" | grep -q "libcuda"; then
        echo -e "   ${GREEN}✅ CUDA libraries linked${NC}"
    else
        echo -e "   ${RED}❌ CUDA libraries not linked${NC}"
    fi
else
    echo -e "   ${RED}❌ InsightFace library not found${NC}"
fi

# Test 3: Create a minimal test
echo -e "\n${BLUE}3. Creating Minimal Compatibility Test${NC}"

# Create a simple C program to test InsightFace initialization
cat > test_minimal.c << 'EOF'
#include <stdio.h>
#include <dlfcn.h>

int main() {
    printf("Testing InsightFace library loading...\n");
    
    // Try to load the InsightFace library
    void* handle = dlopen("third_party/insightface/lib/libInspireFace.so", RTLD_LAZY);
    if (!handle) {
        printf("❌ Failed to load InsightFace library: %s\n", dlerror());
        return 1;
    }
    
    printf("✅ InsightFace library loaded successfully\n");
    
    // Try to get the HFLaunchInspireFace function
    void* func = dlsym(handle, "HFLaunchInspireFace");
    if (!func) {
        printf("❌ Failed to find HFLaunchInspireFace function: %s\n", dlerror());
        dlclose(handle);
        return 1;
    }
    
    printf("✅ HFLaunchInspireFace function found\n");
    
    dlclose(handle);
    printf("✅ Library test completed successfully\n");
    return 0;
}
EOF

# Compile and run the test
echo -e "   ${YELLOW}Compiling minimal test...${NC}"
if gcc -o test_minimal test_minimal.c -ldl; then
    echo -e "   ${GREEN}✅ Compilation successful${NC}"
    
    echo -e "   ${YELLOW}Running minimal test...${NC}"
    if ./test_minimal; then
        echo -e "   ${GREEN}✅ Minimal test passed${NC}"
    else
        echo -e "   ${RED}❌ Minimal test failed${NC}"
    fi
else
    echo -e "   ${RED}❌ Compilation failed${NC}"
fi

# Cleanup
rm -f test_minimal test_minimal.c

# Test 4: Provide recommendations
echo -e "\n${BLUE}4. Recommendations${NC}"

if [ -f "$TENSORRT_MODEL" ] && tar -tf "$TENSORRT_MODEL" | grep -q "README_TENSORRT.txt"; then
    echo -e "${YELLOW}Current Status: Using placeholder model pack${NC}"
    echo ""
    echo -e "${BLUE}To resolve the model compatibility issue:${NC}"
    echo -e "1. ${GREEN}Option A: Obtain TensorRT-compatible models${NC}"
    echo -e "   - Download pre-built TensorRT InsightFace models"
    echo -e "   - Create a proper Pikachu_x86_64.pack with TensorRT models"
    echo -e "   - Replace the placeholder model pack"
    echo ""
    echo -e "2. ${GREEN}Option B: Convert existing models${NC}"
    echo -e "   - Use TensorRT tools to convert ONNX models to TensorRT format"
    echo -e "   - Create platform-specific model packs"
    echo ""
    echo -e "3. ${GREEN}Option C: Use CPU fallback${NC}"
    echo -e "   - Disable TensorRT acceleration for InsightFace"
    echo -e "   - Use CPU-based inference (slower but compatible)"
    echo ""
    echo -e "${YELLOW}Current workaround:${NC}"
    echo -e "- The system will detect the placeholder and provide informative error messages"
    echo -e "- Person statistics will be gracefully disabled on x86_64 until proper models are available"
    echo -e "- The application will continue to work for other features"
    
elif [ -f "$RK3588_MODEL" ] && [ $(tar -tf "$RK3588_MODEL" | grep -c "_rk3588" || echo "0") -gt 0 ]; then
    echo -e "${YELLOW}Current Status: RK3588-specific models detected${NC}"
    echo ""
    echo -e "${RED}Problem: The current Pikachu.pack contains RK3588-specific models${NC}"
    echo -e "${RED}These models will cause segmentation faults on x86_64 TensorRT${NC}"
    echo ""
    echo -e "${GREEN}Solution implemented:${NC}"
    echo -e "- Modified AgeGenderAnalyzer to use platform-specific model selection"
    echo -e "- Added graceful error handling for model compatibility issues"
    echo -e "- Created placeholder TensorRT model pack"
    echo -e "- Person statistics will be disabled until compatible models are provided"
    
else
    echo -e "${GREEN}Current Status: Model compatibility appears good${NC}"
    echo -e "- No obvious compatibility issues detected"
    echo -e "- Ready for testing with actual person statistics functionality"
fi

echo ""
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Model Compatibility Test Complete${NC}"
echo -e "${GREEN}========================================${NC}"
