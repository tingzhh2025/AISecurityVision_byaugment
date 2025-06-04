#pragma once

#include "BaseController.h"
#include <vector>
#include <string>
#include <opencv2/opencv.hpp>

namespace AISecurityVision {

/**
 * @brief Controller for person statistics functionality
 * 
 * Handles person analytics and statistics including:
 * - Person statistics configuration per camera
 * - Enable/disable person statistics
 * - Retrieve person statistics data
 * - Age/gender recognition settings
 */
class PersonStatsController : public BaseController {
public:
    PersonStatsController() = default;
    ~PersonStatsController() override = default;

    // Person statistics data structure
    struct PersonStats {
        int total_persons = 0;
        int male_count = 0;
        int female_count = 0;
        int child_count = 0;
        int young_count = 0;
        int middle_count = 0;
        int senior_count = 0;
        // InsightFace enhanced attributes
        int black_count = 0;
        int asian_count = 0;
        int latino_count = 0;
        int middle_eastern_count = 0;
        int white_count = 0;
        int mask_count = 0;
        int no_mask_count = 0;
        float average_quality = 0.0f;
        std::vector<cv::Rect> person_boxes;
        std::vector<std::string> person_genders;
        std::vector<std::string> person_ages;
        std::vector<std::string> person_races;
        std::vector<float> person_qualities;
        std::vector<bool> person_masks;
    };

    // Person statistics configuration structure
    struct PersonStatsConfig {
        bool enabled = false;
        float gender_threshold = 0.7f;
        float age_threshold = 0.6f;
        int batch_size = 4;
        bool enable_caching = true;
    };

    // Person statistics handlers
    void handleGetPersonStats(const std::string& request, std::string& response, const std::string& cameraId);
    void handlePostPersonStatsEnable(const std::string& request, std::string& response, const std::string& cameraId);
    void handlePostPersonStatsDisable(const std::string& request, std::string& response, const std::string& cameraId);
    void handleGetPersonStatsConfig(const std::string& request, std::string& response, const std::string& cameraId);
    void handlePostPersonStatsConfig(const std::string& request, std::string& response, const std::string& cameraId);

private:
    std::string getControllerName() const override { return "PersonStatsController"; }

    // Person statistics serialization
    std::string serializePersonStats(const PersonStats& stats);
    std::string serializePersonStatsConfig(const PersonStatsConfig& config);
    bool deserializePersonStatsConfig(const std::string& json, PersonStatsConfig& config);
};

} // namespace AISecurityVision
