#!/bin/bash

# InsightFace Installation Script for AI Security Vision System
# This script installs InsightFace library and models to third_party directory

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
INSIGHTFACE_SOURCE="/userdata/source/source/insightface/cpp-package/inspireface"
THIRD_PARTY_DIR="${PROJECT_ROOT}/third_party/insightface"

echo "=== InsightFace Installation Script ==="
echo "Project root: $PROJECT_ROOT"
echo "InsightFace source: $INSIGHTFACE_SOURCE"
echo "Installation target: $THIRD_PARTY_DIR"
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

# Check if InsightFace source exists
if [ ! -d "$INSIGHTFACE_SOURCE" ]; then
    print_error "InsightFace source not found at $INSIGHTFACE_SOURCE"
    print_error "Please ensure InsightFace is available at the expected location"
    exit 1
fi

print_status "Found InsightFace source directory"

# Check if InsightFace is built
INSIGHTFACE_LIB="$INSIGHTFACE_SOURCE/build/lib/libInspireFace.so"
if [ ! -f "$INSIGHTFACE_LIB" ]; then
    print_warning "InsightFace library not found. Building InsightFace..."
    
    cd "$INSIGHTFACE_SOURCE"
    
    # Clean previous build
    if [ -d "build" ]; then
        rm -rf build
    fi
    
    # Create build directory
    mkdir -p build
    cd build
    
    # Configure with CMake
    print_status "Configuring InsightFace build..."
    cmake .. \
        -DCMAKE_BUILD_TYPE=Release \
        -DISF_BUILD_SHARED_LIBS=ON \
        -DISF_BUILD_WITH_TEST=OFF \
        -DISF_BUILD_WITH_SAMPLE=OFF
    
    # Build
    print_status "Building InsightFace (this may take several minutes)..."
    make -j$(nproc)
    
    # Verify build
    if [ ! -f "lib/libInspireFace.so" ]; then
        print_error "Failed to build InsightFace library"
        exit 1
    fi
    
    print_status "InsightFace built successfully"
    cd "$PROJECT_ROOT"
else
    print_status "InsightFace library already built"
fi

# Create third_party directory structure
print_status "Creating third_party directory structure..."
mkdir -p "$THIRD_PARTY_DIR"/{lib,include,models}

# Copy library files
print_status "Copying InsightFace library..."
cp "$INSIGHTFACE_SOURCE/build/lib/libInspireFace.so" "$THIRD_PARTY_DIR/lib/"

# Copy header files
print_status "Copying InsightFace headers..."
cp -r "$INSIGHTFACE_SOURCE/cpp/inspireface/c_api"/* "$THIRD_PARTY_DIR/include/"

# Copy model file if it exists in models directory
if [ -f "$PROJECT_ROOT/models/Pikachu.pack" ]; then
    print_status "Copying Pikachu.pack model..."
    cp "$PROJECT_ROOT/models/Pikachu.pack" "$THIRD_PARTY_DIR/models/"
else
    print_warning "Pikachu.pack model not found in models directory"
    print_warning "Please ensure the model file is available for InsightFace to work"
fi

# Verify installation
print_status "Verifying installation..."

# Check library
if [ ! -f "$THIRD_PARTY_DIR/lib/libInspireFace.so" ]; then
    print_error "Library installation failed"
    exit 1
fi

# Check headers
if [ ! -f "$THIRD_PARTY_DIR/include/inspireface.h" ]; then
    print_error "Header installation failed"
    exit 1
fi

# Check model
if [ ! -f "$THIRD_PARTY_DIR/models/Pikachu.pack" ]; then
    print_warning "Model file not installed"
fi

# Display installation summary
echo
echo "=== Installation Summary ==="
print_status "InsightFace successfully installed to third_party directory"
echo "  Library: $THIRD_PARTY_DIR/lib/libInspireFace.so"
echo "  Headers: $THIRD_PARTY_DIR/include/"
echo "  Models:  $THIRD_PARTY_DIR/models/"
echo

# Check library dependencies
print_status "Checking library dependencies..."
ldd "$THIRD_PARTY_DIR/lib/libInspireFace.so" | head -10

echo
echo "=== Next Steps ==="
echo "1. Build the project with: mkdir build && cd build && cmake .. && make"
echo "2. The CMakeLists.txt has been updated to use the third_party InsightFace"
echo "3. Test the integration with: ./build/AISecurityVision"
echo

print_status "Installation completed successfully!"
