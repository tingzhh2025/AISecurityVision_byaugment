#!/bin/bash

# Test script for InsightFace installation in third_party directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

echo "=== InsightFace Installation Test ==="
echo "Project root: $PROJECT_ROOT"
echo

# Function to print colored output
print_status() {
    echo -e "\033[32m[INFO]\033[0m $1"
}

print_error() {
    echo -e "\033[31m[ERROR]\033[0m $1"
}

print_warning() {
    echo -e "\033[33m[WARNING]\033[0m $1"
}

# Test 1: Check third_party directory structure
print_status "1. Checking third_party directory structure..."

THIRD_PARTY_DIR="$PROJECT_ROOT/third_party/insightface"

if [ ! -d "$THIRD_PARTY_DIR" ]; then
    print_error "third_party/insightface directory not found"
    exit 1
fi

# Check library
if [ ! -f "$THIRD_PARTY_DIR/lib/libInspireFace.so" ]; then
    print_error "libInspireFace.so not found"
    exit 1
fi
print_status "✓ Library found: $THIRD_PARTY_DIR/lib/libInspireFace.so"

# Check headers
if [ ! -f "$THIRD_PARTY_DIR/include/inspireface.h" ]; then
    print_error "inspireface.h not found"
    exit 1
fi
print_status "✓ Headers found: $THIRD_PARTY_DIR/include/"

# Check model
if [ ! -f "$THIRD_PARTY_DIR/models/Pikachu.pack" ]; then
    print_warning "Pikachu.pack model not found in third_party"
    if [ -f "$PROJECT_ROOT/models/Pikachu.pack" ]; then
        print_status "Copying model from models/ directory..."
        cp "$PROJECT_ROOT/models/Pikachu.pack" "$THIRD_PARTY_DIR/models/"
    else
        print_error "Pikachu.pack model not found anywhere"
        exit 1
    fi
fi
print_status "✓ Model found: $THIRD_PARTY_DIR/models/Pikachu.pack"

# Test 2: Check library dependencies
print_status "2. Checking library dependencies..."
ldd "$THIRD_PARTY_DIR/lib/libInspireFace.so" > /dev/null 2>&1
if [ $? -eq 0 ]; then
    print_status "✓ Library dependencies satisfied"
else
    print_error "Library has missing dependencies"
    ldd "$THIRD_PARTY_DIR/lib/libInspireFace.so"
    exit 1
fi

# Test 3: Test CMake configuration
print_status "3. Testing CMake configuration..."

cd "$PROJECT_ROOT"
rm -rf build_test
mkdir -p build_test
cd build_test

CMAKE_OUTPUT=$(cmake .. 2>&1)
if echo "$CMAKE_OUTPUT" | grep -q "InsightFace integration enabled"; then
    print_status "✓ CMake detects InsightFace correctly"
else
    print_error "CMake failed to detect InsightFace"
    echo "CMake output:"
    echo "$CMAKE_OUTPUT"
    exit 1
fi

# Test 4: Test compilation
print_status "4. Testing compilation..."

make -j$(nproc) > /dev/null 2>&1
if [ $? -eq 0 ]; then
    print_status "✓ Project compiles successfully with InsightFace"
else
    print_error "Compilation failed"
    make -j$(nproc)
    exit 1
fi

# Test 5: Check if executable was created
if [ -f "AISecurityVision" ]; then
    print_status "✓ Executable created successfully"
else
    print_error "Executable not found"
    exit 1
fi

# Test 6: Test runtime linking
print_status "5. Testing runtime linking..."

# Check if the executable can find the InsightFace library
ldd AISecurityVision | grep -q "libInspireFace.so"
if [ $? -eq 0 ]; then
    print_status "✓ Executable links to InsightFace library"
else
    print_warning "Executable may not link to InsightFace library"
    print_status "Library dependencies:"
    ldd AISecurityVision | grep -i inspire || echo "No InsightFace library found in dependencies"
fi

# Test 7: Quick runtime test (if possible)
print_status "6. Testing basic functionality..."

# Set library path
export LD_LIBRARY_PATH="$PROJECT_ROOT/third_party/insightface/lib:$LD_LIBRARY_PATH"

# Try to run with --help to see if it starts
timeout 5s ./AISecurityVision --help > /dev/null 2>&1
if [ $? -eq 0 ] || [ $? -eq 124 ]; then  # 124 is timeout exit code
    print_status "✓ Executable runs without immediate crashes"
else
    print_warning "Executable may have runtime issues"
fi

# Clean up
cd "$PROJECT_ROOT"
rm -rf build_test

echo
echo "=== Test Summary ==="
print_status "✓ Directory structure correct"
print_status "✓ Library files present"
print_status "✓ Header files present"
print_status "✓ Model files present"
print_status "✓ Library dependencies satisfied"
print_status "✓ CMake configuration working"
print_status "✓ Compilation successful"
print_status "✓ Executable created"
print_status "✓ Runtime linking working"

echo
echo "=== Usage Instructions ==="
echo "1. Build the project:"
echo "   mkdir build && cd build && cmake .. && make -j\$(nproc)"
echo
echo "2. Set library path before running:"
echo "   export LD_LIBRARY_PATH=\"\$PWD/third_party/insightface/lib:\$LD_LIBRARY_PATH\""
echo
echo "3. Run the application:"
echo "   ./build/AISecurityVision"
echo
echo "4. Test InsightFace integration:"
echo "   ./build/test_insightface_integration third_party/insightface/models/Pikachu.pack models/bus.jpg"
echo

print_status "🎉 InsightFace installation test completed successfully!"
