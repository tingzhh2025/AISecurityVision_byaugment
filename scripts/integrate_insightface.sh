#!/bin/bash

# InsightFace Integration Script
# This script integrates InsightFace library into the AI Security Vision System
# to replace the RKNN-based age/gender analyzer

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INSIGHTFACE_PATH="/userdata/source/source/insightface/cpp-package/inspireface"

echo "=== InsightFace Integration Script ==="
echo "Project root: $PROJECT_ROOT"
echo "InsightFace path: $INSIGHTFACE_PATH"
echo

# Check if InsightFace exists
if [ ! -d "$INSIGHTFACE_PATH" ]; then
    echo "Error: InsightFace not found at $INSIGHTFACE_PATH"
    exit 1
fi

# Check if InsightFace is built
if [ ! -f "$INSIGHTFACE_PATH/build/lib/libInspireFace.so" ] && [ ! -f "$INSIGHTFACE_PATH/build/lib/libInspireFace.a" ]; then
    echo "Warning: InsightFace library not found. Building InsightFace..."
    
    cd "$INSIGHTFACE_PATH"
    
    # Create build directory if it doesn't exist
    mkdir -p build
    cd build
    
    # Configure with CMake
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DISF_BUILD_WITH_SAMPLE=ON \
        -DISF_BUILD_WITH_TEST=ON \
        -DISF_BUILD_SHARED_LIBS=ON \
        -DCMAKE_INSTALL_PREFIX="$INSIGHTFACE_PATH/install"
    
    # Build
    make -j$(nproc)
    
    # Install
    make install
    
    echo "InsightFace built successfully"
    cd "$PROJECT_ROOT"
else
    echo "InsightFace library found"
fi

# Update CMakeLists.txt to include InsightFace
echo "Updating CMakeLists.txt..."

# Backup original CMakeLists.txt
cp "$PROJECT_ROOT/CMakeLists.txt" "$PROJECT_ROOT/CMakeLists.txt.backup"

# Add InsightFace configuration to CMakeLists.txt
cat >> "$PROJECT_ROOT/CMakeLists.txt" << 'EOF'

# InsightFace Integration
set(INSIGHTFACE_ROOT "/userdata/source/source/insightface/cpp-package/inspireface")
set(INSIGHTFACE_INCLUDE_DIR "${INSIGHTFACE_ROOT}/cpp/inspireface/c_api")
set(INSIGHTFACE_LIB_DIR "${INSIGHTFACE_ROOT}/build/lib")

# Find InsightFace library
find_library(INSIGHTFACE_LIB 
    NAMES InspireFace libInspireFace
    PATHS ${INSIGHTFACE_LIB_DIR}
    NO_DEFAULT_PATH
)

if(INSIGHTFACE_LIB)
    message(STATUS "Found InsightFace library: ${INSIGHTFACE_LIB}")
    
    # Add InsightFace include directory
    target_include_directories(${PROJECT_NAME} PRIVATE ${INSIGHTFACE_INCLUDE_DIR})
    
    # Link InsightFace library
    target_link_libraries(${PROJECT_NAME} ${INSIGHTFACE_LIB})
    
    # Add InsightFace source files
    target_sources(${PROJECT_NAME} PRIVATE
        src/ai/InsightFaceAnalyzer.cpp
    )
    
    # Define HAVE_INSIGHTFACE
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_INSIGHTFACE=1)
    
    message(STATUS "InsightFace integration enabled")
else()
    message(WARNING "InsightFace library not found. Using fallback implementation.")
endif()
EOF

echo "CMakeLists.txt updated"

# Create a compatibility header to allow gradual migration
echo "Creating compatibility header..."

cat > "$PROJECT_ROOT/src/ai/AgeGenderAnalyzerCompat.h" << 'EOF'
/**
 * @file AgeGenderAnalyzerCompat.h
 * @brief Compatibility header for gradual migration to InsightFace
 */

#ifndef AGE_GENDER_ANALYZER_COMPAT_H
#define AGE_GENDER_ANALYZER_COMPAT_H

#ifdef HAVE_INSIGHTFACE
#include "InsightFaceAnalyzer.h"
namespace AISecurityVision {
    // Use InsightFace implementation
    using AgeGenderAnalyzer = InsightFaceAnalyzer;
}
#else
#include "AgeGenderAnalyzer.h"
// Use original RKNN implementation
#endif

#endif // AGE_GENDER_ANALYZER_COMPAT_H
EOF

echo "Compatibility header created"

# Update include statements in source files
echo "Updating source file includes..."

# Find all source files that include AgeGenderAnalyzer.h
find "$PROJECT_ROOT/src" -name "*.cpp" -o -name "*.h" | xargs grep -l "AgeGenderAnalyzer.h" | while read file; do
    echo "Updating $file"
    sed -i 's/#include "AgeGenderAnalyzer.h"/#include "AgeGenderAnalyzerCompat.h"/' "$file"
    sed -i 's/#include "..\/ai\/AgeGenderAnalyzer.h"/#include "..\/ai\/AgeGenderAnalyzerCompat.h"/' "$file"
