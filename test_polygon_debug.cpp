#include "src/utils/PolygonValidator.h"
#include <iostream>

int main() {
    std::cout << "🧪 Debug PolygonValidator Test\n" << std::endl;
    
    PolygonValidator validator;
    
    // Test simple triangle
    std::vector<cv::Point> validTriangle = {
        cv::Point(100, 100),
        cv::Point(200, 100),
        cv::Point(150, 200)
    };
    
    std::cout << "Testing triangle with points:" << std::endl;
    for (size_t i = 0; i < validTriangle.size(); ++i) {
        std::cout << "  Point " << i << ": (" << validTriangle[i].x << ", " << validTriangle[i].y << ")" << std::endl;
    }
    
    auto result = validator.validate(validTriangle);
    
    std::cout << "\nValidation Result:" << std::endl;
    std::cout << "  isValid: " << (result.isValid ? "true" : "false") << std::endl;
    std::cout << "  errorMessage: " << result.errorMessage << std::endl;
    std::cout << "  errorCode: " << result.errorCode << std::endl;
    std::cout << "  area: " << result.area << std::endl;
    std::cout << "  isClosed: " << (result.isClosed ? "true" : "false") << std::endl;
    std::cout << "  isConvex: " << (result.isConvex ? "true" : "false") << std::endl;
    std::cout << "  hasSelfIntersection: " << (result.hasSelfIntersection ? "true" : "false") << std::endl;
    
    // Test configuration
    auto config = validator.getConfig();
    std::cout << "\nValidator Configuration:" << std::endl;
    std::cout << "  minPoints: " << config.minPoints << std::endl;
    std::cout << "  maxPoints: " << config.maxPoints << std::endl;
    std::cout << "  minArea: " << config.minArea << std::endl;
    std::cout << "  maxArea: " << config.maxArea << std::endl;
    std::cout << "  requireClosed: " << (config.requireClosed ? "true" : "false") << std::endl;
    std::cout << "  allowSelfIntersection: " << (config.allowSelfIntersection ? "true" : "false") << std::endl;
    std::cout << "  requireConvex: " << (config.requireConvex ? "true" : "false") << std::endl;
    
    return 0;
}
