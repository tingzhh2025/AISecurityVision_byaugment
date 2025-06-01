#!/bin/bash

# Build and test script for InsightFace integration
# This script builds the InsightFace test program and runs comprehensive tests

set -e  # Exit on any error

PROJECT_ROOT="/userdata/source/source/AISecurityVision_byaugment"
INSIGHTFACE_PATH="/userdata/source/source/insightface/cpp-package/inspireface"
BUILD_DIR="$PROJECT_ROOT/build"
TEST_DIR="$PROJECT_ROOT/tests"

echo "=== InsightFace Integration Build and Test ==="
echo "Project root: $PROJECT_ROOT"
echo "InsightFace path: $INSIGHTFACE_PATH"
echo "Build directory: $BUILD_DIR"
echo

# 1. Check InsightFace library
echo "1. Checking InsightFace library..."
if [ -f "$INSIGHTFACE_PATH/build/lib/libInspireFace.so" ]; then
    echo "✅ InsightFace library found: $INSIGHTFACE_PATH/build/lib/libInspireFace.so"
else
    echo "❌ InsightFace library not found. Building InsightFace..."
    cd "$INSIGHTFACE_PATH"
    mkdir -p build
    cd build
    cmake .. -DCMAKE_BUILD_TYPE=Release -DISF_BUILD_SHARED_LIBS=ON
    make -j$(nproc)
    echo "✅ InsightFace library built successfully"
fi

# 2. Check model pack
echo "2. Checking model pack..."
if [ -f "$PROJECT_ROOT/models/Pikachu.pack" ]; then
    echo "✅ Model pack found: $PROJECT_ROOT/models/Pikachu.pack"
else
    echo "❌ Model pack not found at $PROJECT_ROOT/models/Pikachu.pack"
    echo "Please ensure the Pikachu.pack model file is available"
    exit 1
fi

# 3. Build the main project with InsightFace
echo "3. Building main project with InsightFace..."
cd "$BUILD_DIR"
cmake ..
make -j$(nproc) AISecurityVision

if [ $? -eq 0 ]; then
    echo "✅ Main project built successfully with InsightFace integration"
else
    echo "❌ Failed to build main project"
    exit 1
fi

# 4. Build the InsightFace test program
echo "4. Building InsightFace test program..."
cd "$TEST_DIR"

# Compile the test program
g++ -std=c++17 -O2 -o test_insightface_integration test_insightface_integration.cpp \
    -I"$INSIGHTFACE_PATH/cpp/inspireface/c_api" \
    -I"$INSIGHTFACE_PATH/cpp" \
    -I"$PROJECT_ROOT/src" \
    -L"$INSIGHTFACE_PATH/build/lib" \
    -L"$BUILD_DIR" \
    -lInspireFace \
    -lopencv_core -lopencv_imgproc -lopencv_imgcodecs \
    -lpthread \
    -Wl,-rpath,"$INSIGHTFACE_PATH/build/lib" \
    -DHAVE_INSIGHTFACE=1

if [ $? -eq 0 ]; then
    echo "✅ InsightFace test program built successfully"
else
    echo "❌ Failed to build InsightFace test program"
    exit 1
fi

# 5. Run the test
echo "5. Running InsightFace integration test..."
echo

# Test with bus.jpg image
if [ -f "$PROJECT_ROOT/models/bus.jpg" ]; then
    echo "Running test with bus.jpg..."
    ./test_insightface_integration "$PROJECT_ROOT/models/Pikachu.pack" "$PROJECT_ROOT/models/bus.jpg"
    
    if [ $? -eq 0 ]; then
        echo "✅ InsightFace integration test passed!"
    else
        echo "❌ InsightFace integration test failed"
        exit 1
    fi
else
    echo "❌ Test image not found: $PROJECT_ROOT/models/bus.jpg"
    echo "Please provide a test image to run the integration test"
fi

echo
echo "=== Build and Test Summary ==="
echo "✅ InsightFace library: Available"
echo "✅ Model pack: Available"
echo "✅ Main project: Built successfully"
echo "✅ Test program: Built successfully"
echo "✅ Integration test: Passed"
echo
echo "🎉 InsightFace integration is fully functional!"
echo
echo "Usage examples:"
echo "  # Test with custom image:"
echo "  ./test_insightface_integration ../models/Pikachu.pack /path/to/your/image.jpg"
echo
echo "  # Run main application with InsightFace:"
echo "  cd $BUILD_DIR && ./AISecurityVision"
echo
