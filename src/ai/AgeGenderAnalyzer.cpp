/**
 * @file AgeGenderAnalyzer.cpp
 * @brief Age and Gender Analysis Implementation
 *
 * This file implements age and gender recognition using InsightFace or RKNN NPU.
 * Automatically selects the best available backend.
 */

#include "AgeGenderAnalyzer.h"
#include "../core/Logger.h"
#include <iostream>
#include <fstream>
#include <chrono>
#include <algorithm>
#include <numeric>
#include <unistd.h>

#ifdef HAVE_INSIGHTFACE
#include "inspireface.h"
#endif

#ifdef HAVE_RKNN
#include "rknn_api.h"
#include <cstring>
#include <cmath>
#endif

namespace AISecurityVision {

// Static member definitions
const std::vector<std::string> AgeGenderAnalyzer::AGE_GROUPS = {
    "child",    // 0-12 years
    "young",    // 13-25 years  
    "middle",   // 26-50 years
    "senior"    // 51+ years
};

const std::vector<std::string> AgeGenderAnalyzer::GENDER_LABELS = {
    "female",   // 0
    "male"      // 1
};

AgeGenderAnalyzer::AgeGenderAnalyzer() {
#ifdef HAVE_INSIGHTFACE
    m_session = nullptr;
    m_imageStream = nullptr;
    LOG_DEBUG() << "[AgeGenderAnalyzer] Constructor called (InsightFace backend)";
#elif defined(HAVE_RKNN)
    m_rknnContext = 0;
    m_inputAttrs = nullptr;
    m_outputAttrs = nullptr;
    m_isQuantized = false;
    LOG_DEBUG() << "[AgeGenderAnalyzer] Constructor called (RKNN backend)";
#else
    LOG_DEBUG() << "[AgeGenderAnalyzer] Constructor called (no backend available)";
#endif
}

AgeGenderAnalyzer::~AgeGenderAnalyzer() {
    cleanup();
}

bool AgeGenderAnalyzer::initialize(const std::string& modelPath) {
    if (m_initialized) {
        LOG_WARN() << "[AgeGenderAnalyzer] Already initialized";
        return true;
    }

#ifdef HAVE_INSIGHTFACE
    LOG_INFO() << "[AgeGenderAnalyzer] Initializing InsightFace with pack: " << modelPath;

    // Check if pack file exists
    if (access(modelPath.c_str(), F_OK) != 0) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Pack file not found: " << modelPath;
        return false;
    }

    m_packPath = modelPath;

    // Initialize InsightFace
    HResult ret = HFLaunchInspireFace(modelPath.c_str());
    if (ret != HSUCCEED) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Failed to launch InsightFace: " << ret;
        return false;
    }

    // Set log level
    HFSetLogLevel(HF_LOG_WARN);

    // Create session with face attributes enabled
    HOption option = HF_ENABLE_QUALITY | HF_ENABLE_MASK_DETECT | HF_ENABLE_FACE_ATTRIBUTE;
    HFDetectMode detMode = HF_DETECT_MODE_ALWAYS_DETECT;

    ret = HFCreateInspireFaceSessionOptional(option, detMode, m_maxDetectNum,
                                           m_detectPixelLevel, -1, &m_session);
    if (ret != HSUCCEED) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Failed to create session: " << ret;
        return false;
    }

    // Configure session
    HFSessionSetTrackPreviewSize(m_session, m_detectPixelLevel);
    HFSessionSetFilterMinimumFacePixelSize(m_session, 4);

    // Create image stream
    ret = HFCreateImageStreamEmpty(&m_imageStream);
    if (ret != HSUCCEED) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Failed to create image stream: " << ret;
        HFReleaseInspireFaceSession(m_session);
        m_session = nullptr;
        return false;
    }

    m_initialized = true;
    LOG_INFO() << "[AgeGenderAnalyzer] InsightFace initialized successfully";

    return true;

