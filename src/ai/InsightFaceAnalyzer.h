/**
 * @file InsightFaceAnalyzer.h
 * @brief Age, Gender, and Race Analysis using InsightFace
 *
 * This file implements comprehensive face analysis using InsightFace library
 * for age, gender, race recognition, and face quality assessment.
 * Designed as a replacement for the RKNN-based AgeGenderAnalyzer.
 */

#ifndef INSIGHTFACE_ANALYZER_H
#define INSIGHTFACE_ANALYZER_H

#include <vector>
#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include "PersonFilter.h"

// Forward declarations for InsightFace C API
typedef void* HFSession;
typedef void* HFImageStream;
typedef void* HFImageBitmap;
typedef int HResult;
typedef int HOption;

namespace AISecurityVision {

/**
 * @brief Enhanced person attributes structure with InsightFace capabilities
 */
struct PersonAttributes {
    std::string gender = "unknown";           // "male", "female", "unknown"
    std::string age_group = "unknown";        // "child", "young", "middle", "senior", "unknown"
    std::string race = "unknown";             // "black", "asian", "latino", "middle_eastern", "white", "unknown"
    float gender_confidence = 0.0f;          // Gender prediction confidence
    float age_confidence = 0.0f;             // Age prediction confidence
    float race_confidence = 0.0f;            // Race prediction confidence
    float quality_score = 0.0f;              // Face quality score (0.0-1.0)
    bool has_mask = false;                    // Mask detection result
    int track_id = -1;                        // Associated track ID
    int64_t timestamp = 0;                    // Analysis timestamp
    
    PersonAttributes() = default;
    
    PersonAttributes(const std::string& g, const std::string& a, 
                    float gc = 0.0f, float ac = 0.0f, 
                    int tid = -1, int64_t ts = 0)
        : gender(g), age_group(a), gender_confidence(gc), 
          age_confidence(ac), track_id(tid), timestamp(ts) {}
    
    bool isValid() const {
        return gender != "unknown" && age_group != "unknown" &&
               gender_confidence > 0.0f && age_confidence > 0.0f;
    }
    
    std::string toString() const {
        return "Gender: " + gender + " (" + std::to_string(gender_confidence) + 
               "), Age: " + age_group + " (" + std::to_string(age_confidence) + 
               "), Race: " + race + ", Quality: " + std::to_string(quality_score) +
               ", Mask: " + (has_mask ? "Yes" : "No");
    }
};

/**
 * @brief Age, Gender, and Race analyzer using InsightFace
 *
 * This class implements comprehensive face analysis using InsightFace library
 * including age, gender, race recognition, quality assessment, and mask detection.
 */
class InsightFaceAnalyzer {
public:
    InsightFaceAnalyzer();
    virtual ~InsightFaceAnalyzer();
    
    /**
     * @brief Initialize the analyzer with InsightFace model pack
     * @param packPath Path to the InsightFace model pack file
     * @return true if initialization successful, false otherwise
     */
    bool initialize(const std::string& packPath = "models/Pikachu.pack");
    
    /**
     * @brief Analyze face attributes for multiple persons
     * @param persons Vector of person detections with crops
     * @return Vector of person attributes
     */
    std::vector<PersonAttributes> analyze(const std::vector<PersonDetection>& persons);
    
    /**
     * @brief Analyze face attributes for a single person
     * @param personCrop Person crop image
     * @return Person attributes
     */
    PersonAttributes analyzeSingle(const cv::Mat& personCrop);
    
    /**
     * @brief Check if analyzer is initialized
     * @return true if initialized, false otherwise
     */
    bool isInitialized() const;
    
    /**
     * @brief Cleanup resources
     */
    void cleanup();
    
    /**
     * @brief Get model information
     * @return Vector of model info strings
     */
    std::vector<std::string> getModelInfo() const;
    
    // Configuration methods
    void setGenderThreshold(float threshold) { m_genderThreshold = threshold; }
    void setAgeThreshold(float threshold) { m_ageThreshold = threshold; }
    void setQualityThreshold(float threshold) { m_qualityThreshold = threshold; }
    void setMaxDetectNum(int maxNum) { m_maxDetectNum = maxNum; }
    void setDetectPixelLevel(int level) { m_detectPixelLevel = level; }
    
    float getGenderThreshold() const { return m_genderThreshold; }
    float getAgeThreshold() const { return m_ageThreshold; }
    float getQualityThreshold() const { return m_qualityThreshold; }
    int getMaxDetectNum() const { return m_maxDetectNum; }
    int getDetectPixelLevel() const { return m_detectPixelLevel; }
    
    // Performance metrics
    double getLastInferenceTime() const { return m_inferenceTime; }
    double getAverageInferenceTime() const;
    size_t getAnalysisCount() const { return m_analysisCount; }

private:
    // InsightFace session and handles
    HFSession m_session;
    HFImageStream m_imageStream;
    
    // Configuration
    float m_genderThreshold = 0.7f;
    float m_ageThreshold = 0.6f;
    float m_qualityThreshold = 0.5f;
    int m_maxDetectNum = 20;
    int m_detectPixelLevel = 160;
    
    // State
    bool m_initialized = false;
    std::string m_packPath;
    
    // Performance tracking
    double m_inferenceTime = 0.0;
    std::vector<double> m_inferenceTimes;
    size_t m_analysisCount = 0;
    
    // Helper methods
    PersonAttributes processInsightFaceResult(int faceIndex, 
                                            const void* multipleFaceData,
                                            const void* attributeResult);
    std::string mapInsightFaceGender(int genderCode);
    std::string mapInsightFaceAge(int ageBracket);
    std::string mapInsightFaceRace(int raceCode);
    cv::Mat preprocessImage(const cv::Mat& image);
    
    // Constants
    static constexpr float DEFAULT_GENDER_THRESHOLD = 0.7f;
    static constexpr float DEFAULT_AGE_THRESHOLD = 0.6f;
    static constexpr float DEFAULT_QUALITY_THRESHOLD = 0.5f;
    static constexpr int DEFAULT_MAX_DETECT_NUM = 20;
    static constexpr int DEFAULT_DETECT_PIXEL_LEVEL = 160;
    static constexpr int MIN_CROP_SIZE = 64;
};

} // namespace AISecurityVision

#endif // INSIGHTFACE_ANALYZER_H
