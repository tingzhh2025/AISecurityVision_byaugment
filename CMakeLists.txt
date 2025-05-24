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
