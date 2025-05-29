#include "PolygonValidator.h"
#include <cmath>
#include <algorithm>
#include <iostream>

PolygonValidator::PolygonValidator() {
    // Use default configuration
}

PolygonValidator::PolygonValidator(const ValidationConfig& config) : m_config(config) {
}

PolygonValidator::ValidationResult PolygonValidator::validate(const std::vector<cv::Point>& polygon) const {
    ValidationResult result;

    // Perform all validation checks in logical order
    if (!validatePointCount(polygon, result)) return result;
    if (!validateCoordinateRanges(polygon, result)) return result;
    if (!validateClosed(polygon, result)) return result;
    if (!validateSelfIntersection(polygon, result)) return result;  // Check before area
    if (!validateArea(polygon, result)) return result;
    if (!validateConvexity(polygon, result)) return result;

    // If all validations pass
    result.isValid = true;
    result.errorMessage = "Polygon is valid";
    result.errorCode = "VALID";

    return result;
}

bool PolygonValidator::isValid(const std::vector<cv::Point>& polygon) const {
    return validate(polygon).isValid;
}

void PolygonValidator::setConfig(const ValidationConfig& config) {
    m_config = config;
}

const PolygonValidator::ValidationConfig& PolygonValidator::getConfig() const {
    return m_config;
}

bool PolygonValidator::validatePointCount(const std::vector<cv::Point>& polygon, ValidationResult& result) const {
    int pointCount = static_cast<int>(polygon.size());

    if (pointCount < m_config.minPoints) {
        result.errorMessage = "Polygon has too few points (minimum " + std::to_string(m_config.minPoints) + " required)";
        result.errorCode = "INSUFFICIENT_POINTS";
        return false;
    }

    if (pointCount > m_config.maxPoints) {
        result.errorMessage = "Polygon has too many points (maximum " + std::to_string(m_config.maxPoints) + " allowed)";
        result.errorCode = "EXCESSIVE_POINTS";
        return false;
    }

    return true;
}

bool PolygonValidator::validateCoordinateRanges(const std::vector<cv::Point>& polygon, ValidationResult& result) const {
    for (size_t i = 0; i < polygon.size(); ++i) {
        const cv::Point& point = polygon[i];

        if (point.x < m_config.minX || point.x > m_config.maxX) {
            result.errorMessage = "Point " + std::to_string(i) + " X coordinate (" + std::to_string(point.x) +
                                ") is out of range [" + std::to_string(m_config.minX) + ", " + std::to_string(m_config.maxX) + "]";
            result.errorCode = "COORDINATE_OUT_OF_RANGE";
            return false;
        }

        if (point.y < m_config.minY || point.y > m_config.maxY) {
            result.errorMessage = "Point " + std::to_string(i) + " Y coordinate (" + std::to_string(point.y) +
                                ") is out of range [" + std::to_string(m_config.minY) + ", " + std::to_string(m_config.maxY) + "]";
            result.errorCode = "COORDINATE_OUT_OF_RANGE";
            return false;
        }
    }

    return true;
}

bool PolygonValidator::validateClosed(const std::vector<cv::Point>& polygon, ValidationResult& result) const {
    result.isClosed = isPolygonClosed(polygon);

    if (m_config.requireClosed && !result.isClosed) {
        result.errorMessage = "Polygon is not closed (first and last points must be the same)";
        result.errorCode = "NOT_CLOSED";
        return false;
    }

    return true;
}

bool PolygonValidator::validateArea(const std::vector<cv::Point>& polygon, ValidationResult& result) const {
    result.area = calculatePolygonArea(polygon);

    if (result.area < m_config.minArea) {
        result.errorMessage = "Polygon area (" + std::to_string(result.area) +
                            ") is below minimum (" + std::to_string(m_config.minArea) + ")";
        result.errorCode = "AREA_TOO_SMALL";
        return false;
    }

    if (result.area > m_config.maxArea) {
        result.errorMessage = "Polygon area (" + std::to_string(result.area) +
                            ") exceeds maximum (" + std::to_string(m_config.maxArea) + ")";
        result.errorCode = "AREA_TOO_LARGE";
        return false;
    }

    return true;
}

bool PolygonValidator::validateSelfIntersection(const std::vector<cv::Point>& polygon, ValidationResult& result) const {
    result.hasSelfIntersection = hasPolygonSelfIntersection(polygon);

    if (!m_config.allowSelfIntersection && result.hasSelfIntersection) {
        result.errorMessage = "Polygon has self-intersecting edges";
        result.errorCode = "SELF_INTERSECTION";
        return false;
    }

    return true;
}

