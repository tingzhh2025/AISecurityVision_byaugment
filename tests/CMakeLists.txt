# Tests CMakeLists.txt
# Unit tests for core components

# Create basic test executable
add_executable(test_basic cpp/test_basic.cpp)
target_link_libraries(test_basic ${OpenCV_LIBS})

# Add test
add_test(NAME BasicTest COMMAND test_basic)