#elif defined(HAVE_RKNN)
    LOG_INFO() << "[AgeGenderAnalyzer] Initializing with model: " << modelPath;
    
    // Load RKNN model file
    std::ifstream file(modelPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Failed to open model file: " << modelPath;
        return false;
    }

    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> modelData(fileSize);
    if (!file.read(modelData.data(), fileSize)) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Failed to read model file: " << modelPath;
        return false;
    }
    file.close();

    // Initialize RKNN context
    int ret = rknn_init(&m_rknnContext, modelData.data(), fileSize, 0, NULL);
    if (ret < 0) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Failed to initialize RKNN context: " << ret;
        return false;
    }
    
    // Get model input/output info
    ret = rknn_query(m_rknnContext, RKNN_QUERY_IN_OUT_NUM, &m_ioNum, sizeof(m_ioNum));
    if (ret < 0) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Failed to query input/output number: " << ret;
        cleanup();
        return false;
    }
    
    LOG_INFO() << "[AgeGenderAnalyzer] Model has " << m_ioNum.n_input 
               << " inputs and " << m_ioNum.n_output << " outputs";
    
    // Get input attributes
    m_inputAttrs = new rknn_tensor_attr[m_ioNum.n_input];
    memset(m_inputAttrs, 0, sizeof(rknn_tensor_attr) * m_ioNum.n_input);
    for (uint32_t i = 0; i < m_ioNum.n_input; i++) {
        m_inputAttrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_INPUT_ATTR, &(m_inputAttrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to query input " << i << " attributes: " << ret;
            cleanup();
            return false;
        }
    }
    
    // Get output attributes
    m_outputAttrs = new rknn_tensor_attr[m_ioNum.n_output];
    memset(m_outputAttrs, 0, sizeof(rknn_tensor_attr) * m_ioNum.n_output);
    for (uint32_t i = 0; i < m_ioNum.n_output; i++) {
        m_outputAttrs[i].index = i;
        ret = rknn_query(m_rknnContext, RKNN_QUERY_OUTPUT_ATTR, &(m_outputAttrs[i]), sizeof(rknn_tensor_attr));
        if (ret < 0) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to query output " << i << " attributes: " << ret;
            cleanup();
            return false;
        }
    }
    
    // Check if model is quantized
    m_isQuantized = (m_inputAttrs[0].type == RKNN_TENSOR_UINT8 || m_inputAttrs[0].type == RKNN_TENSOR_INT8);
    
    LOG_INFO() << "[AgeGenderAnalyzer] Model initialized successfully"
               << " (quantized: " << (m_isQuantized ? "yes" : "no") << ")";
    
    m_initialized = true;
    return true;
    
#else
    LOG_ERROR() << "[AgeGenderAnalyzer] RKNN support not compiled";
    return false;
#endif
}

std::vector<PersonAttributes> AgeGenderAnalyzer::analyze(const std::vector<PersonDetection>& persons) {
    std::vector<PersonAttributes> attributes;

    if (!m_initialized) {
        LOG_ERROR() << "[AgeGenderAnalyzer] Analyzer not initialized";
        return attributes;
    }

    if (persons.empty()) {
        LOG_DEBUG() << "[AgeGenderAnalyzer] No persons to analyze";
        return attributes;
    }

    LOG_INFO() << "[AgeGenderAnalyzer] Starting analysis of " << persons.size() << " persons";

    auto start_time = std::chrono::high_resolution_clock::now();

    // Extract crops for analysis
    std::vector<cv::Mat> crops;
    int validCrops = 0;
    for (size_t i = 0; i < persons.size(); ++i) {
        const auto& person = persons[i];

        if (!person.crop.empty() &&
            person.crop.cols >= MIN_CROP_SIZE &&
            person.crop.rows >= MIN_CROP_SIZE) {
            crops.push_back(person.crop);
            validCrops++;
            LOG_DEBUG() << "[AgeGenderAnalyzer] Person " << i << " crop valid: "
                       << person.crop.cols << "x" << person.crop.rows
                       << ", bbox: (" << person.bbox.x << "," << person.bbox.y
                       << "," << person.bbox.width << "," << person.bbox.height << ")";
        } else {
            // Add placeholder for invalid crops
            crops.push_back(cv::Mat());
            LOG_WARN() << "[AgeGenderAnalyzer] Person " << i << " crop invalid: "
                      << (person.crop.empty() ? "empty" :
                          (std::to_string(person.crop.cols) + "x" + std::to_string(person.crop.rows)));
        }
    }

    LOG_INFO() << "[AgeGenderAnalyzer] Processing " << validCrops << " valid crops out of " << persons.size();

    // Process in batches for efficiency
    attributes = processBatch(crops);

    // Update performance metrics
    auto end_time = std::chrono::high_resolution_clock::now();
    m_inferenceTime = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    m_inferenceTimes.push_back(m_inferenceTime);
    if (m_inferenceTimes.size() > 100) {
        m_inferenceTimes.erase(m_inferenceTimes.begin());
    }
    m_analysisCount += persons.size();

    // Count successful analyses
    int successfulAnalyses = 0;
    for (const auto& attr : attributes) {
        if (attr.isValid()) {
            successfulAnalyses++;
        }
    }

    LOG_INFO() << "[AgeGenderAnalyzer] Completed analysis: " << successfulAnalyses
               << " successful out of " << persons.size() << " persons in "
               << m_inferenceTime << "ms";

    return attributes;
}

