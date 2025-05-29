#pragma once

#include <opencv2/opencv.hpp>
#ifndef DISABLE_OPENCV_DNN
#include <opencv2/dnn.hpp>
#endif
#include <vector>
#include <string>
#include <memory>

/**
 * @brief ReID (Re-Identification) feature extractor using pre-trained ResNet50 model
 *
 * This class implements person/object re-identification feature extraction
 * using a pre-trained ResNet50 model optimized for TensorRT inference.
 *
 * Features:
 * - ResNet50-based feature extraction
 * - 128-2048 dimensional embedding vectors
 * - TensorRT optimization support
 * - Batch processing capability
 * - Cross-camera tracking preparation
 * - Integration with ByteTracker
 */
class ReIDExtractor {
public:
    // ReID embedding structure
    struct ReIDEmbedding {
        std::vector<float> features;    // Feature vector (128-2048 dimensions)
        int trackId;                    // Associated track ID
        int classId;                    // Object class ID
        cv::Rect bbox;                  // Bounding box
        float confidence;               // Detection confidence
        int64_t timestamp;              // Extraction timestamp

        ReIDEmbedding() : trackId(-1), classId(-1), confidence(0.0f), timestamp(0) {}

        // Calculate cosine similarity with another embedding
        float cosineSimilarity(const ReIDEmbedding& other) const;

        // Check if embedding is valid
        bool isValid() const { return !features.empty() && trackId >= 0; }

        // Get feature dimension
        size_t getDimension() const { return features.size(); }
    };

    ReIDExtractor();
    ~ReIDExtractor();

    // Initialization
    bool initialize(const std::string& modelPath = "models/reid_resnet50.onnx");
    void cleanup();
    bool isInitialized() const;

    // Feature extraction
    std::vector<ReIDEmbedding> extractFeatures(const cv::Mat& frame,
                                              const std::vector<cv::Rect>& detections,
                                              const std::vector<int>& trackIds,
                                              const std::vector<int>& classIds = {},
                                              const std::vector<float>& confidences = {});

    // Single object feature extraction
    ReIDEmbedding extractSingleFeature(const cv::Mat& frame,
                                      const cv::Rect& bbox,
                                      int trackId = -1,
                                      int classId = -1,
                                      float confidence = 1.0f);

    // Batch processing
    std::vector<std::vector<ReIDEmbedding>> extractBatch(const std::vector<cv::Mat>& frames,
                                                        const std::vector<std::vector<cv::Rect>>& detections,
                                                        const std::vector<std::vector<int>>& trackIds);

    // Configuration
    void setInputSize(int width, int height);
    void setFeatureDimension(int dimension);
    void setNormalization(bool enabled);
    void setMinObjectSize(int minWidth, int minHeight);

    // Model information
    cv::Size getInputSize() const;
    int getFeatureDimension() const;
    bool isNormalizationEnabled() const;

    // Statistics
    double getInferenceTime() const;
    size_t getExtractionCount() const;
    float getAverageInferenceTime() const;

    // Similarity computation
    static float computeCosineSimilarity(const std::vector<float>& features1,
                                        const std::vector<float>& features2);
    static float computeEuclideanDistance(const std::vector<float>& features1,
                                         const std::vector<float>& features2);

private:
    // TensorRT engine and context (for future TensorRT implementation)
    void* m_engine;
    void* m_context;
    void* m_stream;

    // GPU memory buffers
    void* m_inputBuffer;
    void* m_outputBuffer;

    // OpenCV DNN network (current implementation, only when DNN is enabled)
#ifndef DISABLE_OPENCV_DNN
    cv::dnn::Net m_net;
#endif

    // Configuration
    bool m_initialized;
    bool m_normalizationEnabled;
    int m_inputWidth;
    int m_inputHeight;
    int m_featureDimension;
    int m_minObjectWidth;
    int m_minObjectHeight;

    // Model parameters
    size_t m_inputSize;
    size_t m_outputSize;
    std::string m_modelPath;

    // Statistics
    mutable double m_inferenceTime;
    mutable size_t m_extractionCount;
    mutable std::vector<double> m_inferenceTimes;

    // Internal methods
    bool loadModel(const std::string& modelPath);
    bool setupTensorRT();
    cv::Mat preprocessImage(const cv::Mat& image);
    cv::Mat extractROI(const cv::Mat& frame, const cv::Rect& bbox);
    std::vector<float> postprocessFeatures(const cv::Mat& output);
    std::vector<float> normalizeFeatures(const std::vector<float>& features);
    bool isValidDetection(const cv::Rect& bbox) const;
    bool allocateBuffers();
    void deallocateBuffers();

    // Feature extraction helpers
    std::vector<float> extractFeaturesFromROI(const cv::Mat& roi);
    std::vector<float> generateHandcraftedFeatures(const cv::Mat& roi);
    cv::Mat computeLBP(const cv::Mat& gray);
    cv::Mat resizeAndPad(const cv::Mat& image, cv::Size targetSize);
};
