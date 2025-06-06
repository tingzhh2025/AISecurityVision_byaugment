# Test programs CMakeLists.txt
cmake_minimum_required(VERSION 3.16)

# Test for YOLOv8 backends
add_executable(test_yolov8_backends test_yolov8_backends.cpp)

# Include directories from parent
target_include_directories(test_yolov8_backends PRIVATE 
    ${CMAKE_SOURCE_DIR}/src
    ${OpenCV_INCLUDE_DIRS}
)

# Link with necessary libraries
target_link_libraries(test_yolov8_backends 
    ${OpenCV_LIBS}
    pthread
)

# Add source files needed for test
target_sources(test_yolov8_backends PRIVATE
    ${CMAKE_SOURCE_DIR}/src/ai/YOLOv8Detector.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/YOLOv8DetectorFactory.cpp
    ${CMAKE_SOURCE_DIR}/src/ai/YOLOv8CPUDetector.cpp
    ${CMAKE_SOURCE_DIR}/src/core/Logger.cpp
)

# Add backend-specific sources conditionally
if(RKNN_FOUND)
    target_sources(test_yolov8_backends PRIVATE
        ${CMAKE_SOURCE_DIR}/src/ai/YOLOv8RKNNDetector.cpp
    )
    target_link_libraries(test_yolov8_backends ${RKNN_LIBRARIES})
    target_include_directories(test_yolov8_backends PRIVATE ${RKNN_INCLUDE_DIRS})
    target_compile_definitions(test_yolov8_backends PRIVATE HAVE_RKNN)
endif()

if(TENSORRT_FOUND)
    target_sources(test_yolov8_backends PRIVATE
        ${CMAKE_SOURCE_DIR}/src/ai/YOLOv8TensorRTDetector.cpp
    )
    target_link_libraries(test_yolov8_backends ${TENSORRT_LIBRARIES})
    target_include_directories(test_yolov8_backends PRIVATE ${TENSORRT_INCLUDE_DIRS})
    target_compile_definitions(test_yolov8_backends PRIVATE HAVE_TENSORRT)
endif()

# Set C++ standard
target_compile_features(test_yolov8_backends PRIVATE cxx_std_17)

# Install test program
install(TARGETS test_yolov8_backends DESTINATION bin)
