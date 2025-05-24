#include "src/utils/PolygonValidator.h"
#include "src/api/APIService.h"
#include <iostream>
#include <cassert>
#include <vector>

/**
 * @brief Comprehensive test suite for enhanced ROI polygon validation
 * 
 * Tests various polygon validation scenarios including:
 * - Basic validation (point count, coordinate ranges)
 * - Geometric validation (area, self-intersection, convexity)
 * - API integration with detailed error reporting
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
    std::cout << "✅ Valid triangle test passed" << std::endl;
    
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
    std::cout << "✅ Area too small test passed: " << result.errorMessage << std::endl;
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
    std::cout << "✅ Valid quadrilateral test passed" << std::endl;
}

void testConvexityDetection() {
    std::cout << "\n=== Testing Convexity Detection ===" << std::endl;
    
    // Configure validator to require convex polygons
    PolygonValidator::ValidationConfig config;
    config.requireConvex = true;
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
    std::cout << "✅ Convex rectangle test passed" << std::endl;
    
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
    std::cout << "✅ Non-convex L-shape test passed: " << result.errorMessage << std::endl;
}

void testAPIIntegration() {
    std::cout << "\n=== Testing API Integration ===" << std::endl;
    
    APIService apiService(8080);
    
    // Test 1: Valid polygon through API
    std::vector<cv::Point> validPolygon = {
        cv::Point(100, 100),
        cv::Point(300, 100),
        cv::Point(300, 300),
        cv::Point(100, 300)
    };
    
    auto result = apiService.validateROIPolygonDetailed(validPolygon);
    assert(result.isValid);
    std::cout << "✅ API valid polygon test passed" << std::endl;
    
    // Test 2: Invalid polygon through API
    std::vector<cv::Point> invalidPolygon = {
        cv::Point(100, 100),
        cv::Point(200, 100)  // Only 2 points
    };
    
    result = apiService.validateROIPolygonDetailed(invalidPolygon);
    assert(!result.isValid);
    assert(!result.errorMessage.empty());
    assert(!result.errorCode.empty());
    std::cout << "✅ API invalid polygon test passed: " << result.errorMessage << std::endl;
    
    // Test 3: Backward compatibility
    assert(apiService.validateROIPolygon(validPolygon));
    assert(!apiService.validateROIPolygon(invalidPolygon));
    std::cout << "✅ API backward compatibility test passed" << std::endl;
}

void testEdgeCases() {
    std::cout << "\n=== Testing Edge Cases ===" << std::endl;
    
    PolygonValidator validator;
    
    // Test 1: Empty polygon
    std::vector<cv::Point> empty;
    auto result = validator.validate(empty);
    assert(!result.isValid);
    std::cout << "✅ Empty polygon test passed: " << result.errorMessage << std::endl;
    
    // Test 2: Single point
    std::vector<cv::Point> singlePoint = {cv::Point(100, 100)};
    result = validator.validate(singlePoint);
    assert(!result.isValid);
    std::cout << "✅ Single point test passed: " << result.errorMessage << std::endl;
    
    // Test 3: Collinear points
    std::vector<cv::Point> collinear = {
        cv::Point(100, 100),
        cv::Point(150, 100),
        cv::Point(200, 100)
    };
    result = validator.validate(collinear);
    assert(!result.isValid);
    assert(result.errorCode == "AREA_TOO_SMALL");
    std::cout << "✅ Collinear points test passed: " << result.errorMessage << std::endl;
}

int main() {
    std::cout << "🧪 Starting Enhanced ROI Polygon Validation Tests\n" << std::endl;
    
    try {
        testBasicValidation();
        testSelfIntersection();
        testConvexityDetection();
        testAPIIntegration();
        testEdgeCases();
        
        std::cout << "\n🎉 All polygon validation tests passed successfully!" << std::endl;
        std::cout << "✅ Task 48: ROI Polygon Validation - Implementation Complete" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Test failed with unknown exception" << std::endl;
        return 1;
    }
    
    return 0;
}
