/**
 * @file AgeGenderAnalyzer.h
 * @brief Age and Gender Analysis for Person Detection
 *
 * This file implements age and gender recognition using InsightFace library.
 * Designed as a non-intrusive extension to the existing AI vision system.
 */

#ifndef AGE_GENDER_ANALYZER_H
#define AGE_GENDER_ANALYZER_H

#include <vector>
#include <string>
#include <memory>
#include <opencv2/opencv.hpp>
#include "PersonFilter.h"

// Forward declarations for InsightFace C API
#ifdef HAVE_INSIGHTFACE
#include "inspireface.h"
#else
typedef void* HFSession;
typedef void* HFImageStream;
typedef void* HFImageBitmap;
typedef int HResult;
typedef int HOption;
#endif

#ifdef HAVE_RKNN
#include "rknn_api.h"
#endif

namespace AISecurityVision {

/**
 * @brief Person attributes structure with enhanced InsightFace capabilities
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

    PersonAttributes(const std::string& g, const std::string& a, float gc = 0.0f, float ac = 0.0f)
        : gender(g), age_group(a), gender_confidence(gc), age_confidence(ac) {}

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
 * @brief Age and Gender analyzer using InsightFace
 *
 * This class implements comprehensive face analysis using InsightFace library
 * including age, gender, race recognition, quality assessment, and mask detection.
 */
class AgeGenderAnalyzer {
public:
    AgeGenderAnalyzer();
    virtual ~AgeGenderAnalyzer();

    /**
     * @brief Initialize the analyzer with InsightFace model pack
     * @param packPath Path to the InsightFace model pack file
     * @return true if initialization successful, false otherwise
     */
    bool initialize(const std::string& packPath = "models/Pikachu.pack");
    
    /**
     * @brief Analyze age and gender for multiple persons
     * @param persons Vector of person detections with crops
     * @return Vector of person attributes
     */
    std::vector<PersonAttributes> analyze(const std::vector<PersonDetection>& persons);
    
    /**
     * @brief Analyze age and gender for a single person
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
    void setBatchSize(int batchSize) { m_batchSize = batchSize; }
    
    float getGenderThreshold() const { return m_genderThreshold; }
    float getAgeThreshold() const { return m_ageThreshold; }
    int getBatchSize() const { return m_batchSize; }
    
    // Performance metrics
    double getLastInferenceTime() const { return m_inferenceTime; }
    double getAverageInferenceTime() const;
    size_t getAnalysisCount() const { return m_analysisCount; }

private:
#ifdef HAVE_INSIGHTFACE
    // InsightFace session and handles
    HFSession m_session;
    HFImageStream m_imageStream;
    std::string m_packPath;

    // Configuration
    float m_qualityThreshold = 0.5f;
    int m_maxDetectNum = 20;
    int m_detectPixelLevel = 160;

    // Helper methods for InsightFace
    PersonAttributes processInsightFaceResult(int faceIndex,
                                            const void* multipleFaceData,
                                            const void* attributeResult,
                                            const void* qualityResult = nullptr,
                                            const void* maskResult = nullptr);
    std::string mapInsightFaceGender(int genderCode);
    std::string mapInsightFaceAge(int ageBracket);
    std::string mapInsightFaceRace(int raceCode);
    cv::Mat preprocessImage(const cv::Mat& image);
    std::vector<PersonAttributes> processBatch(const std::vector<cv::Mat>& crops);

#elif defined(HAVE_RKNN)
    // RKNN context and attributes
    rknn_context m_rknnContext;
    rknn_input_output_num m_ioNum;
    rknn_tensor_attr* m_inputAttrs;
    rknn_tensor_attr* m_outputAttrs;

    // Model properties
    bool m_isQuantized;
    int m_inputWidth = 224;   // MobileNetV2 input size
    int m_inputHeight = 224;
    int m_inputChannels = 3;

    // Helper methods
    cv::Mat preprocessImage(const cv::Mat& image);
    PersonAttributes postprocessResults(rknn_output* outputs, rknn_tensor_attr* output_attrs);
    std::vector<PersonAttributes> processBatch(const std::vector<cv::Mat>& crops);

    // Utility functions
    static std::string decodeGender(const float* genderOutput, float& confidence);
    static std::string decodeAgeGroup(const float* ageOutput, float& confidence);
    static float deqntAffineToF32(int8_t qnt, int32_t zp, float scale);
#endif
    
    // Configuration
    float m_genderThreshold = 0.7f;
    float m_ageThreshold = 0.6f;
    int m_batchSize = 4;  // Optimal batch size for RK3588
    
    // State
    bool m_initialized = false;
    
    // Performance tracking
    double m_inferenceTime = 0.0;
    std::vector<double> m_inferenceTimes;
    size_t m_analysisCount = 0;
    
    // Age group mappings
    static const std::vector<std::string> AGE_GROUPS;
    static const std::vector<std::string> GENDER_LABELS;
    
    // Constants
    static constexpr float DEFAULT_GENDER_THRESHOLD = 0.7f;
    static constexpr float DEFAULT_AGE_THRESHOLD = 0.6f;
    static constexpr int DEFAULT_BATCH_SIZE = 4;
    static constexpr int MIN_CROP_SIZE = 64;
};

} // namespace AISecurityVision

#endif // AGE_GENDER_ANALYZER_H
