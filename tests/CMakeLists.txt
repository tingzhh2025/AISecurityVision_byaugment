# Tests CMakeLists.txt
# TODO: Add unit tests for core components

# For now, create a placeholder test
add_executable(test_basic test_basic.cpp)
target_link_libraries(test_basic ${OpenCV_LIBS})

# Add test
add_test(NAME BasicTest COMMAND test_basic)
