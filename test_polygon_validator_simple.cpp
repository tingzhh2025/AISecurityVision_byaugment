#include "src/utils/PolygonValidator.h"
#include <iostream>
#include <cassert>
#include <vector>

/**
 * @brief Simple test suite for PolygonValidator class
 * 
 * Tests the core polygon validation functionality without API dependencies
 */

void testBasicValidation() {
    std::cout << "=== Testing Basic Polygon Validation ===" << std::endl;
    
    PolygonValidator validator;
    
    // Test 1: Valid triangle
    std::vector<cv::Point> validTriangle = {
        cv::Point(100, 100),
        cv::Point(200, 100),
        cv::Point(150, 200)
    };
    auto result = validator.validate(validTriangle);
    assert(result.isValid);
    std::cout << "✅ Valid triangle test passed (Area: " << result.area << ")" << std::endl;
    
    // Test 2: Too few points
    std::vector<cv::Point> twoPoints = {
        cv::Point(100, 100),
        cv::Point(200, 100)
    };
    result = validator.validate(twoPoints);
    assert(!result.isValid);
    assert(result.errorCode == "INSUFFICIENT_POINTS");
    std::cout << "✅ Insufficient points test passed: " << result.errorMessage << std::endl;
    
    // Test 3: Coordinates out of range
    std::vector<cv::Point> outOfRange = {
        cv::Point(-10, 100),
        cv::Point(200, 100),
        cv::Point(150, 200)
    };
    result = validator.validate(outOfRange);
    assert(!result.isValid);
    assert(result.errorCode == "COORDINATE_OUT_OF_RANGE");
    std::cout << "✅ Coordinate out of range test passed: " << result.errorMessage << std::endl;
    
    // Test 4: Area too small
    std::vector<cv::Point> tinyTriangle = {
        cv::Point(100, 100),
        cv::Point(101, 100),
        cv::Point(100, 101)
    };
    result = validator.validate(tinyTriangle);
    assert(!result.isValid);
    assert(result.errorCode == "AREA_TOO_SMALL");
    std::cout << "✅ Area too small test passed: " << result.errorMessage << " (Area: " << result.area << ")" << std::endl;
}

void testSelfIntersection() {
    std::cout << "\n=== Testing Self-Intersection Detection ===" << std::endl;
    
    PolygonValidator validator;
    
    // Test 1: Self-intersecting bowtie shape
    std::vector<cv::Point> bowtie = {
        cv::Point(100, 100),
        cv::Point(200, 200),
        cv::Point(200, 100),
        cv::Point(100, 200)
    };
    auto result = validator.validate(bowtie);
    assert(!result.isValid);
    assert(result.errorCode == "SELF_INTERSECTION");
    assert(result.hasSelfIntersection);
    std::cout << "✅ Self-intersection detection test passed: " << result.errorMessage << std::endl;
    
    // Test 2: Valid non-intersecting quadrilateral
    std::vector<cv::Point> validQuad = {
        cv::Point(100, 100),
        cv::Point(200, 100),
        cv::Point(200, 200),
        cv::Point(100, 200)
    };
    result = validator.validate(validQuad);
    assert(result.isValid);
    assert(!result.hasSelfIntersection);
    std::cout << "✅ Valid quadrilateral test passed (Area: " << result.area << ")" << std::endl;
}

void testConvexityDetection() {
    std::cout << "\n=== Testing Convexity Detection ===" << std::endl;
    
    // Configure validator to require convex polygons
    PolygonValidator::ValidationConfig config;
    config.requireConvex = true;
    config.minArea = 100.0;  // Lower minimum for test shapes
    PolygonValidator validator(config);
    
    // Test 1: Convex rectangle
    std::vector<cv::Point> convexRect = {
        cv::Point(100, 100),
        cv::Point(200, 100),
        cv::Point(200, 200),
        cv::Point(100, 200)
    };
    auto result = validator.validate(convexRect);
    assert(result.isValid);
    assert(result.isConvex);
    std::cout << "✅ Convex rectangle test passed (Area: " << result.area << ")" << std::endl;
    
    // Test 2: Non-convex L-shape
    std::vector<cv::Point> lShape = {
        cv::Point(100, 100),
        cv::Point(200, 100),
        cv::Point(200, 150),
        cv::Point(150, 150),
        cv::Point(150, 200),
        cv::Point(100, 200)
    };
    result = validator.validate(lShape);
    assert(!result.isValid);
    assert(result.errorCode == "NOT_CONVEX");
    assert(!result.isConvex);
    std::cout << "✅ Non-convex L-shape test passed: " << result.errorMessage << " (Area: " << result.area << ")" << std::endl;
}

