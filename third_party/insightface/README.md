# InsightFace Integration

This directory contains the InsightFace library and models for the AI Security Vision System.

## Directory Structure

```
third_party/insightface/
├── lib/
│   └── libInspireFace.so          # InsightFace shared library
├── include/
│   ├── inspireface.h              # Main C API header
│   ├── inspireface.cc             # C API implementation
│   ├── inspireface_internal.h     # Internal definitions
│   └── intypedef.h                # Type definitions
├── models/
│   └── Pikachu.pack               # InsightFace model pack
└── README.md                      # This file
```

## Features

The InsightFace integration provides comprehensive face analysis capabilities:

- **Face Detection**: Automatic face detection in images
- **Age Recognition**: 9 age brackets (0-2, 3-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60-69, 70+)
- **Gender Recognition**: Male/Female classification
- **Race Recognition**: Black, Asian, Latino/Hispanic, Middle Eastern, White
- **Quality Assessment**: Face quality scoring for filtering
- **Mask Detection**: Automatic face mask detection

## Model Pack

The `Pikachu.pack` model contains all necessary models for face analysis:
- Face detection models
- Age/gender recognition models  
- Quality assessment models
- Mask detection models

## Installation

The InsightFace library was installed using the automated script:

```bash
./scripts/install_insightface.sh
```

This script:
1. Builds InsightFace from source if needed
2. Copies library files to `third_party/insightface/lib/`
3. Copies header files to `third_party/insightface/include/`
4. Copies model files to `third_party/insightface/models/`
5. Updates CMakeLists.txt configuration

## CMake Integration

The CMakeLists.txt automatically detects and links InsightFace:

```cmake
# InsightFace Integration (Optional)
set(INSIGHTFACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/insightface")
set(INSIGHTFACE_INCLUDE_DIR "${INSIGHTFACE_ROOT}/include")
set(INSIGHTFACE_LIB_DIR "${INSIGHTFACE_ROOT}/lib")

find_library(INSIGHTFACE_LIB NAMES InspireFace PATHS ${INSIGHTFACE_LIB_DIR})

if(INSIGHTFACE_LIB)
    target_include_directories(${PROJECT_NAME} PRIVATE ${INSIGHTFACE_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${INSIGHTFACE_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_INSIGHTFACE=1)
endif()
```

## Usage in Code

```cpp
#include "AgeGenderAnalyzer.h"

// Initialize with InsightFace
AgeGenderAnalyzer analyzer;
analyzer.initialize("third_party/insightface/models/Pikachu.pack");

// Analyze person crops
std::vector<PersonDetection> persons = PersonFilter::filterPersons(detections, frame);
auto attributes = analyzer.analyze(persons);

for (const auto& attr : attributes) {
    std::cout << "Age: " << attr.age << ", Gender: " << attr.gender 
              << ", Race: " << attr.race << std::endl;
}
```

## Performance

- **Inference Time**: ~15-35ms per person
- **Hardware**: CPU inference (RK3588)
- **Memory**: ~200MB model loading
- **Accuracy**: High accuracy for age/gender/race recognition

## Dependencies

The InsightFace library depends on:
- OpenCV (for image processing)
- MNN (for neural network inference)
- Standard C++ libraries

All dependencies are statically linked into libInspireFace.so.

## Troubleshooting

### Library Not Found
If CMake cannot find the library:
```bash
# Verify library exists
ls -la third_party/insightface/lib/libInspireFace.so

# Check dependencies
ldd third_party/insightface/lib/libInspireFace.so
```

### Model Loading Issues
If model loading fails:
```bash
# Verify model file
ls -la third_party/insightface/models/Pikachu.pack
file third_party/insightface/models/Pikachu.pack
```

### Build Issues
If compilation fails:
```bash
# Reinstall InsightFace
./scripts/install_insightface.sh

# Clean and rebuild
rm -rf build && mkdir build && cd build && cmake .. && make
```

## Source

This installation is based on InsightFace from:
- Source: `/userdata/source/source/insightface/cpp-package/inspireface`
- Version: Latest from local repository
- Build: Release configuration with shared libraries