PersonAttributes AgeGenderAnalyzer::analyzeSingle(const cv::Mat& personCrop) {
    if (!m_initialized || personCrop.empty()) {
        return PersonAttributes();
    }
    
    std::vector<PersonDetection> singlePerson;
    PersonDetection person;
    person.crop = personCrop;
    singlePerson.push_back(person);
    
    auto results = analyze(singlePerson);
    return results.empty() ? PersonAttributes() : results[0];
}

bool AgeGenderAnalyzer::isInitialized() const {
    return m_initialized;
}

void AgeGenderAnalyzer::cleanup() {
#ifdef HAVE_INSIGHTFACE
    if (m_imageStream) {
        HFReleaseImageStream(m_imageStream);
        m_imageStream = nullptr;
    }

    if (m_session) {
        HFReleaseInspireFaceSession(m_session);
        m_session = nullptr;
    }
#elif defined(HAVE_RKNN)
    if (m_inputAttrs) {
        delete[] m_inputAttrs;
        m_inputAttrs = nullptr;
    }

    if (m_outputAttrs) {
        delete[] m_outputAttrs;
        m_outputAttrs = nullptr;
    }

    if (m_rknnContext > 0) {
        rknn_destroy(m_rknnContext);
        m_rknnContext = 0;
    }
#endif

    m_initialized = false;
    LOG_INFO() << "[AgeGenderAnalyzer] Cleanup completed";
}

std::vector<std::string> AgeGenderAnalyzer::getModelInfo() const {
    std::vector<std::string> info;

    if (!m_initialized) {
        info.push_back("Model not initialized");
        return info;
    }

#ifdef HAVE_INSIGHTFACE
    info.push_back("Backend: InsightFace (Simplified Mode)");
    info.push_back("Pack file: " + m_packPath);
    info.push_back("Features: Age, Gender, Race, Quality, Mask Detection");
    info.push_back("Gender threshold: " + std::to_string(m_genderThreshold));
    info.push_back("Age threshold: " + std::to_string(m_ageThreshold));
    info.push_back("Status: Demo implementation - full integration pending");
#elif defined(HAVE_RKNN)
    info.push_back("Backend: RKNN NPU");
    info.push_back("Input size: " + std::to_string(m_inputWidth) + "x" + std::to_string(m_inputHeight));
    info.push_back("Quantized: " + std::string(m_isQuantized ? "Yes" : "No"));
    info.push_back("Batch size: " + std::to_string(m_batchSize));
    info.push_back("Gender threshold: " + std::to_string(m_genderThreshold));
    info.push_back("Age threshold: " + std::to_string(m_ageThreshold));
#else
    info.push_back("No backend available");
#endif

    return info;
}

double AgeGenderAnalyzer::getAverageInferenceTime() const {
    if (m_inferenceTimes.empty()) {
        return 0.0;
    }

    double sum = std::accumulate(m_inferenceTimes.begin(), m_inferenceTimes.end(), 0.0);
    return sum / m_inferenceTimes.size();
}