void testAreaCalculation() {
    std::cout << "\n=== Testing Area Calculation ===" << std::endl;
    
    PolygonValidator validator;
    
    // Test 1: Rectangle area calculation
    std::vector<cv::Point> rectangle = {
        cv::Point(0, 0),
        cv::Point(100, 0),
        cv::Point(100, 50),
        cv::Point(0, 50)
    };
    auto result = validator.validate(rectangle);
    assert(result.isValid);
    assert(std::abs(result.area - 5000.0) < 1.0);  // 100 * 50 = 5000
    std::cout << "✅ Rectangle area calculation test passed: " << result.area << " (expected ~5000)" << std::endl;
    
    // Test 2: Triangle area calculation
    std::vector<cv::Point> triangle = {
        cv::Point(0, 0),
        cv::Point(100, 0),
        cv::Point(50, 100)
    };
    result = validator.validate(triangle);
    assert(result.isValid);
    assert(std::abs(result.area - 5000.0) < 1.0);  // 0.5 * 100 * 100 = 5000
    std::cout << "✅ Triangle area calculation test passed: " << result.area << " (expected ~5000)" << std::endl;
}

void testEdgeCases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;
    
    PolygonValidator validator;
    
    // Test 1: Empty polygon
    std::vector<cv::Point> empty;
    auto result = validator.validate(empty);
    assert(!result.isValid);
    assert(result.errorCode == "INSUFFICIENT_POINTS");
    std::cout << "✅ Empty polygon test passed: " << result.errorMessage << std::endl;
    
    // Test 2: Single point
    std::vector<cv::Point> singlePoint = {cv::Point(100, 100)};
    result = validator.validate(singlePoint);
    assert(!result.isValid);
    assert(result.errorCode == "INSUFFICIENT_POINTS");
    std::cout << "✅ Single point test passed: " << result.errorMessage << std::endl;
    
    // Test 3: Collinear points (zero area)
    std::vector<cv::Point> collinear = {
        cv::Point(100, 100),
        cv::Point(150, 100),
        cv::Point(200, 100)
    };
    result = validator.validate(collinear);
    assert(!result.isValid);
    assert(result.errorCode == "AREA_TOO_SMALL");
    assert(result.area == 0.0);
    std::cout << "✅ Collinear points test passed: " << result.errorMessage << " (Area: " << result.area << ")" << std::endl;
}

void testConfigurationOptions() {
    std::cout << "\n=== Testing Configuration Options ===" << std::endl;
    
    // Test custom configuration
    PolygonValidator::ValidationConfig config;
    config.minPoints = 4;  // Require at least 4 points
    config.maxPoints = 10;
    config.minArea = 1000.0;  // Higher minimum area
    config.requireClosed = true;
    config.allowSelfIntersection = true;  // Allow self-intersection
    
    PolygonValidator validator(config);
    
    // Test 1: Triangle should fail (< 4 points)
    std::vector<cv::Point> triangle = {
        cv::Point(100, 100),
        cv::Point(200, 100),
        cv::Point(150, 200)
    };
    auto result = validator.validate(triangle);
    assert(!result.isValid);
    assert(result.errorCode == "INSUFFICIENT_POINTS");
    std::cout << "✅ Custom min points test passed: " << result.errorMessage << std::endl;
    
    // Test 2: Valid quadrilateral with sufficient area
    std::vector<cv::Point> largeQuad = {
        cv::Point(100, 100),
        cv::Point(200, 100),
        cv::Point(200, 200),
        cv::Point(100, 200),
        cv::Point(100, 100)  // Closed polygon
    };
    result = validator.validate(largeQuad);
    assert(result.isValid);
    assert(result.isClosed);
    std::cout << "✅ Custom configuration test passed (Area: " << result.area << ")" << std::endl;
}

int main() {
    std::cout << "🧪 Starting PolygonValidator Tests\n" << std::endl;
    
    try {
        testBasicValidation();
        testSelfIntersection();
        testConvexityDetection();
        testAreaCalculation();
        testEdgeCases();
        testConfigurationOptions();
        
        std::cout << "\n🎉 All PolygonValidator tests passed successfully!" << std::endl;
        std::cout << "✅ Task 48: ROI Polygon Validation - Core Implementation Complete" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
