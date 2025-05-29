#pragma once

#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

/**
 * @brief Comprehensive polygon validation utility class
 *
 * Provides advanced validation for ROI polygons including:
 * - Basic validation (minimum points, coordinate ranges)
 * - Geometric validation (self-intersection, area, convexity)
 * - Detailed error reporting with specific validation codes
 */
class PolygonValidator {
public:
    /**
     * @brief Validation result structure with detailed error information
     */
    struct ValidationResult {
        bool isValid;
        std::string errorMessage;
        std::string errorCode;
        double area;
        bool isClosed;
        bool isConvex;
        bool hasSelfIntersection;

        ValidationResult() : isValid(false), area(0.0), isClosed(false),
                           isConvex(false), hasSelfIntersection(false) {}
    };

    /**
     * @brief Validation configuration parameters
     */
    struct ValidationConfig {
        int minPoints = 3;
        int maxPoints = 100;
        int minX = 0;
        int maxX = 10000;
        int minY = 0;
        int maxY = 10000;
        double minArea = 100.0;  // Minimum area in pixels
        double maxArea = 1000000.0;  // Maximum area in pixels
        bool requireClosed = false;  // Allow open polygons for ROI flexibility
        bool allowSelfIntersection = false;
        bool requireConvex = false;

        ValidationConfig() = default;
    };

public:
    /**
     * @brief Constructor with default validation configuration
     */
    PolygonValidator();

    /**
     * @brief Constructor with custom validation configuration
     * @param config Custom validation parameters
     */
    explicit PolygonValidator(const ValidationConfig& config);

    /**
     * @brief Validate polygon with detailed result
     * @param polygon Vector of polygon vertices
     * @return Detailed validation result
     */
    ValidationResult validate(const std::vector<cv::Point>& polygon) const;

    /**
     * @brief Simple boolean validation (backward compatibility)
     * @param polygon Vector of polygon vertices
     * @return True if polygon is valid, false otherwise
     */
    bool isValid(const std::vector<cv::Point>& polygon) const;

    /**
     * @brief Update validation configuration
     * @param config New validation parameters
     */
    void setConfig(const ValidationConfig& config);

    /**
     * @brief Get current validation configuration
     * @return Current validation parameters
     */
    const ValidationConfig& getConfig() const;

private:
    ValidationConfig m_config;

    // Basic validation methods
    bool validatePointCount(const std::vector<cv::Point>& polygon, ValidationResult& result) const;
    bool validateCoordinateRanges(const std::vector<cv::Point>& polygon, ValidationResult& result) const;
    bool validateClosed(const std::vector<cv::Point>& polygon, ValidationResult& result) const;

    // Geometric validation methods
    bool validateArea(const std::vector<cv::Point>& polygon, ValidationResult& result) const;
    bool validateSelfIntersection(const std::vector<cv::Point>& polygon, ValidationResult& result) const;
    bool validateConvexity(const std::vector<cv::Point>& polygon, ValidationResult& result) const;

    // Utility methods
    double calculatePolygonArea(const std::vector<cv::Point>& polygon) const;
    bool isPolygonClosed(const std::vector<cv::Point>& polygon) const;
    bool isPolygonConvex(const std::vector<cv::Point>& polygon) const;
    bool hasPolygonSelfIntersection(const std::vector<cv::Point>& polygon) const;
    bool doLinesIntersect(const cv::Point& p1, const cv::Point& q1,
                         const cv::Point& p2, const cv::Point& q2) const;
    int orientation(const cv::Point& p, const cv::Point& q, const cv::Point& r) const;
    bool onSegment(const cv::Point& p, const cv::Point& q, const cv::Point& r) const;
};
