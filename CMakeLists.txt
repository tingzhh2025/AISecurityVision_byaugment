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
find_package(OpenCV REQUIRED)

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

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/include)
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
)

# Create executable
add_executable(${PROJECT_NAME}
    src/main.cpp
    ${SOURCES}
)

# Link libraries
set(LINK_LIBRARIES ${OpenCV_LIBS} pthread dl)

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

target_link_libraries(${PROJECT_NAME} ${LINK_LIBRARIES})

# Compiler-specific options
target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_17)

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
message(STATUS "OpenCV version: ${OpenCV_VERSION}")
message(STATUS "SQLite3 found: ${SQLITE3_FOUND}")
message(STATUS "FFmpeg found: ${LIBAV_FOUND}")
message(STATUS "NVML found: ${NVML_FOUND}")
message(STATUS "libcurl found: ${CURL_FOUND}")
