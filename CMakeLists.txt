cmake_minimum_required(VERSION 3.18)
project(AISecurityVision VERSION 1.0.0 LANGUAGES CXX CUDA)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set CUDA standard
set(CMAKE_CUDA_STANDARD 17)
set(CMAKE_CUDA_STANDARD_REQUIRED ON)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(PkgConfig REQUIRED)
find_package(OpenCV REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(CUDA REQUIRED)

# Find TensorRT
find_path(TENSORRT_INCLUDE_DIR NvInfer.h
    HINTS ${TENSORRT_ROOT} ${CUDA_TOOLKIT_ROOT_DIR}
    PATH_SUFFIXES include)
find_library(TENSORRT_LIBRARY_INFER nvinfer
    HINTS ${TENSORRT_ROOT} ${CUDA_TOOLKIT_ROOT_DIR}
    PATH_SUFFIXES lib lib64 lib/x64)
find_library(TENSORRT_LIBRARY_INFER_PLUGIN nvinfer_plugin
    HINTS ${TENSORRT_ROOT} ${CUDA_TOOLKIT_ROOT_DIR}
    PATH_SUFFIXES lib lib64 lib/x64)

# Find FFmpeg
pkg_check_modules(LIBAV REQUIRED IMPORTED_TARGET
    libavdevice
    libavformat
    libavfilter
    libavcodec
    libswresample
    libswscale
    libavutil)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${OpenCV_INCLUDE_DIRS})
include_directories(${TENSORRT_INCLUDE_DIR})
include_directories(${CUDA_INCLUDE_DIRS})

# Source files
file(GLOB_RECURSE SOURCES 
    "src/core/*.cpp"
    "src/video/*.cpp"
    "src/ai/*.cpp"
    "src/recognition/*.cpp"
    "src/output/*.cpp"
    "src/api/*.cpp"
    "src/database/*.cpp"
)

# CUDA source files
file(GLOB_RECURSE CUDA_SOURCES 
    "src/ai/*.cu"
    "src/video/*.cu"
)

# Create executable
add_executable(${PROJECT_NAME} 
    src/main.cpp
    ${SOURCES}
    ${CUDA_SOURCES}
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${OpenCV_LIBS}
    ${TENSORRT_LIBRARY_INFER}
    ${TENSORRT_LIBRARY_INFER_PLUGIN}
    ${CUDA_LIBRARIES}
    ${CUDA_CUBLAS_LIBRARIES}
    ${CUDA_curand_LIBRARY}
    ${CUDA_cusparse_LIBRARY}
    SQLite::SQLite3
    PkgConfig::LIBAV
    pthread
    dl
)

# Compiler-specific options
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# CUDA specific settings
set_property(TARGET ${PROJECT_NAME} PROPERTY CUDA_SEPARABLE_COMPILATION ON)

# Install targets
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY models/ DESTINATION share/${PROJECT_NAME}/models)
install(DIRECTORY config/ DESTINATION share/${PROJECT_NAME}/config)

# Testing
enable_testing()
add_subdirectory(tests)

# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "CUDA standard: ${CMAKE_CUDA_STANDARD}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "TensorRT include: ${TENSORRT_INCLUDE_DIR}")
message(STATUS "TensorRT library: ${TENSORRT_LIBRARY_INFER}")
