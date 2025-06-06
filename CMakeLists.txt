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

# Find OpenCV 4.5.5 or higher
find_package(OpenCV 4.5.5 REQUIRED)

# Verify we're using a compatible OpenCV version
if(OpenCV_VERSION VERSION_LESS "4.5.5")
    message(FATAL_ERROR "OpenCV 4.5.5 or higher is required, found ${OpenCV_VERSION}")
endif()

# Use system OpenCV libraries instead of hardcoded paths
# This allows the build to work on different architectures
set(OpenCV_LIBS ${OpenCV_LIBS})

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
if(ENABLE_RKNN_NPU)
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
        ${RKNN_ROOT}/lib
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
            ${RKNN_ROOT}/lib
        )
    endif()

    find_path(RKNN_API_INCLUDE_DIR rknn_api.h
        PATHS
        /usr/include
        /usr/local/include
        /opt/rknn/include
        ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu2/include
        ${CMAKE_SOURCE_DIR}/build/models/rknn_model_zoo/3rdparty/rknpu1/include
        ${RKNN_ROOT}/include
    )

    if(RKNN_API_LIBRARY AND RKNN_API_INCLUDE_DIR)
        set(RKNN_FOUND TRUE)
        set(RKNN_LIBRARIES ${RKNN_API_LIBRARY})
        set(RKNN_INCLUDE_DIRS ${RKNN_API_INCLUDE_DIR})
        message(STATUS "Found RKNN API: ${RKNN_API_LIBRARY}")
        message(STATUS "RKNN include: ${RKNN_API_INCLUDE_DIR}")
    else()
        set(RKNN_FOUND FALSE)
        message(STATUS "RKNN API not found - NPU acceleration will not be available")
    endif()
else()
    set(RKNN_FOUND FALSE)
    message(STATUS "RKNN NPU support disabled by user")
endif()

# Platform-specific acceleration options - Make them mutually exclusive
option(ENABLE_CUDA_TENSORRT "Enable CUDA TensorRT acceleration for x86_64/Jetson" OFF)
option(ENABLE_RKNN_NPU "Enable RKNN NPU acceleration for Rockchip platforms" ON)
option(FORCE_BACKEND "Force specific backend: AUTO, RKNN, TENSORRT, CPU" "AUTO")

# Ensure CUDA TensorRT and RKNN NPU are mutually exclusive
if(ENABLE_CUDA_TENSORRT AND ENABLE_RKNN_NPU)
    message(WARNING "Both CUDA TensorRT and RKNN NPU are enabled. Making them mutually exclusive...")
    message(STATUS "Disabling RKNN NPU to prioritize CUDA TensorRT")
    set(ENABLE_RKNN_NPU OFF)
endif()