bool PolygonValidator::validateConvexity(const std::vector<cv::Point>& polygon, ValidationResult& result) const {
    result.isConvex = isPolygonConvex(polygon);

    if (m_config.requireConvex && !result.isConvex) {
        result.errorMessage = "Polygon is not convex";
        result.errorCode = "NOT_CONVEX";
        return false;
    }

    return true;
}

double PolygonValidator::calculatePolygonArea(const std::vector<cv::Point>& polygon) const {
    if (polygon.size() < 3) return 0.0;

    // Use the shoelace formula
    double area = 0.0;
    size_t n = polygon.size();

    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        area += polygon[i].x * polygon[j].y;
        area -= polygon[j].x * polygon[i].y;
    }

    return std::abs(area) / 2.0;
}

bool PolygonValidator::isPolygonClosed(const std::vector<cv::Point>& polygon) const {
    if (polygon.size() < 3) return false;

    // Check if first and last points are the same
    return polygon.front() == polygon.back();
}

bool PolygonValidator::isPolygonConvex(const std::vector<cv::Point>& polygon) const {
    if (polygon.size() < 3) return false;

    size_t n = polygon.size();
    bool hasPositive = false;
    bool hasNegative = false;

    for (size_t i = 0; i < n; ++i) {
        size_t j = (i + 1) % n;
        size_t k = (i + 2) % n;

        int cross = (polygon[j].x - polygon[i].x) * (polygon[k].y - polygon[j].y) -
                   (polygon[j].y - polygon[i].y) * (polygon[k].x - polygon[j].x);

        if (cross > 0) hasPositive = true;
        if (cross < 0) hasNegative = true;

        if (hasPositive && hasNegative) return false;
    }

    return true;
}

bool PolygonValidator::hasPolygonSelfIntersection(const std::vector<cv::Point>& polygon) const {
    if (polygon.size() < 4) return false;

    size_t n = polygon.size();

    // Check each pair of non-adjacent edges for intersection
    for (size_t i = 0; i < n; ++i) {
        size_t i1 = (i + 1) % n;

        for (size_t j = i + 2; j < n; ++j) {
            size_t j1 = (j + 1) % n;

            // Skip adjacent edges
            if (j1 == i) continue;

            if (doLinesIntersect(polygon[i], polygon[i1], polygon[j], polygon[j1])) {
                return true;
            }
        }
    }

    return false;
}

bool PolygonValidator::doLinesIntersect(const cv::Point& p1, const cv::Point& q1,
                                       const cv::Point& p2, const cv::Point& q2) const {
    int o1 = orientation(p1, q1, p2);
    int o2 = orientation(p1, q1, q2);
    int o3 = orientation(p2, q2, p1);
    int o4 = orientation(p2, q2, q1);

    // General case
    if (o1 != o2 && o3 != o4) return true;

    // Special cases
    // p1, q1 and p2 are colinear and p2 lies on segment p1q1
    if (o1 == 0 && onSegment(p1, p2, q1)) return true;

    // p1, q1 and q2 are colinear and q2 lies on segment p1q1
    if (o2 == 0 && onSegment(p1, q2, q1)) return true;

    // p2, q2 and p1 are colinear and p1 lies on segment p2q2
    if (o3 == 0 && onSegment(p2, p1, q2)) return true;

    // p2, q2 and q1 are colinear and q1 lies on segment p2q2
    if (o4 == 0 && onSegment(p2, q1, q2)) return true;

    return false;
}

int PolygonValidator::orientation(const cv::Point& p, const cv::Point& q, const cv::Point& r) const {
    // Calculate the orientation of ordered triplet (p, q, r)
    // Returns:
    // 0 --> p, q and r are colinear
    // 1 --> Clockwise
    // 2 --> Counterclockwise

    long long val = (long long)(q.y - p.y) * (r.x - q.x) - (long long)(q.x - p.x) * (r.y - q.y);

    if (val == 0) return 0;  // colinear
    return (val > 0) ? 1 : 2; // clockwise or counterclockwise
}

bool PolygonValidator::onSegment(const cv::Point& p, const cv::Point& q, const cv::Point& r) const {
    // Check if point q lies on line segment pr
    return q.x <= std::max(p.x, r.x) && q.x >= std::min(p.x, r.x) &&
           q.y <= std::max(p.y, r.y) && q.y >= std::min(p.y, r.y);
}
