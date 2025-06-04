cmake_minimum_required(VERSION 3.16)
project(AISecurityVision VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find required packages
find_package(PkgConfig REQUIRED)

# Force OpenCV 4.5.5 to avoid version conflicts
set(OpenCV_DIR "/usr/lib/aarch64-linux-gnu/cmake/opencv4")
find_package(OpenCV 4.5.5 EXACT REQUIRED)

# Verify we're using the correct OpenCV version
if(OpenCV_VERSION VERSION_LESS "4.5.5")
    message(FATAL_ERROR "OpenCV 4.5.5 is required, found ${OpenCV_VERSION}")
endif()

# Force specific OpenCV library paths to ensure 4.5.5 is used
set(OpenCV_LIBS_OVERRIDE
    /usr/lib/aarch64-linux-gnu/libopencv_core.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_imgproc.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_imgcodecs.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_videoio.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_video.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_calib3d.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_features2d.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_flann.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_highgui.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_photo.so.4.5.5
    /usr/lib/aarch64-linux-gnu/libopencv_stitching.so.4.5.5
)

# Check if all required 4.5.5 libraries exist
foreach(lib ${OpenCV_LIBS_OVERRIDE})
    if(NOT EXISTS ${lib})
        message(WARNING "OpenCV 4.5.5 library not found: ${lib}")
    endif()
endforeach()

# Override OpenCV_LIBS with our specific 4.5.5 libraries
set(OpenCV_LIBS ${OpenCV_LIBS_OVERRIDE})

# Find or download httplib
find_path(HTTPLIB_INCLUDE_DIR httplib.h
    PATHS
    /usr/include
    /usr/local/include
    ${CMAKE_SOURCE_DIR}/third_party/httplib
)

if(NOT HTTPLIB_INCLUDE_DIR)
    message(STATUS "httplib not found, downloading...")
    file(DOWNLOAD
        "https://raw.githubusercontent.com/yhirose/cpp-httplib/v0.14.1/httplib.h"
        "${CMAKE_SOURCE_DIR}/third_party/httplib/httplib.h"
        SHOW_PROGRESS
    )
    set(HTTPLIB_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/httplib")
endif()

if(HTTPLIB_INCLUDE_DIR)
    include_directories(${HTTPLIB_INCLUDE_DIR})
    message(STATUS "httplib found at: ${HTTPLIB_INCLUDE_DIR}")
endif()

# Try to find SQLite3
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SQLITE3 sqlite3)
endif()

if(NOT SQLITE3_FOUND)
    find_library(SQLITE3_LIBRARIES sqlite3)
    if(SQLITE3_LIBRARIES)
        set(SQLITE3_FOUND TRUE)
    endif()
endif()

# Find FFmpeg (optional for now)
pkg_check_modules(LIBAV
    libavformat
    libavcodec
    libswscale
    libavutil)

# Find NVIDIA Management Library (NVML) - optional
find_library(NVML_LIBRARIES nvidia-ml
    PATHS
    /usr/lib/x86_64-linux-gnu
    /usr/lib64
    /usr/local/cuda/lib64
    /usr/local/cuda/lib
    /opt/cuda/lib64
    /opt/cuda/lib
)

if(NVML_LIBRARIES)
    set(NVML_FOUND TRUE)
    # Try to find the header
    find_path(NVML_INCLUDE_DIRS nvml.h
        PATHS
        /usr/include
        /usr/local/cuda/include
        /opt/cuda/include
    )
endif()

# Find RKNN libraries for Rockchip NPU support
find_library(RKNN_API_LIBRARY rknn_api
    PATHS
    /usr/lib
    /usr/local/lib
    /usr/lib/aarch64-linux-gnu
    /usr/lib/arm-linux-gnueabihf
    /opt/rknn/lib
    /opt/rknn/lib64
    ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu2/Linux/aarch64
    ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu1/Linux/aarch64
)

# Also try to find librknnrt.so (RKNPU2 runtime library)
if(NOT RKNN_API_LIBRARY)
    find_library(RKNN_API_LIBRARY rknnrt
        PATHS
        /usr/lib
        /usr/local/lib
        /usr/lib/aarch64-linux-gnu
        /usr/lib/arm-linux-gnueabihf
        /opt/rknn/lib
        /opt/rknn/lib64
        ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu2/Linux/aarch64
        ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu1/Linux/aarch64
    )
endif()

find_path(RKNN_API_INCLUDE_DIR rknn_api.h
    PATHS
    /usr/include
    /usr/local/include
    /opt/rknn/include
    ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu2/include
    ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu1/include
)

if(RKNN_API_LIBRARY AND RKNN_API_INCLUDE_DIR)
    set(RKNN_FOUND TRUE)
    set(RKNN_LIBRARIES ${RKNN_API_LIBRARY})
    set(RKNN_INCLUDE_DIRS ${RKNN_API_INCLUDE_DIR})
    message(STATUS "Found RKNN API: ${RKNN_API_LIBRARY}")
else()
    set(RKNN_FOUND FALSE)
    message(STATUS "RKNN API not found - NPU acceleration will not be available")
endif()

# Find TensorRT libraries for NVIDIA GPU support (optional)
find_library(TENSORRT_LIBRARY nvinfer
    PATHS
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
    /usr/local/lib
    /usr/local/cuda/lib64
    /opt/tensorrt/lib
    /usr/lib/tensorrt
)

find_library(TENSORRT_ONNX_LIBRARY nvonnxparser
    PATHS
    /usr/lib/x86_64-linux-gnu
    /usr/lib/aarch64-linux-gnu
    /usr/local/lib
    /usr/local/cuda/lib64
    /opt/tensorrt/lib
    /usr/lib/tensorrt
)

find_path(TENSORRT_INCLUDE_DIR NvInfer.h
    PATHS
    /usr/include
    /usr/local/include
    /usr/local/cuda/include
    /opt/tensorrt/include
    /usr/include/tensorrt
)

if(TENSORRT_LIBRARY AND TENSORRT_ONNX_LIBRARY AND TENSORRT_INCLUDE_DIR)
    set(TENSORRT_FOUND TRUE)
    set(TENSORRT_LIBRARIES ${TENSORRT_LIBRARY} ${TENSORRT_ONNX_LIBRARY})
    set(TENSORRT_INCLUDE_DIRS ${TENSORRT_INCLUDE_DIR})
    message(STATUS "Found TensorRT: ${TENSORRT_LIBRARIES}")

    # Also find CUDA runtime
    find_package(CUDA QUIET)
    if(CUDA_FOUND)
        list(APPEND TENSORRT_LIBRARIES ${CUDA_LIBRARIES})
        list(APPEND TENSORRT_INCLUDE_DIRS ${CUDA_INCLUDE_DIRS})
        message(STATUS "Found CUDA: ${CUDA_LIBRARIES}")
    endif()
else()
    set(TENSORRT_FOUND FALSE)
    message(STATUS "TensorRT not found - GPU acceleration will not be available")
endif()

# Find libcurl for HTTP alarm delivery
find_package(CURL REQUIRED)
if(CURL_FOUND)
    set(CURL_FOUND TRUE)
    message(STATUS "libcurl found: ${CURL_LIBRARIES}")
else()
    message(FATAL_ERROR "libcurl is required for HTTP alarm delivery")
endif()

# Find or download websocketpp for WebSocket alarm delivery
find_path(WEBSOCKETPP_INCLUDE_DIR websocketpp/config/asio_no_tls.hpp
    PATHS
    /usr/include
    /usr/local/include
    ${CMAKE_SOURCE_DIR}/third_party/websocketpp
)

if(NOT WEBSOCKETPP_INCLUDE_DIR)
    message(STATUS "websocketpp not found, downloading...")

    # Create directory
    file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/websocketpp")

    # Download websocketpp headers
    file(DOWNLOAD
        "https://github.com/zaphoyd/websocketpp/archive/refs/tags/0.8.2.tar.gz"
        "${CMAKE_SOURCE_DIR}/third_party/websocketpp-0.8.2.tar.gz"
        SHOW_PROGRESS
    )

    # Extract the archive
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_SOURCE_DIR}/third_party/websocketpp-0.8.2.tar.gz"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/"
    )

    # Move headers to expected location
    execute_process(
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/third_party/websocketpp-0.8.2/websocketpp"
        "${CMAKE_SOURCE_DIR}/third_party/websocketpp/websocketpp"
    )

    set(WEBSOCKETPP_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/websocketpp")
endif()

if(WEBSOCKETPP_INCLUDE_DIR)
    include_directories(${WEBSOCKETPP_INCLUDE_DIR})
    message(STATUS "websocketpp found at: ${WEBSOCKETPP_INCLUDE_DIR}")

    # Find Boost (required by websocketpp)
    find_package(Boost COMPONENTS system thread)
    if(NOT Boost_FOUND)
        message(STATUS "Boost not found, using standalone ASIO")
        # Download standalone ASIO if Boost is not available
        find_path(ASIO_INCLUDE_DIR asio.hpp
            PATHS
            /usr/include
            /usr/local/include
            ${CMAKE_SOURCE_DIR}/third_party/asio
        )

        if(NOT ASIO_INCLUDE_DIR)
            message(STATUS "ASIO not found, downloading...")
            file(DOWNLOAD
                "https://github.com/chriskohlhoff/asio/archive/refs/tags/asio-1-24-0.tar.gz"
                "${CMAKE_SOURCE_DIR}/third_party/asio-1-24-0.tar.gz"
                SHOW_PROGRESS
            )

            execute_process(
                COMMAND ${CMAKE_COMMAND} -E tar xzf "${CMAKE_SOURCE_DIR}/third_party/asio-1-24-0.tar.gz"
                WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/"
            )

            execute_process(
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/third_party/asio-asio-1-24-0/asio/include"
                "${CMAKE_SOURCE_DIR}/third_party/asio"
            )

            set(ASIO_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/asio")
        endif()

        if(ASIO_INCLUDE_DIR)
            include_directories(${ASIO_INCLUDE_DIR})
            add_definitions(-DASIO_STANDALONE)
            message(STATUS "Standalone ASIO found at: ${ASIO_INCLUDE_DIR}")
        endif()
    endif()
endif()

# Find or install Eclipse Paho MQTT C++ client for MQTT alarm delivery
find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(PAHO_MQTT_CPP QUIET paho-mqttpp3)
endif()

if(NOT PAHO_MQTT_CPP_FOUND)
    message(STATUS "Paho MQTT C++ not found via pkg-config, checking for manual installation...")

    find_path(PAHO_MQTT_CPP_INCLUDE_DIR mqtt/async_client.h
        PATHS
        /usr/include
        /usr/local/include
        ${CMAKE_SOURCE_DIR}/third_party/paho.mqtt.cpp/src
    )

    find_library(PAHO_MQTT_CPP_LIBRARY
        NAMES paho-mqttpp3 libpaho-mqttpp3
        PATHS
        /usr/lib
        /usr/local/lib
        ${CMAKE_SOURCE_DIR}/third_party/paho.mqtt.cpp/lib
    )

    find_library(PAHO_MQTT_C_LIBRARY
        NAMES paho-mqtt3as libpaho-mqtt3as
        PATHS
        /usr/lib
        /usr/local/lib
        ${CMAKE_SOURCE_DIR}/third_party/paho.mqtt.c/lib
    )

    if(PAHO_MQTT_CPP_INCLUDE_DIR AND PAHO_MQTT_CPP_LIBRARY AND PAHO_MQTT_C_LIBRARY)
        set(PAHO_MQTT_CPP_FOUND TRUE)
        set(PAHO_MQTT_CPP_INCLUDE_DIRS ${PAHO_MQTT_CPP_INCLUDE_DIR})
        set(PAHO_MQTT_CPP_LIBRARIES ${PAHO_MQTT_CPP_LIBRARY} ${PAHO_MQTT_C_LIBRARY})
        message(STATUS "Found Paho MQTT C++: ${PAHO_MQTT_CPP_LIBRARIES}")
    else()
        message(STATUS "Paho MQTT C++ not found, will use simple MQTT client")

        # Create a simple MQTT client interface for basic functionality
        file(MAKE_DIRECTORY "${CMAKE_SOURCE_DIR}/third_party/mqtt")

        # Create a minimal MQTT client header (will be implemented in source)
        set(PAHO_MQTT_CPP_INCLUDE_DIRS "${CMAKE_SOURCE_DIR}/third_party/mqtt")
        set(PAHO_MQTT_CPP_FOUND TRUE)
        set(USE_SIMPLE_MQTT TRUE)
        add_definitions(-DUSE_SIMPLE_MQTT)
    endif()
endif()

if(PAHO_MQTT_CPP_FOUND)
    include_directories(${PAHO_MQTT_CPP_INCLUDE_DIRS})
    message(STATUS "MQTT support enabled")
    add_definitions(-DHAVE_MQTT)
endif()

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/third_party)
include_directories(${OpenCV_INCLUDE_DIRS})

# Source files
file(GLOB_RECURSE SOURCES
    "src/core/*.cpp"
    "src/video/*.cpp"
    "src/ai/*.cpp"
    "src/recognition/*.cpp"
    "src/output/*.cpp"
    "src/api/*.cpp"
    "src/database/*.cpp"
    "src/utils/*.cpp"
    "src/onvif/*.cpp"
    "src/network/*.cpp"
)

# Remove LoggerExample.cpp from sources as it contains unimplemented features
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/core/LoggerExample.cpp")

# Remove YOLOv8DetectorExample.cpp from sources as it contains a main function
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/ai/YOLOv8DetectorExample.cpp")

# AI-only sources for simple tests
file(GLOB AI_SOURCES
    "src/ai/YOLOv8Detector.cpp"
    "src/core/TaskManager.cpp"
)

# Add MQTT client source if using simple MQTT
if(USE_SIMPLE_MQTT)
    list(APPEND SOURCES "third_party/mqtt/simple_mqtt.cpp")
    message(STATUS "Adding simple MQTT client source")
endif()

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${SOURCES}
)