# Detect platform architecture
if(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64")
    set(IS_ARM64 TRUE)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    set(IS_X86_64 TRUE)
endif()

# Find TensorRT libraries for NVIDIA GPU support (optional)
if(ENABLE_CUDA_TENSORRT)
    # First try to find CUDA
    find_package(CUDA QUIET)
    
    if(CUDA_FOUND)
        message(STATUS "CUDA version: ${CUDA_VERSION}")
        
        # Find TensorRT components
        find_library(TENSORRT_LIBRARY nvinfer
            PATHS
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
            /usr/local/lib
            /usr/local/cuda/lib64
            /opt/tensorrt/lib
            /usr/lib/tensorrt
            ${TENSORRT_ROOT}/lib
        )

        find_library(TENSORRT_ONNX_LIBRARY nvonnxparser
            PATHS
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
            /usr/local/lib
            /usr/local/cuda/lib64
            /opt/tensorrt/lib
            /usr/lib/tensorrt
            ${TENSORRT_ROOT}/lib
        )
        
        # Additional TensorRT libraries for complete support
        find_library(TENSORRT_PLUGIN_LIBRARY nvinfer_plugin
            PATHS
            /usr/lib/x86_64-linux-gnu
            /usr/lib/aarch64-linux-gnu
            /usr/local/lib
            /usr/local/cuda/lib64
            /opt/tensorrt/lib
            /usr/lib/tensorrt
            ${TENSORRT_ROOT}/lib
        )

        find_path(TENSORRT_INCLUDE_DIR NvInfer.h
            PATHS
            /usr/include
            /usr/local/include
            /usr/local/cuda/include
            /opt/tensorrt/include
            /usr/include/tensorrt
            ${TENSORRT_ROOT}/include
        )

        if(TENSORRT_LIBRARY AND TENSORRT_ONNX_LIBRARY AND TENSORRT_INCLUDE_DIR)
            set(TENSORRT_FOUND TRUE)
            set(TENSORRT_LIBRARIES ${TENSORRT_LIBRARY} ${TENSORRT_ONNX_LIBRARY})
            
            # Add plugin library if found
            if(TENSORRT_PLUGIN_LIBRARY)
                list(APPEND TENSORRT_LIBRARIES ${TENSORRT_PLUGIN_LIBRARY})
            endif()
            
            # Add CUDA libraries
            list(APPEND TENSORRT_LIBRARIES ${CUDA_LIBRARIES} ${CUDA_CUDART_LIBRARY})
            
            # Add cuDNN if available
            find_library(CUDNN_LIBRARY cudnn
                PATHS
                /usr/lib/x86_64-linux-gnu
                /usr/lib/aarch64-linux-gnu
                /usr/local/cuda/lib64
                ${CUDA_TOOLKIT_ROOT_DIR}/lib64
            )
            if(CUDNN_LIBRARY)
                list(APPEND TENSORRT_LIBRARIES ${CUDNN_LIBRARY})
                message(STATUS "Found cuDNN: ${CUDNN_LIBRARY}")
            endif()
            
            set(TENSORRT_INCLUDE_DIRS ${TENSORRT_INCLUDE_DIR} ${CUDA_INCLUDE_DIRS})
            message(STATUS "Found TensorRT: ${TENSORRT_LIBRARIES}")
            message(STATUS "TensorRT include: ${TENSORRT_INCLUDE_DIRS}")
            
            # Print TensorRT version if available
            if(EXISTS "${TENSORRT_INCLUDE_DIR}/NvInferVersion.h")
                file(READ "${TENSORRT_INCLUDE_DIR}/NvInferVersion.h" VERSION_FILE_CONTENT)
                string(REGEX MATCH "NV_TENSORRT_MAJOR[ ]+([0-9]+)" _ ${VERSION_FILE_CONTENT})
                set(TENSORRT_VERSION_MAJOR ${CMAKE_MATCH_1})
                string(REGEX MATCH "NV_TENSORRT_MINOR[ ]+([0-9]+)" _ ${VERSION_FILE_CONTENT})
                set(TENSORRT_VERSION_MINOR ${CMAKE_MATCH_1})
                message(STATUS "TensorRT version: ${TENSORRT_VERSION_MAJOR}.${TENSORRT_VERSION_MINOR}")
            endif()
        else()
            set(TENSORRT_FOUND FALSE)
            message(STATUS "TensorRT not found - GPU acceleration will not be available")
        endif()
    else()
        set(TENSORRT_FOUND FALSE)
        message(STATUS "CUDA not found - TensorRT GPU acceleration will not be available")
    endif()
else()
    set(TENSORRT_FOUND FALSE)
    message(STATUS "CUDA TensorRT support disabled by user")
endif()

# Find libcurl for HTTP alarm delivery
find_package(CURL REQUIRED)
if(CURL_FOUND)
    set(CURL_FOUND TRUE)
    message(STATUS "libcurl found: ${CURL_LIBRARIES}")
else()
    message(FATAL_ERROR "libcurl is required for HTTP alarm delivery")
endif()

# Find OpenSSL for JWT authentication (NEW - Phase 2)
find_package(OpenSSL REQUIRED)
if(OpenSSL_FOUND)
    message(STATUS "OpenSSL found: ${OPENSSL_LIBRARIES}")
else()
    message(FATAL_ERROR "OpenSSL is required for JWT authentication")
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
include_directories(${CMAKE_SOURCE_DIR}/third_party/nlohmann)
include_directories(${OpenCV_INCLUDE_DIRS})

# Phase 4 build options
option(ENABLE_SECURITY "Enable security features (rate limiting, input validation)" ON)
option(ENABLE_RATE_LIMITING "Enable API rate limiting" ON)
option(ENABLE_INPUT_VALIDATION "Enable input validation and sanitization" ON)
option(ENABLE_ACCESS_LOGGING "Enable access logging" ON)
option(ENABLE_CONNECTION_POOL "Enable database connection pooling" ON)
option(ENABLE_SWAGGER_UI "Enable Swagger UI for API documentation" ON)
option(ENABLE_PERFORMANCE_MONITORING "Enable performance monitoring" ON)

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
    "src/auth/*.cpp"
    "src/security/*.cpp"
)

# Remove LoggerExample.cpp from sources as it contains unimplemented features
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/core/LoggerExample.cpp")

# Remove YOLOv8DetectorExample.cpp from sources as it contains a main function
list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/ai/YOLOv8DetectorExample.cpp")

# Remove TensorRT implementation if not available
if(NOT TENSORRT_FOUND)
    list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/ai/YOLOv8TensorRTDetector.cpp")
endif()

# Remove RKNN implementation if not available
if(NOT RKNN_FOUND)
    list(REMOVE_ITEM SOURCES "${CMAKE_SOURCE_DIR}/src/ai/YOLOv8RKNNDetector.cpp")
endif()

