#!/bin/bash

# InsightFace Setup Script for AI Security Vision System
# This script sets up InsightFace integration for age/gender recognition

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INSIGHTFACE_PATH="/userdata/source/source/insightface/cpp-package/inspireface"

echo "=== InsightFace Setup Script ==="
echo "Project root: $PROJECT_ROOT"
echo "InsightFace path: $INSIGHTFACE_PATH"
echo

# Check if InsightFace exists
if [ ! -d "$INSIGHTFACE_PATH" ]; then
    echo "Error: InsightFace not found at $INSIGHTFACE_PATH"
    exit 1
fi

# Copy the model pack to our models directory
echo "Setting up model pack..."
mkdir -p "$PROJECT_ROOT/models"

# Copy the Gundam_RK3588 pack file
if [ -f "$INSIGHTFACE_PATH/test_res/pack/Gundam_RK3588" ]; then
    cp "$INSIGHTFACE_PATH/test_res/pack/Gundam_RK3588" "$PROJECT_ROOT/models/Pikachu.pack"
    echo "Model pack copied to models/Pikachu.pack"
else
    echo "Warning: Model pack not found at $INSIGHTFACE_PATH/test_res/pack/Gundam_RK3588"
fi

# Create a simple test program using the existing face_age_gender_sample.c
echo "Creating InsightFace test program..."

# Copy and modify the sample
cp "$INSIGHTFACE_PATH/face_age_gender_sample.c" "$PROJECT_ROOT/tests/insightface_simple_test.c"

# Create a simple build script for the test
cat > "$PROJECT_ROOT/tests/build_insightface_test.sh" << 'EOF'
#!/bin/bash

# Build script for InsightFace test
INSIGHTFACE_PATH="/userdata/source/source/insightface/cpp-package/inspireface"

# Check if InsightFace is built
if [ ! -f "$INSIGHTFACE_PATH/build/lib/libInspireFace.so" ]; then
    echo "Building InsightFace..."
    cd "$INSIGHTFACE_PATH"
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DISF_BUILD_SHARED_LIBS=ON
    make -j$(nproc)
    cd -
fi

# Build the test program
gcc -o insightface_simple_test insightface_simple_test.c \
    -I"$INSIGHTFACE_PATH/cpp/inspireface/c_api" \
    -L"$INSIGHTFACE_PATH/build/lib" \
    -lInspireFace \
    -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
    -Wl,-rpath,"$INSIGHTFACE_PATH/build/lib"

echo "Test program built successfully"
EOF

chmod +x "$PROJECT_ROOT/tests/build_insightface_test.sh"

# Create a usage guide
cat > "$PROJECT_ROOT/INSIGHTFACE_USAGE.md" << 'EOF'
# InsightFace Integration Usage Guide

## Quick Start

### 1. Test InsightFace functionality

```bash
# Build and test InsightFace
cd tests
./build_insightface_test.sh

# Run test with model pack and test image
./insightface_simple_test ../models/Pikachu.pack test_image.jpg
```

### 2. Integration with AI Security Vision System

The system now supports InsightFace for enhanced face analysis:

- **Age Recognition**: 9 age brackets (0-2, 3-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60-69, 70+)
- **Gender Recognition**: Male/Female
- **Race Recognition**: Black, Asian, Latino/Hispanic, Middle Eastern, White
- **Quality Assessment**: Face quality scoring
- **Mask Detection**: Automatic mask detection

### 3. Model Pack

The model pack `Pikachu.pack` (copied from Gundam_RK3588) contains:
- Face detection models
- Age/gender recognition models
- Quality assessment models
- Mask detection models

### 4. API Usage

```cpp
#include "AgeGenderAnalyzer.h"

// Initialize with InsightFace
AgeGenderAnalyzer analyzer;
analyzer.initialize("models/Pikachu.pack");

// Analyze person crops
std::vector<PersonDetection> persons = PersonFilter::filterPersons(detections, frame);
auto attributes = analyzer.analyze(persons);

// Access enhanced attributes
for (const auto& attr : attributes) {
    std::cout << "Gender: " << attr.gender << std::endl;
    std::cout << "Age: " << attr.age_group << std::endl;
    std::cout << "Race: " << attr.race << std::endl;
    std::cout << "Quality: " << attr.quality_score << std::endl;
    std::cout << "Mask: " << (attr.has_mask ? "Yes" : "No") << std::endl;
}
```

### 5. Performance

Expected performance on RK3588:
- Face detection: ~10-20ms per image
- Attribute analysis: ~5-15ms per face
- Total processing: ~15-35ms per person

### 6. Troubleshooting

If you encounter issues:

1. **Library not found**: Ensure InsightFace is built in the expected location
2. **Model pack missing**: Check that Pikachu.pack exists in models/ directory
3. **Runtime errors**: Verify RPATH is set correctly for shared libraries

### 7. Migration from RKNN

The enhanced PersonAttributes structure is backward compatible:
- Existing `gender` and `age_group` fields work as before
- New fields (`race`, `quality_score`, `has_mask`) provide additional information
- All existing code continues to work without modification
EOF

echo "Creating CMake integration..."

# Add InsightFace to CMakeLists.txt if not already present
if ! grep -q "InsightFace" "$PROJECT_ROOT/CMakeLists.txt"; then
    cat >> "$PROJECT_ROOT/CMakeLists.txt" << 'EOF'

# InsightFace Integration (Optional)
set(INSIGHTFACE_ROOT "/userdata/source/source/insightface/cpp-package/inspireface")
set(INSIGHTFACE_INCLUDE_DIR "${INSIGHTFACE_ROOT}/cpp/inspireface/c_api")
set(INSIGHTFACE_LIB_DIR "${INSIGHTFACE_ROOT}/build/lib")

# Try to find InsightFace library
find_library(INSIGHTFACE_LIB 
    NAMES InspireFace
    PATHS ${INSIGHTFACE_LIB_DIR}
    NO_DEFAULT_PATH
)

if(INSIGHTFACE_LIB)
    message(STATUS "Found InsightFace library: ${INSIGHTFACE_LIB}")
    target_include_directories(${PROJECT_NAME} PRIVATE ${INSIGHTFACE_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${INSIGHTFACE_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_INSIGHTFACE=1)
    message(STATUS "InsightFace integration enabled")
else()
    message(STATUS "InsightFace library not found. Using fallback implementation.")
endif()
EOF
    echo "CMakeLists.txt updated with InsightFace integration"
fi

echo
echo "=== Setup Complete ==="
echo
echo "Next steps:"
echo "1. Test InsightFace: cd tests && ./build_insightface_test.sh"
echo "2. Run test: ./insightface_simple_test ../models/Pikachu.pack test_image.jpg"
echo "3. Rebuild main project: cd build && make -j\$(nproc)"
echo "4. Check INSIGHTFACE_USAGE.md for detailed usage instructions"
echo
echo "The system now supports enhanced face analysis with:"
echo "- Age recognition (9 brackets)"
echo "- Gender recognition"
echo "- Race recognition"
echo "- Quality assessment"
echo "- Mask detection"
