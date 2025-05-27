#include "src/utils/PolygonValidator.h"
#include <iostream>

int main() {
    std::cout << "🧪 Debug Self-Intersection Test\n" << std::endl;
    
    PolygonValidator validator;
    
    // Test self-intersecting bowtie shape
    std::vector<cv::Point> bowtie = {
        cv::Point(100, 100),
        cv::Point(200, 200),
        cv::Point(200, 100),
        cv::Point(100, 200)
    };
    
    std::cout << "Testing bowtie with points:" << std::endl;
    for (size_t i = 0; i < bowtie.size(); ++i) {
        std::cout << "  Point " << i << ": (" << bowtie[i].x << ", " << bowtie[i].y << ")" << std::endl;
    }
    
    auto result = validator.validate(bowtie);
    
    std::cout << "\nValidation Result:" << std::endl;
    std::cout << "  isValid: " << (result.isValid ? "true" : "false") << std::endl;
    std::cout << "  errorMessage: " << result.errorMessage << std::endl;
    std::cout << "  errorCode: " << result.errorCode << std::endl;
    std::cout << "  area: " << result.area << std::endl;
    std::cout << "  isClosed: " << (result.isClosed ? "true" : "false") << std::endl;
    std::cout << "  isConvex: " << (result.isConvex ? "true" : "false") << std::endl;
    std::cout << "  hasSelfIntersection: " << (result.hasSelfIntersection ? "true" : "false") << std::endl;
    
    return 0;
}