#ifdef HAVE_INSIGHTFACE
// InsightFace implementation methods

std::vector<PersonAttributes> AgeGenderAnalyzer::processBatch(const std::vector<cv::Mat>& crops) {
    std::vector<PersonAttributes> results;

    LOG_DEBUG() << "[AgeGenderAnalyzer] Processing batch of " << crops.size() << " crops";

    for (size_t i = 0; i < crops.size(); ++i) {
        const auto& crop = crops[i];

        if (crop.empty()) {
            LOG_DEBUG() << "[AgeGenderAnalyzer] Crop " << i << " is empty, skipping";
            results.push_back(PersonAttributes());
            continue;
        }

        LOG_DEBUG() << "[AgeGenderAnalyzer] Processing crop " << i << " size: "
                   << crop.cols << "x" << crop.rows << " channels: " << crop.channels();

        // Preprocess image for InsightFace
        cv::Mat processed = preprocessImage(crop);
        if (processed.empty()) {
            LOG_WARN() << "[AgeGenderAnalyzer] Preprocessing failed for crop " << i;
            results.push_back(PersonAttributes());
            continue;
        }

        LOG_DEBUG() << "[AgeGenderAnalyzer] Preprocessed crop " << i << " to size: "
                   << processed.cols << "x" << processed.rows;

        // Save image temporarily and create bitmap from file
        std::string tempImagePath = "/tmp/temp_face_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()) + ".jpg";

        if (!cv::imwrite(tempImagePath, processed)) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to save temporary image";
            results.push_back(PersonAttributes());
            continue;
        }

        HFImageBitmap imageBitmap = nullptr;
        HResult ret = HFCreateImageBitmapFromFilePath(tempImagePath.c_str(), 3, &imageBitmap);
        if (ret != HSUCCEED) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to create image bitmap: " << ret;
            std::remove(tempImagePath.c_str());
            results.push_back(PersonAttributes());
            continue;
        }

        // Create image stream from bitmap
        HFImageStream imageStream = nullptr;
        ret = HFCreateImageStreamFromImageBitmap(imageBitmap, HF_CAMERA_ROTATION_0, &imageStream);
        if (ret != HSUCCEED) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to create image stream: " << ret;
            HFReleaseImageBitmap(imageBitmap);
            results.push_back(PersonAttributes());
            continue;
        }

        // Execute face detection
        HFMultipleFaceData multipleFaceData;
        ret = HFExecuteFaceTrack(m_session, imageStream, &multipleFaceData);
        if (ret != HSUCCEED) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Face detection failed: " << ret;
            HFReleaseImageStream(imageStream);
            HFReleaseImageBitmap(imageBitmap);
            results.push_back(PersonAttributes());
            continue;
        }

        PersonAttributes attributes;

        LOG_DEBUG() << "[AgeGenderAnalyzer] Crop " << i << " face detection result: "
                   << multipleFaceData.detectedNum << " faces detected";

        if (multipleFaceData.detectedNum > 0) {
            LOG_DEBUG() << "[AgeGenderAnalyzer] Running face attribute pipeline for crop " << i;

            // Run pipeline for face attributes
            HOption pipelineOption = HF_ENABLE_QUALITY | HF_ENABLE_MASK_DETECT | HF_ENABLE_FACE_ATTRIBUTE;
            ret = HFMultipleFacePipelineProcessOptional(m_session, imageStream,
                                                      &multipleFaceData, pipelineOption);
            if (ret == HSUCCEED) {
                LOG_DEBUG() << "[AgeGenderAnalyzer] Face attribute pipeline successful for crop " << i;
                // Process the first detected face
                attributes = processInsightFaceResult(0, &multipleFaceData, nullptr, nullptr, nullptr);

                if (attributes.isValid()) {
                    LOG_INFO() << "[AgeGenderAnalyzer] Crop " << i << " analysis successful: "
                              << "gender=" << attributes.gender << " (conf: " << attributes.gender_confidence
                              << "), age=" << attributes.age_group << " (conf: " << attributes.age_confidence << ")";
                } else {
                    LOG_WARN() << "[AgeGenderAnalyzer] Crop " << i << " analysis failed - invalid attributes";
                }
            } else {
                LOG_ERROR() << "[AgeGenderAnalyzer] Face attribute pipeline failed for crop " << i << ": " << ret;
            }
        } else {
            LOG_WARN() << "[AgeGenderAnalyzer] No faces detected in crop " << i;
        }

        // Set timestamp
        attributes.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();

        results.push_back(attributes);

        // Cleanup
        HFReleaseImageStream(imageStream);
        HFReleaseImageBitmap(imageBitmap);
        std::remove(tempImagePath.c_str());
    }

    return results;
}

