#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <string>

/**
 * @brief Face recognition verification result
 */
struct FaceVerificationResult {
    int face_id;
    std::string name;
    float confidence;
    float similarity_score;

    FaceVerificationResult() : face_id(0), confidence(0.0f), similarity_score(0.0f) {}
    FaceVerificationResult(int id, const std::string& faceName, float conf, float sim)
        : face_id(id), name(faceName), confidence(conf), similarity_score(sim) {}
};

class FaceRecognizer {
public:
    FaceRecognizer();
    ~FaceRecognizer();

    bool initialize();
    std::vector<std::string> recognize(const cv::Mat& frame, const std::vector<cv::Rect>& detections);

    // Face verification methods for Task 62
    std::vector<float> extractFaceEmbedding(const cv::Mat& faceImage);
    std::vector<FaceVerificationResult> verifyFace(const cv::Mat& faceImage,
                                                  const std::vector<struct FaceRecord>& registeredFaces,
                                                  float threshold = 0.7f);
    float calculateCosineSimilarity(const std::vector<float>& embedding1,
                                   const std::vector<float>& embedding2);

private:
    // Helper methods
    cv::Mat preprocessFaceImage(const cv::Mat& image);
    std::vector<float> generateDummyEmbedding(const cv::Mat& image);
};
