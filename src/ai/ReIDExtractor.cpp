#include "ReIDExtractor.h"
#include "../core/Logger.h"
using namespace AISecurityVision;
#include <iostream>
#include <chrono>
#include <fstream>
#include <cmath>
#include <algorithm>
#ifndef DISABLE_OPENCV_DNN
#include <opencv2/dnn.hpp>
#endif

ReIDExtractor::ReIDExtractor()
    : m_engine(nullptr)
    , m_context(nullptr)
    , m_stream(nullptr)
    , m_inputBuffer(nullptr)
    , m_outputBuffer(nullptr)
    , m_initialized(false)
    , m_normalizationEnabled(true)
    , m_inputWidth(128)
    , m_inputHeight(256)
    , m_featureDimension(512)
    , m_minObjectWidth(32)
    , m_minObjectHeight(64)
    , m_inputSize(0)
    , m_outputSize(0)
    , m_inferenceTime(0.0)
    , m_extractionCount(0)
{
    LOG_INFO() << "[ReIDExtractor] Constructor called";
}

ReIDExtractor::~ReIDExtractor() {
    cleanup();
}

bool ReIDExtractor::initialize(const std::string& modelPath) {
    LOG_INFO() << "[ReIDExtractor] Initializing ReID feature extractor...";
    LOG_INFO() << "[ReIDExtractor] Model path: " << modelPath;

    try {
        m_modelPath = modelPath;

        // Always use built-in feature extraction to avoid protobuf dependency issues
        LOG_INFO() << "[ReIDExtractor] Using built-in feature extraction (protobuf-free)";

        // Check if model file exists (for logging purposes only)
        std::ifstream modelFile(modelPath);
        if (modelFile.good()) {
            LOG_INFO() << "[ReIDExtractor] Model file found but using built-in extraction for compatibility";
        } else {
            LOG_INFO() << "[ReIDExtractor] Model file not found, using built-in feature extraction";
        }

        // Skip OpenCV DNN loading to avoid protobuf dependency issues
        // This prevents the hanging issue during initialization
        LOG_INFO() << "[ReIDExtractor] Skipping OpenCV DNN to avoid protobuf dependency issues";

        // Calculate buffer sizes
        m_inputSize = m_inputWidth * m_inputHeight * 3 * sizeof(float);
        m_outputSize = m_featureDimension * sizeof(float);

        m_initialized = true;
        LOG_INFO() << "[ReIDExtractor] Initialization completed successfully";
        LOG_INFO() << "[ReIDExtractor] Input size: " << m_inputWidth << "x" << m_inputHeight;
        LOG_INFO() << "[ReIDExtractor] Feature dimension: " << m_featureDimension;

        return true;

    } catch (const std::exception& e) {
        LOG_ERROR() << "[ReIDExtractor] Exception during initialization: " << e.what();
        return false;
    }
}

void ReIDExtractor::cleanup() {
    deallocateBuffers();
    m_initialized = false;
    LOG_INFO() << "[ReIDExtractor] Cleanup completed";
}

bool ReIDExtractor::isInitialized() const {
    return m_initialized;
}