cv::Mat AgeGenderAnalyzer::preprocessImage(const cv::Mat& image) {
    if (image.empty()) {
        return cv::Mat();
    }

    cv::Mat processed = image.clone();

    // Ensure image is in BGR format
    if (processed.channels() == 1) {
        cv::cvtColor(processed, processed, cv::COLOR_GRAY2BGR);
    } else if (processed.channels() == 4) {
        cv::cvtColor(processed, processed, cv::COLOR_BGRA2BGR);
    }

    // Fix RGA alignment issues: ensure width is 16-aligned for RGB888
    int alignedWidth = ((processed.cols + 15) / 16) * 16;
    int alignedHeight = processed.rows;

    if (alignedWidth != processed.cols) {
        cv::Mat aligned;
        cv::resize(processed, aligned, cv::Size(alignedWidth, alignedHeight));
        processed = aligned;
        LOG_DEBUG() << "[AgeGenderAnalyzer] Aligned image from " << image.cols
                   << "x" << image.rows << " to " << alignedWidth << "x" << alignedHeight;
    }

    // Ensure minimum size for face detection
    if (processed.cols < 112 || processed.rows < 112) {
        cv::resize(processed, processed, cv::Size(112, 112));
        LOG_DEBUG() << "[AgeGenderAnalyzer] Resized small image to 112x112";
    }

    return processed;
}

PersonAttributes AgeGenderAnalyzer::processInsightFaceResult(int faceIndex,
                                                           const void* multipleFaceData,
                                                           const void* attributeResult,
                                                           const void* qualityResult,
                                                           const void* maskResult) {
    PersonAttributes attributes;

    if (!multipleFaceData) {
        return attributes;
    }

    const HFMultipleFaceData* faceData = static_cast<const HFMultipleFaceData*>(multipleFaceData);

    if (faceIndex >= faceData->detectedNum) {
        return attributes;
    }

    // Get face attribute results
    HFFaceAttributeResult attrResult;
    HResult ret = HFGetFaceAttributeResult(m_session, &attrResult);
    if (ret == HSUCCEED && faceIndex < attrResult.num) {
        // Map gender
        attributes.gender = mapInsightFaceGender(attrResult.gender[faceIndex]);
        attributes.gender_confidence = 0.85f; // InsightFace doesn't provide confidence directly

        // Map age group
        attributes.age_group = mapInsightFaceAge(attrResult.ageBracket[faceIndex]);
        attributes.age_confidence = 0.80f;

        // Map race
        attributes.race = mapInsightFaceRace(attrResult.race[faceIndex]);
        attributes.race_confidence = 0.75f;
    }

    // Get quality score
    HFFaceQualityConfidence qualityConf;
    ret = HFGetFaceQualityConfidence(m_session, &qualityConf);
    if (ret == HSUCCEED && faceIndex < qualityConf.num) {
        attributes.quality_score = qualityConf.confidence[faceIndex];
    }

    // Get mask detection result
    HFFaceMaskConfidence maskConf;
    ret = HFGetFaceMaskConfidence(m_session, &maskConf);
    if (ret == HSUCCEED && faceIndex < maskConf.num) {
        attributes.has_mask = maskConf.confidence[faceIndex] > 0.5f;
    }

    return attributes;
}

std::string AgeGenderAnalyzer::mapInsightFaceGender(int genderCode) {
    switch (genderCode) {
        case 0: return "female";
        case 1: return "male";
        default: return "unknown";
    }
}