# AI-only sources for simple tests
file(GLOB AI_SOURCES
    "src/ai/YOLOv8Detector.cpp"
    "src/ai/YOLOv8DetectorFactory.cpp"
    "src/ai/YOLOv8CPUDetector.cpp"
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

# Add OpenSSL for JWT authentication (NEW - Phase 2)
if(OpenSSL_FOUND)
    list(APPEND LINK_LIBRARIES ${OPENSSL_LIBRARIES})
    include_directories(${OPENSSL_INCLUDE_DIR})
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENSSL)
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

# Phase 4 feature definitions
if(ENABLE_SECURITY)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_SECURITY)
    message(STATUS "Security features enabled")
endif()

if(ENABLE_RATE_LIMITING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_RATE_LIMITING)
    message(STATUS "API rate limiting enabled")
endif()

if(ENABLE_INPUT_VALIDATION)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_INPUT_VALIDATION)
    message(STATUS "Input validation enabled")
endif()

if(ENABLE_ACCESS_LOGGING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_ACCESS_LOGGING)
    message(STATUS "Access logging enabled")
endif()

if(ENABLE_CONNECTION_POOL)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_CONNECTION_POOL)
    message(STATUS "Database connection pooling enabled")
endif()

if(ENABLE_SWAGGER_UI)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_SWAGGER_UI)
    message(STATUS "Swagger UI enabled")
endif()

if(ENABLE_PERFORMANCE_MONITORING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_PERFORMANCE_MONITORING)
    message(STATUS "Performance monitoring enabled")
endif()

target_link_libraries(${PROJECT_NAME} ${LINK_LIBRARIES})

# Compiler-specific options
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

# Add tests subdirectory
option(BUILD_TESTS "Build test programs" ON)
if(BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Install targets
install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(DIRECTORY models/ DESTINATION share/${PROJECT_NAME}/models OPTIONAL)
install(DIRECTORY config/ DESTINATION share/${PROJECT_NAME}/config OPTIONAL)
install(FILES scripts/convert_yolov8_to_rknn.py DESTINATION share/${PROJECT_NAME}/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES scripts/convert_yolov8_to_tensorrt.py DESTINATION share/${PROJECT_NAME}/scripts
        PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
install(FILES docs/RKNN_SETUP_GUIDE.md DESTINATION share/${PROJECT_NAME}/docs)
install(FILES docs/TENSORRT_SETUP_GUIDE.md DESTINATION share/${PROJECT_NAME}/docs)



# Print configuration summary
message(STATUS "========================================")
message(STATUS "AISecurityVision Configuration Summary")
message(STATUS "========================================")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "System processor: ${CMAKE_SYSTEM_PROCESSOR}")
message(STATUS "")
message(STATUS "Core Dependencies:")
message(STATUS "  OpenCV version: ${OpenCV_VERSION}")
message(STATUS "  SQLite3: ${SQLITE3_FOUND}")
message(STATUS "  FFmpeg: ${LIBAV_FOUND}")
message(STATUS "  libcurl: ${CURL_FOUND}")
message(STATUS "  OpenSSL: ${OpenSSL_FOUND}")
message(STATUS "")
message(STATUS "AI Acceleration Backends:")
if(RKNN_FOUND)
    message(STATUS "  RKNN NPU (RK3588): ENABLED")
    message(STATUS "    - Library: ${RKNN_LIBRARIES}")
    message(STATUS "    - Include: ${RKNN_INCLUDE_DIRS}")
else()
    message(STATUS "  RKNN NPU (RK3588): DISABLED")
endif()
if(TENSORRT_FOUND)
    message(STATUS "  TensorRT GPU: ENABLED")
    message(STATUS "    - CUDA version: ${CUDA_VERSION}")
    message(STATUS "    - Libraries: ${TENSORRT_LIBRARIES}")
    if(DEFINED TENSORRT_VERSION_MAJOR)
        message(STATUS "    - TensorRT version: ${TENSORRT_VERSION_MAJOR}.${TENSORRT_VERSION_MINOR}")
    endif()
else()
    message(STATUS "  TensorRT GPU: DISABLED")
endif()
message(STATUS "  CPU Fallback: ALWAYS AVAILABLE")
message(STATUS "")
message(STATUS "Platform Options:")
message(STATUS "  ENABLE_CUDA_TENSORRT: ${ENABLE_CUDA_TENSORRT}")
message(STATUS "  ENABLE_RKNN_NPU: ${ENABLE_RKNN_NPU}")
message(STATUS "  FORCE_BACKEND: ${FORCE_BACKEND}")
message(STATUS "")
message(STATUS "Additional Features:")
message(STATUS "  NVML (GPU monitoring): ${NVML_FOUND}")
message(STATUS "  WebSocket support: ${WEBSOCKETPP_INCLUDE_DIR}")
message(STATUS "  MQTT support: ${PAHO_MQTT_CPP_FOUND}")
message(STATUS "  InsightFace: ${INSIGHTFACE_LIB}")
message(STATUS "========================================")
message(STATUS "")

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
