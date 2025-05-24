#include <iostream>
#include <opencv2/opencv.hpp>

int main() {
    std::cout << "Basic test - OpenCV version: " << CV_VERSION << std::endl;
    
    // Test OpenCV functionality
    cv::Mat testImage = cv::Mat::zeros(100, 100, CV_8UC3);
    if (testImage.empty()) {
        std::cerr << "Failed to create test image" << std::endl;
        return 1;
    }
    
    std::cout << "Basic test passed!" << std::endl;
    return 0;
}