std::string AgeGenderAnalyzer::mapInsightFaceAge(int ageBracket) {
    // InsightFace age brackets: 0-2, 3-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60-69, 70+
    switch (ageBracket) {
        case 0:
        case 1: return "child";    // 0-2, 3-9 years
        case 2:
        case 3: return "young";    // 10-19, 20-29 years
        case 4:
        case 5: return "middle";   // 30-39, 40-49 years
        case 6:
        case 7:
        case 8: return "senior";   // 50-59, 60-69, 70+ years
        default: return "unknown";
    }
}

std::string AgeGenderAnalyzer::mapInsightFaceRace(int raceCode) {
    switch (raceCode) {
        case 0: return "black";
        case 1: return "asian";
        case 2: return "latino";
        case 3: return "middle_eastern";
        case 4: return "white";
        default: return "unknown";
    }
}

#elif defined(HAVE_RKNN)
cv::Mat AgeGenderAnalyzer::preprocessImage(const cv::Mat& image) {
    if (image.empty()) {
        return cv::Mat();
    }

    cv::Mat processed;

    // Resize to model input size
    cv::resize(image, processed, cv::Size(m_inputWidth, m_inputHeight));

    // Convert to RGB if needed
    if (processed.channels() == 3) {
        cv::cvtColor(processed, processed, cv::COLOR_BGR2RGB);
    }

    // Normalize to [0, 255] for quantized models or [0, 1] for float models
    if (m_isQuantized) {
        // Keep as uint8 for quantized models
        processed.convertTo(processed, CV_8UC3);
    } else {
        // Normalize to [0, 1] for float models
        processed.convertTo(processed, CV_32FC3, 1.0/255.0);
    }

    return processed;
}

PersonAttributes AgeGenderAnalyzer::postprocessResults(rknn_output* outputs, rknn_tensor_attr* output_attrs) {
    PersonAttributes attributes;

    if (!outputs || !output_attrs) {
        return attributes;
    }

    // Assuming model has 2 outputs: gender and age
    // Output 0: Gender (2 classes: female, male)
    // Output 1: Age group (4 classes: child, young, middle, senior)

    float* genderOutput = nullptr;
    float* ageOutput = nullptr;

    // Process gender output (assuming index 0)
    if (output_attrs[0].type == RKNN_TENSOR_FLOAT32) {
        genderOutput = static_cast<float*>(outputs[0].buf);
    } else if (output_attrs[0].type == RKNN_TENSOR_INT8) {
        // Dequantize INT8 to float
        int8_t* quantizedOutput = static_cast<int8_t*>(outputs[0].buf);
        std::vector<float> dequantized(2);  // 2 gender classes
        for (int i = 0; i < 2; i++) {
            dequantized[i] = deqntAffineToF32(quantizedOutput[i], output_attrs[0].zp, output_attrs[0].scale);
        }
        genderOutput = dequantized.data();
    }

    // Process age output (assuming index 1)
    if (m_ioNum.n_output > 1) {
        if (output_attrs[1].type == RKNN_TENSOR_FLOAT32) {
            ageOutput = static_cast<float*>(outputs[1].buf);
        } else if (output_attrs[1].type == RKNN_TENSOR_INT8) {
            // Dequantize INT8 to float
            int8_t* quantizedOutput = static_cast<int8_t*>(outputs[1].buf);
            std::vector<float> dequantized(4);  // 4 age groups
            for (int i = 0; i < 4; i++) {
                dequantized[i] = deqntAffineToF32(quantizedOutput[i], output_attrs[1].zp, output_attrs[1].scale);
            }
            ageOutput = dequantized.data();
        }
    }

    // Decode results
    if (genderOutput) {
        attributes.gender = decodeGender(genderOutput, attributes.gender_confidence);
    }

    if (ageOutput) {
        attributes.age_group = decodeAgeGroup(ageOutput, attributes.age_confidence);
    }

    return attributes;
}