# Ensure we create an executable, not a shared library
set_target_properties(${PROJECT_NAME} PROPERTIES
    POSITION_INDEPENDENT_CODE OFF
    ENABLE_EXPORTS OFF
)

# Link libraries
set(LINK_LIBRARIES ${OpenCV_LIBS} pthread dl)

# Skip OpenCV DNN library completely due to protobuf compatibility issues
# We use RKNN NPU for AI inference instead, which provides better performance
message(STATUS "Skipping OpenCV DNN library - using RKNN NPU for AI inference")
message(STATUS "Note: OpenCV DNN disabled to avoid protobuf version conflicts")
add_definitions(-DDISABLE_OPENCV_DNN)

# Add SQLite3 if found
if(SQLITE3_FOUND)
    list(APPEND LINK_LIBRARIES ${SQLITE3_LIBRARIES})
    if(SQLITE3_INCLUDE_DIRS)
        include_directories(${SQLITE3_INCLUDE_DIRS})
    endif()
endif()

# Add FFmpeg if found
if(LIBAV_FOUND)
    list(APPEND LINK_LIBRARIES ${LIBAV_LIBRARIES})
    include_directories(${LIBAV_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_FFMPEG)
endif()

# Add NVML if found
if(NVML_FOUND)
    list(APPEND LINK_LIBRARIES ${NVML_LIBRARIES})
    if(NVML_INCLUDE_DIRS)
        include_directories(${NVML_INCLUDE_DIRS})
    endif()
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_NVML)
endif()