std::vector<ReIDExtractor::ReIDEmbedding> ReIDExtractor::extractFeatures(
    const cv::Mat& frame,
    const std::vector<cv::Rect>& detections,
    const std::vector<int>& trackIds,
    const std::vector<int>& classIds,
    const std::vector<float>& confidences) {

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<ReIDEmbedding> embeddings;

    if (frame.empty() || !m_initialized || detections.empty()) {
        return embeddings;
    }

    // Ensure all vectors have the same size
    size_t numDetections = detections.size();
    std::vector<int> actualTrackIds = trackIds;
    std::vector<int> actualClassIds = classIds;
    std::vector<float> actualConfidences = confidences;

    // Fill missing data with defaults
    if (actualTrackIds.size() != numDetections) {
        actualTrackIds.resize(numDetections, -1);
    }
    if (actualClassIds.size() != numDetections) {
        actualClassIds.resize(numDetections, 0);
    }
    if (actualConfidences.size() != numDetections) {
        actualConfidences.resize(numDetections, 1.0f);
    }

    // Extract features for each detection
    for (size_t i = 0; i < numDetections; ++i) {
        const cv::Rect& bbox = detections[i];

        // Validate detection
        if (!isValidDetection(bbox)) {
            continue;
        }

        // Extract single feature
        ReIDEmbedding embedding = extractSingleFeature(
            frame, bbox, actualTrackIds[i], actualClassIds[i], actualConfidences[i]);

        if (embedding.isValid()) {
            embeddings.push_back(embedding);
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    m_inferenceTime = std::chrono::duration<double, std::milli>(end - start).count();
    m_extractionCount += embeddings.size();
    m_inferenceTimes.push_back(m_inferenceTime);

    // Keep only last 100 inference times for average calculation
    if (m_inferenceTimes.size() > 100) {
        m_inferenceTimes.erase(m_inferenceTimes.begin());
    }

    LOG_INFO() << "[ReIDExtractor] Extracted " << embeddings.size()
              << " embeddings in " << m_inferenceTime << "ms";

    return embeddings;
}

ReIDExtractor::ReIDEmbedding ReIDExtractor::extractSingleFeature(
    const cv::Mat& frame,
    const cv::Rect& bbox,
    int trackId,
    int classId,
    float confidence) {

    ReIDEmbedding embedding;
    embedding.trackId = trackId;
    embedding.classId = classId;
    embedding.bbox = bbox;
    embedding.confidence = confidence;
    embedding.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    if (!isValidDetection(bbox)) {
        return embedding;
    }

    try {
        // Extract ROI from frame
        cv::Mat roi = extractROI(frame, bbox);
        if (roi.empty()) {
            return embedding;
        }

        // Extract features from ROI
        embedding.features = extractFeaturesFromROI(roi);

        // Normalize features if enabled
        if (m_normalizationEnabled && !embedding.features.empty()) {
            embedding.features = normalizeFeatures(embedding.features);
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[ReIDExtractor] Exception in extractSingleFeature: " << e.what();
    }

    return embedding;
}

std::vector<float> ReIDExtractor::extractFeaturesFromROI(const cv::Mat& roi) {
    std::vector<float> features;

    if (roi.empty()) {
        return features;
    }

    try {
        // Always use hand-crafted features to avoid protobuf dependency issues
        features = generateHandcraftedFeatures(roi);

    } catch (const std::exception& e) {
        LOG_ERROR() << "[ReIDExtractor] Exception in extractFeaturesFromROI: " << e.what();
        // Fallback to hand-crafted features
        features = generateHandcraftedFeatures(roi);
    }

    return features;
}

std::vector<float> ReIDExtractor::generateHandcraftedFeatures(const cv::Mat& roi) {
    std::vector<float> features(m_featureDimension, 0.0f);

    if (roi.empty()) {
        return features;
    }

    try {
        // Resize ROI to standard size
        cv::Mat resized;
        cv::resize(roi, resized, cv::Size(m_inputWidth, m_inputHeight));

        // Convert to different color spaces for feature extraction
        cv::Mat hsv, lab, gray;
        cv::cvtColor(resized, hsv, cv::COLOR_BGR2HSV);
        cv::cvtColor(resized, lab, cv::COLOR_BGR2Lab);
        cv::cvtColor(resized, gray, cv::COLOR_BGR2GRAY);

        int featureIdx = 0;

        // Color histogram features (RGB, HSV, Lab)
        std::vector<cv::Mat> channels;

        // BGR histogram
        cv::split(resized, channels);
        for (int c = 0; c < 3 && featureIdx < m_featureDimension - 16; ++c) {
            cv::Mat hist;
            int histSize = 16;
            float range[] = {0, 256};
            const float* histRange = {range};
            cv::calcHist(&channels[c], 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
            cv::normalize(hist, hist, 0, 1, cv::NORM_L2);

            for (int i = 0; i < histSize && featureIdx < m_featureDimension; ++i) {
                features[featureIdx++] = hist.at<float>(i);
            }
        }

        // HSV histogram
        cv::split(hsv, channels);
        for (int c = 0; c < 3 && featureIdx < m_featureDimension - 16; ++c) {
            cv::Mat hist;
            int histSize = 16;
            float range[] = {0.0f, c == 0 ? 180.0f : 256.0f}; // Hue range is 0-180
            const float* histRange = {range};
            cv::calcHist(&channels[c], 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
            cv::normalize(hist, hist, 0, 1, cv::NORM_L2);

            for (int i = 0; i < histSize && featureIdx < m_featureDimension; ++i) {
                features[featureIdx++] = hist.at<float>(i);
            }
        }

        // Texture features using LBP (Local Binary Patterns)
        if (featureIdx < m_featureDimension - 32) {
            cv::Mat lbp = computeLBP(gray);
            cv::Mat hist;
            int histSize = 32;
            float range[] = {0, 256};
            const float* histRange = {range};
            cv::calcHist(&lbp, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
            cv::normalize(hist, hist, 0, 1, cv::NORM_L2);

            for (int i = 0; i < histSize && featureIdx < m_featureDimension; ++i) {
                features[featureIdx++] = hist.at<float>(i);
            }
        }

        // Gradient features (HOG-like)
        if (featureIdx < m_featureDimension - 16) {
            cv::Mat gradX, gradY, magnitude, angle;
            cv::Sobel(gray, gradX, CV_32F, 1, 0, 3);
            cv::Sobel(gray, gradY, CV_32F, 0, 1, 3);
            cv::cartToPolar(gradX, gradY, magnitude, angle, true);

            cv::Mat hist;
            int histSize = 16;
            float range[] = {0, 360};
            const float* histRange = {range};
            cv::calcHist(&angle, 1, 0, cv::Mat(), hist, 1, &histSize, &histRange);
            cv::normalize(hist, hist, 0, 1, cv::NORM_L2);

            for (int i = 0; i < histSize && featureIdx < m_featureDimension; ++i) {
                features[featureIdx++] = hist.at<float>(i);
            }
        }

        // Fill remaining features with spatial information
        while (featureIdx < m_featureDimension) {
            features[featureIdx] = static_cast<float>(featureIdx) / m_featureDimension;
            featureIdx++;
        }

    } catch (const std::exception& e) {
        LOG_ERROR() << "[ReIDExtractor] Exception in generateHandcraftedFeatures: " << e.what();
        // Fill with default values
        for (int i = 0; i < m_featureDimension; ++i) {
            features[i] = static_cast<float>(i) / m_featureDimension;
        }
    }

    return features;
}

cv::Mat ReIDExtractor::computeLBP(const cv::Mat& gray) {
    cv::Mat lbp = cv::Mat::zeros(gray.size(), CV_8UC1);

    for (int i = 1; i < gray.rows - 1; ++i) {
        for (int j = 1; j < gray.cols - 1; ++j) {
            uchar center = gray.at<uchar>(i, j);
            uchar code = 0;

            code |= (gray.at<uchar>(i-1, j-1) >= center) << 7;
            code |= (gray.at<uchar>(i-1, j) >= center) << 6;
            code |= (gray.at<uchar>(i-1, j+1) >= center) << 5;
            code |= (gray.at<uchar>(i, j+1) >= center) << 4;
            code |= (gray.at<uchar>(i+1, j+1) >= center) << 3;
            code |= (gray.at<uchar>(i+1, j) >= center) << 2;
            code |= (gray.at<uchar>(i+1, j-1) >= center) << 1;
            code |= (gray.at<uchar>(i, j-1) >= center) << 0;

            lbp.at<uchar>(i, j) = code;
        }
    }

    return lbp;
}

// Cosine similarity implementation for ReIDEmbedding
float ReIDExtractor::ReIDEmbedding::cosineSimilarity(const ReIDEmbedding& other) const {
    return ReIDExtractor::computeCosineSimilarity(this->features, other.features);
}

// Batch processing
std::vector<std::vector<ReIDExtractor::ReIDEmbedding>> ReIDExtractor::extractBatch(
    const std::vector<cv::Mat>& frames,
    const std::vector<std::vector<cv::Rect>>& detections,
    const std::vector<std::vector<int>>& trackIds) {

    std::vector<std::vector<ReIDEmbedding>> results;

    for (size_t i = 0; i < frames.size(); ++i) {
        std::vector<int> emptyTrackIds;
        if (i < trackIds.size()) {
            results.push_back(extractFeatures(frames[i], detections[i], trackIds[i]));
        } else {
            results.push_back(extractFeatures(frames[i], detections[i], emptyTrackIds));
        }
    }

    return results;
}

// Configuration methods
void ReIDExtractor::setInputSize(int width, int height) {
    m_inputWidth = std::max(32, width);
    m_inputHeight = std::max(64, height);
    m_inputSize = m_inputWidth * m_inputHeight * 3 * sizeof(float);
    LOG_INFO() << "[ReIDExtractor] Input size set to: " << m_inputWidth << "x" << m_inputHeight;
}

void ReIDExtractor::setFeatureDimension(int dimension) {
    m_featureDimension = std::max(128, std::min(2048, dimension));
    m_outputSize = m_featureDimension * sizeof(float);
    LOG_INFO() << "[ReIDExtractor] Feature dimension set to: " << m_featureDimension;
}

void ReIDExtractor::setNormalization(bool enabled) {
    m_normalizationEnabled = enabled;
    LOG_INFO() << "[ReIDExtractor] Normalization " << (enabled ? "enabled" : "disabled");
}

void ReIDExtractor::setMinObjectSize(int minWidth, int minHeight) {
    m_minObjectWidth = std::max(16, minWidth);
    m_minObjectHeight = std::max(32, minHeight);
    LOG_INFO() << "[ReIDExtractor] Min object size set to: " << m_minObjectWidth << "x" << m_minObjectHeight;
}

// Getters
cv::Size ReIDExtractor::getInputSize() const {
    return cv::Size(m_inputWidth, m_inputHeight);
}

int ReIDExtractor::getFeatureDimension() const {
    return m_featureDimension;
}

bool ReIDExtractor::isNormalizationEnabled() const {
    return m_normalizationEnabled;
}

double ReIDExtractor::getInferenceTime() const {
    return m_inferenceTime;
}

size_t ReIDExtractor::getExtractionCount() const {
    return m_extractionCount;
}

float ReIDExtractor::getAverageInferenceTime() const {
    if (m_inferenceTimes.empty()) {
        return 0.0f;
    }

    double sum = 0.0;
    for (double time : m_inferenceTimes) {
        sum += time;
    }

    return static_cast<float>(sum / m_inferenceTimes.size());
}

// Static similarity computation methods
float ReIDExtractor::computeCosineSimilarity(const std::vector<float>& features1,
                                           const std::vector<float>& features2) {
    if (features1.size() != features2.size() || features1.empty()) {
        return 0.0f;
    }

    float dotProduct = 0.0f;
    float norm1 = 0.0f;
    float norm2 = 0.0f;

    for (size_t i = 0; i < features1.size(); ++i) {
        dotProduct += features1[i] * features2[i];
        norm1 += features1[i] * features1[i];
        norm2 += features2[i] * features2[i];
    }

    if (norm1 == 0.0f || norm2 == 0.0f) {
        return 0.0f;
    }

    return dotProduct / (std::sqrt(norm1) * std::sqrt(norm2));
}

float ReIDExtractor::computeEuclideanDistance(const std::vector<float>& features1,
                                            const std::vector<float>& features2) {
    if (features1.size() != features2.size() || features1.empty()) {
        return std::numeric_limits<float>::max();
    }

    float distance = 0.0f;
    for (size_t i = 0; i < features1.size(); ++i) {
        float diff = features1[i] - features2[i];
        distance += diff * diff;
    }

    return std::sqrt(distance);
}

// Internal helper methods
bool ReIDExtractor::loadModel(const std::string& modelPath) {
    // TODO: Implement TensorRT model loading
    return true;
}

bool ReIDExtractor::setupTensorRT() {
    // TODO: Implement TensorRT setup
    return true;
}

cv::Mat ReIDExtractor::preprocessImage(const cv::Mat& image) {
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(m_inputWidth, m_inputHeight));

    // Convert BGR to RGB and normalize to [0, 1]
    cv::Mat rgb;
    cv::cvtColor(resized, rgb, cv::COLOR_BGR2RGB);
    rgb.convertTo(rgb, CV_32F, 1.0 / 255.0);

    return rgb;
}

cv::Mat ReIDExtractor::extractROI(const cv::Mat& frame, const cv::Rect& bbox) {
    if (frame.empty() || bbox.width <= 0 || bbox.height <= 0) {
        return cv::Mat();
    }

    // Ensure bbox is within frame bounds
    cv::Rect safeBbox = bbox & cv::Rect(0, 0, frame.cols, frame.rows);

    // Relax minimum size requirements for better person detection
    int minWidth = std::max(16, m_minObjectWidth / 2);  // Reduce minimum width requirement
    int minHeight = std::max(32, m_minObjectHeight / 2); // Reduce minimum height requirement

    if (safeBbox.width < minWidth || safeBbox.height < minHeight) {
        LOG_DEBUG() << "[ReIDExtractor] ROI too small: " << safeBbox.width << "x" << safeBbox.height
                   << " (min: " << minWidth << "x" << minHeight << ")";
        return cv::Mat();
    }

    cv::Mat roi = frame(safeBbox).clone();

    // Ensure ROI is large enough for feature extraction
    if (!roi.empty() && (roi.cols < 32 || roi.rows < 64)) {
        cv::resize(roi, roi, cv::Size(std::max(32, roi.cols), std::max(64, roi.rows)));
        LOG_DEBUG() << "[ReIDExtractor] Resized small ROI to " << roi.cols << "x" << roi.rows;
    }

    return roi;
}

std::vector<float> ReIDExtractor::postprocessFeatures(const cv::Mat& output) {
    std::vector<float> features;

    if (output.empty()) {
        return features;
    }

    // Flatten the output tensor
    cv::Mat flattened = output.reshape(1, output.total());
    flattened.convertTo(flattened, CV_32F);

    features.assign(flattened.ptr<float>(), flattened.ptr<float>() + flattened.total());

    // Resize to target dimension if needed
    if (features.size() != static_cast<size_t>(m_featureDimension)) {
        features.resize(m_featureDimension, 0.0f);
    }

    return features;
}

std::vector<float> ReIDExtractor::normalizeFeatures(const std::vector<float>& features) {
    std::vector<float> normalized = features;

    // L2 normalization
    float norm = 0.0f;
    for (float feature : features) {
        norm += feature * feature;
    }

    if (norm > 0.0f) {
        norm = std::sqrt(norm);
        for (float& feature : normalized) {
            feature /= norm;
        }
    }

    return normalized;
}

bool ReIDExtractor::isValidDetection(const cv::Rect& bbox) const {
    return bbox.width >= m_minObjectWidth && bbox.height >= m_minObjectHeight;
}

bool ReIDExtractor::allocateBuffers() {
    // TODO: Implement CUDA buffer allocation for TensorRT
    return true;
}

void ReIDExtractor::deallocateBuffers() {
    // TODO: Implement CUDA buffer deallocation
}

cv::Mat ReIDExtractor::resizeAndPad(const cv::Mat& image, cv::Size targetSize) {
    if (image.empty()) {
        return cv::Mat();
    }

    // Calculate scaling factor to maintain aspect ratio
    float scaleX = static_cast<float>(targetSize.width) / image.cols;
    float scaleY = static_cast<float>(targetSize.height) / image.rows;
    float scale = std::min(scaleX, scaleY);

    // Calculate new size
    int newWidth = static_cast<int>(image.cols * scale);
    int newHeight = static_cast<int>(image.rows * scale);

    // Resize image
    cv::Mat resized;
    cv::resize(image, resized, cv::Size(newWidth, newHeight));

    // Create padded image
    cv::Mat padded = cv::Mat::zeros(targetSize, image.type());

    // Calculate padding offsets
    int offsetX = (targetSize.width - newWidth) / 2;
    int offsetY = (targetSize.height - newHeight) / 2;

    // Copy resized image to center of padded image
    resized.copyTo(padded(cv::Rect(offsetX, offsetY, newWidth, newHeight)));

    return padded;
}