std::vector<PersonAttributes> AgeGenderAnalyzer::processBatch(const std::vector<cv::Mat>& crops) {
    std::vector<PersonAttributes> results;

    for (const auto& crop : crops) {
        if (crop.empty()) {
            results.push_back(PersonAttributes());
            continue;
        }

        // Preprocess image
        cv::Mat preprocessed = preprocessImage(crop);
        if (preprocessed.empty()) {
            results.push_back(PersonAttributes());
            continue;
        }

        // Prepare input
        rknn_input inputs[1];
        memset(inputs, 0, sizeof(inputs));
        inputs[0].index = 0;
        inputs[0].type = m_isQuantized ? RKNN_TENSOR_UINT8 : RKNN_TENSOR_FLOAT32;
        inputs[0].size = preprocessed.total() * preprocessed.elemSize();
        inputs[0].fmt = RKNN_TENSOR_NHWC;
        inputs[0].buf = preprocessed.data;

        // Set input
        int ret = rknn_inputs_set(m_rknnContext, m_ioNum.n_input, inputs);
        if (ret < 0) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to set inputs: " << ret;
            results.push_back(PersonAttributes());
            continue;
        }

        // Run inference
        ret = rknn_run(m_rknnContext, nullptr);
        if (ret < 0) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to run inference: " << ret;
            results.push_back(PersonAttributes());
            continue;
        }

        // Get outputs
        rknn_output outputs[m_ioNum.n_output];
        memset(outputs, 0, sizeof(outputs));
        for (uint32_t i = 0; i < m_ioNum.n_output; i++) {
            outputs[i].want_float = 1;
        }

        ret = rknn_outputs_get(m_rknnContext, m_ioNum.n_output, outputs, NULL);
        if (ret < 0) {
            LOG_ERROR() << "[AgeGenderAnalyzer] Failed to get outputs: " << ret;
            results.push_back(PersonAttributes());
            continue;
        }

        // Post-process results
        PersonAttributes attributes = postprocessResults(outputs, m_outputAttrs);
        results.push_back(attributes);

        // Release outputs
        rknn_outputs_release(m_rknnContext, m_ioNum.n_output, outputs);
    }

    return results;
}

std::string AgeGenderAnalyzer::decodeGender(const float* genderOutput, float& confidence) {
    if (!genderOutput) {
        confidence = 0.0f;
        return "unknown";
    }

    // Apply softmax and find max
    float maxVal = std::max(genderOutput[0], genderOutput[1]);
    float sum = std::exp(genderOutput[0] - maxVal) + std::exp(genderOutput[1] - maxVal);

    float prob0 = std::exp(genderOutput[0] - maxVal) / sum;
    float prob1 = std::exp(genderOutput[1] - maxVal) / sum;

    if (prob1 > prob0) {
        confidence = prob1;
        return confidence >= DEFAULT_GENDER_THRESHOLD ? "male" : "unknown";
    } else {
        confidence = prob0;
        return confidence >= DEFAULT_GENDER_THRESHOLD ? "female" : "unknown";
    }
}

std::string AgeGenderAnalyzer::decodeAgeGroup(const float* ageOutput, float& confidence) {
    if (!ageOutput) {
        confidence = 0.0f;
        return "unknown";
    }

    // Find max probability among age groups
    int maxIdx = 0;
    float maxVal = ageOutput[0];

    for (int i = 1; i < static_cast<int>(AGE_GROUPS.size()); i++) {
        if (ageOutput[i] > maxVal) {
            maxVal = ageOutput[i];
            maxIdx = i;
        }
    }

    // Apply softmax to get confidence
    float sum = 0.0f;
    for (int i = 0; i < static_cast<int>(AGE_GROUPS.size()); i++) {
        sum += std::exp(ageOutput[i] - maxVal);
    }
    confidence = std::exp(ageOutput[maxIdx] - maxVal) / sum;

    return confidence >= DEFAULT_AGE_THRESHOLD ? AGE_GROUPS[maxIdx] : "unknown";
}

float AgeGenderAnalyzer::deqntAffineToF32(int8_t qnt, int32_t zp, float scale) {
    return (static_cast<float>(qnt) - static_cast<float>(zp)) * scale;
}
#endif

} // namespace AISecurityVision