# Add RKNN if found
if(RKNN_FOUND)
    list(APPEND LINK_LIBRARIES ${RKNN_LIBRARIES})
    include_directories(${RKNN_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_RKNN)
endif()

# Add TensorRT if found
if(TENSORRT_FOUND)
    list(APPEND LINK_LIBRARIES ${TENSORRT_LIBRARIES})
    include_directories(${TENSORRT_INCLUDE_DIRS})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_TENSORRT)
endif()

# Add libcurl
if(CURL_FOUND)
    list(APPEND LINK_LIBRARIES ${CURL_LIBRARIES})
    include_directories(${CURL_INCLUDE_DIRS})
endif()

# Add WebSocket dependencies
if(WEBSOCKETPP_INCLUDE_DIR)
    if(Boost_FOUND)
        list(APPEND LINK_LIBRARIES ${Boost_LIBRARIES})
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_WEBSOCKETPP)
    else()
        # Using standalone ASIO, no additional libraries needed
        target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_WEBSOCKETPP ASIO_STANDALONE)
    endif()
endif()

# Add MQTT dependencies
if(PAHO_MQTT_CPP_FOUND AND PAHO_MQTT_CPP_LIBRARIES)
    list(APPEND LINK_LIBRARIES ${PAHO_MQTT_CPP_LIBRARIES})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_PAHO_MQTT)