done

# Create a test program for InsightFace
echo "Creating InsightFace test program..."

cat > "$PROJECT_ROOT/tests/insightface_test.cpp" << 'EOF'
/**
 * @file insightface_test.cpp
 * @brief Test program for InsightFace integration
 */

#include <iostream>
#include <opencv2/opencv.hpp>
#include "../src/ai/InsightFaceAnalyzer.h"

using namespace AISecurityVision;

int main(int argc, char* argv[]) {
    std::cout << "=== InsightFace Integration Test ===" << std::endl;
    
    // Test InsightFace analyzer
    InsightFaceAnalyzer analyzer;
    
    // Try to initialize
    std::string packPath = "models/Pikachu.pack";
    if (argc > 1) {
        packPath = argv[1];
    }
    
    std::cout << "Initializing with pack: " << packPath << std::endl;
    
    if (!analyzer.initialize(packPath)) {
        std::cout << "Failed to initialize InsightFace analyzer" << std::endl;
        std::cout << "Make sure the pack file exists: " << packPath << std::endl;
        return 1;
    }
    
    std::cout << "InsightFace analyzer initialized successfully!" << std::endl;
    
    // Print model info
    auto modelInfo = analyzer.getModelInfo();
    std::cout << "\nModel Information:" << std::endl;
    for (const auto& info : modelInfo) {
        std::cout << "  " << info << std::endl;
    }
    
    // Test with a sample image if provided
    if (argc > 2) {
        std::string imagePath = argv[2];
        std::cout << "\nTesting with image: " << imagePath << std::endl;
        
        cv::Mat image = cv::imread(imagePath);
        if (image.empty()) {
            std::cout << "Failed to load image: " << imagePath << std::endl;
            return 1;
        }
        
        // Analyze the image
        auto attributes = analyzer.analyzeSingle(image);
        
        std::cout << "Analysis result:" << std::endl;
        std::cout << "  " << attributes.toString() << std::endl;
        std::cout << "  Inference time: " << analyzer.getLastInferenceTime() << "ms" << std::endl;
    }
    
    std::cout << "\n=== Test Completed Successfully ===" << std::endl;
    return 0;
}
EOF

# Update CMakeLists.txt to include the test
cat >> "$PROJECT_ROOT/CMakeLists.txt" << 'EOF'

# InsightFace test program
if(INSIGHTFACE_LIB)
    add_executable(insightface_test tests/insightface_test.cpp src/ai/InsightFaceAnalyzer.cpp)
    target_include_directories(insightface_test PRIVATE ${INSIGHTFACE_INCLUDE_DIR})
    target_link_libraries(insightface_test ${INSIGHTFACE_LIB} ${OpenCV_LIBS})
    target_compile_definitions(insightface_test PRIVATE HAVE_INSIGHTFACE=1)
endif()
EOF

echo "Test program created"

# Create setup instructions
cat > "$PROJECT_ROOT/INSIGHTFACE_SETUP.md" << 'EOF'
# InsightFace Integration Setup

## Prerequisites

1. InsightFace library built and installed
2. Model pack file (e.g., Pikachu.pack)

## Build Instructions

```bash
# 1. Build the project with InsightFace support
mkdir build && cd build
cmake ..
make -j$(nproc)

# 2. Test InsightFace integration
./insightface_test models/Pikachu.pack [test_image.jpg]
```

## Model Pack

The InsightFace analyzer requires a model pack file. You can:

1. Use an existing pack file from InsightFace
2. Create your own pack file with required models
3. Download from InsightFace model zoo

Place the pack file in the `models/` directory as `Pikachu.pack` or specify the path when initializing.

## Migration

The integration uses a compatibility header that automatically selects:
- InsightFaceAnalyzer when HAVE_INSIGHTFACE is defined
- Original AgeGenderAnalyzer as fallback

This allows gradual migration without breaking existing code.

## Features

InsightFace provides:
- Age group recognition (9 age brackets)
- Gender recognition (male/female)
- Race recognition (5 categories)
- Face quality assessment
- Mask detection
- High accuracy and performance
EOF

echo
echo "=== Integration Complete ==="
echo
echo "Next steps:"
echo "1. Ensure InsightFace model pack is available at models/Pikachu.pack"
echo "2. Rebuild the project: cd build && make -j\$(nproc)"
echo "3. Test with: ./insightface_test models/Pikachu.pack"
echo "4. Check INSIGHTFACE_SETUP.md for detailed instructions"
echo
echo "The system will automatically use InsightFace when available,"
echo "or fall back to the original RKNN implementation."
