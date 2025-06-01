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