endif()

target_link_libraries(${PROJECT_NAME} ${LINK_LIBRARIES})

# Compiler-specific options
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Install targets
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY models/ DESTINATION share/${PROJECT_NAME}/models OPTIONAL)
install(DIRECTORY config/ DESTINATION share/${PROJECT_NAME}/config OPTIONAL)
install(FILES scripts/convert_yolov8_to_rknn.py DESTINATION share/${PROJECT_NAME}/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES docs/RKNN_SETUP_GUIDE.md DESTINATION share/${PROJECT_NAME}/docs)



# Print configuration summary
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "SQLite3 found: ${SQLITE3_FOUND}")
message(STATUS "FFmpeg found: ${LIBAV_FOUND}")
message(STATUS "NVML found: ${NVML_FOUND}")
message(STATUS "RKNN found: ${RKNN_FOUND}")
message(STATUS "TensorRT found: ${TENSORRT_FOUND}")
message(STATUS "libcurl found: ${CURL_FOUND}")
message(STATUS "")
message(STATUS "YOLOv8 Detector Backends Available:")
if(RKNN_FOUND)
    message(STATUS "  - RKNN NPU (RK3588): YES")
else()
    message(STATUS "  - RKNN NPU (RK3588): NO")
endif()
if(TENSORRT_FOUND)
    message(STATUS "  - TensorRT GPU: YES")
else()
    message(STATUS "  - TensorRT GPU: NO")
endif()

# InsightFace Integration (Optional)
# Use local third_party installation
set(INSIGHTFACE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/third_party/insightface")
set(INSIGHTFACE_INCLUDE_DIR "${INSIGHTFACE_ROOT}/include")
set(INSIGHTFACE_LIB_DIR "${INSIGHTFACE_ROOT}/lib")
set(INSIGHTFACE_MODEL_DIR "${INSIGHTFACE_ROOT}/models")

# Try to find InsightFace library
find_library(INSIGHTFACE_LIB
    NAMES InspireFace
    PATHS ${INSIGHTFACE_LIB_DIR}
    NO_DEFAULT_PATH
)

if(INSIGHTFACE_LIB AND EXISTS "${INSIGHTFACE_INCLUDE_DIR}/inspireface.h")
    message(STATUS "Found InsightFace library: ${INSIGHTFACE_LIB}")
    message(STATUS "InsightFace include dir: ${INSIGHTFACE_INCLUDE_DIR}")
    message(STATUS "InsightFace model dir: ${INSIGHTFACE_MODEL_DIR}")
    target_include_directories(${PROJECT_NAME} PRIVATE ${INSIGHTFACE_INCLUDE_DIR})
    target_link_libraries(${PROJECT_NAME} ${INSIGHTFACE_LIB})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_INSIGHTFACE=1)
    target_compile_definitions(${PROJECT_NAME} PRIVATE INSIGHTFACE_MODEL_PATH="${INSIGHTFACE_MODEL_DIR}/Pikachu.pack")
    message(STATUS "InsightFace integration enabled")
else()
    message(STATUS "InsightFace library not found in third_party. Using RKNN fallback.")
    message(STATUS "  - Library path: ${INSIGHTFACE_LIB}")
    message(STATUS "  - Header path: ${INSIGHTFACE_INCLUDE_DIR}/inspireface.h")
endif()
