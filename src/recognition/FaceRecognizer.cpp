#include "FaceRecognizer.h"
#include "../database/DatabaseManager.h"
#include <iostream>
#include <cmath>
#include <algorithm>
#include <random>

#include "../core/Logger.h"
using namespace AISecurityVision;
FaceRecognizer::FaceRecognizer() {}
FaceRecognizer::~FaceRecognizer() {}

bool FaceRecognizer::initialize() {
    LOG_INFO() << "[FaceRecognizer] Initialized with face verification support";
    return true;
}

std::vector<std::string> FaceRecognizer::recognize(const cv::Mat& frame, const std::vector<cv::Rect>& detections) {
    return std::vector<std::string>();
}

// Task 62: Face verification implementation
std::vector<float> FaceRecognizer::extractFaceEmbedding(const cv::Mat& faceImage) {
    if (faceImage.empty()) {
        LOG_ERROR() << "[FaceRecognizer] Empty face image provided";
        return std::vector<float>();
    }

    // Preprocess the face image
    cv::Mat preprocessed = preprocessFaceImage(faceImage);

    // TODO: Replace with actual face recognition model (ResNet, FaceNet, etc.)
    // For now, generate a deterministic dummy embedding based on image content
    return generateDummyEmbedding(preprocessed);
}

std::vector<FaceVerificationResult> FaceRecognizer::verifyFace(const cv::Mat& faceImage,
                                                              const std::vector<FaceRecord>& registeredFaces,
                                                              float threshold) {
    std::vector<FaceVerificationResult> results;

    if (faceImage.empty()) {
        LOG_ERROR() << "[FaceRecognizer] Empty face image for verification";
        return results;
    }

    // Extract embedding from input image
    std::vector<float> inputEmbedding = extractFaceEmbedding(faceImage);
    if (inputEmbedding.empty()) {
        LOG_ERROR() << "[FaceRecognizer] Failed to extract embedding from input image";
        return results;
    }

    LOG_INFO() << "[FaceRecognizer] Verifying face against " << registeredFaces.size()
              << " registered faces with threshold " << threshold;

    // Compare against all registered faces
    for (const auto& face : registeredFaces) {
        if (face.embedding.empty()) {
            LOG_INFO() << "[FaceRecognizer] Skipping face " << face.name << " (no embedding)";
            continue;
        }

        // Calculate similarity
        float similarity = calculateCosineSimilarity(inputEmbedding, face.embedding);

        // Convert similarity to confidence (0-1 range)
        float confidence = std::max(0.0f, similarity);

        LOG_INFO() << "[FaceRecognizer] Face " << face.name << " similarity: "
                  << similarity << ", confidence: " << confidence;

        // Add to results if above threshold
        if (confidence >= threshold) {
            results.emplace_back(face.id, face.name, confidence, similarity);
        }
    }

    // Sort results by confidence (highest first)
    std::sort(results.begin(), results.end(),
              [](const FaceVerificationResult& a, const FaceVerificationResult& b) {
                  return a.confidence > b.confidence;
              });

    LOG_INFO() << "[FaceRecognizer] Found " << results.size() << " matches above threshold";

    return results;
}

float FaceRecognizer::calculateCosineSimilarity(const std::vector<float>& embedding1,
                                               const std::vector<float>& embedding2) {
    if (embedding1.size() != embedding2.size() || embedding1.empty()) {
        LOG_ERROR() << "[FaceRecognizer] Embedding size mismatch or empty embeddings";
        return 0.0f;
    }

    // Calculate dot product
    float dotProduct = 0.0f;
    for (size_t i = 0; i < embedding1.size(); ++i) {
        dotProduct += embedding1[i] * embedding2[i];
    }

    // Calculate magnitudes
    float magnitude1 = 0.0f;
    float magnitude2 = 0.0f;
    for (size_t i = 0; i < embedding1.size(); ++i) {
        magnitude1 += embedding1[i] * embedding1[i];
        magnitude2 += embedding2[i] * embedding2[i];
    }

    magnitude1 = std::sqrt(magnitude1);
    magnitude2 = std::sqrt(magnitude2);

    // Avoid division by zero
    if (magnitude1 == 0.0f || magnitude2 == 0.0f) {
        return 0.0f;
    }

    // Calculate cosine similarity
    float similarity = dotProduct / (magnitude1 * magnitude2);

    // Clamp to [-1, 1] range
    similarity = std::max(-1.0f, std::min(1.0f, similarity));

    return similarity;
}

cv::Mat FaceRecognizer::preprocessFaceImage(const cv::Mat& image) {
    cv::Mat processed;

    // Convert to grayscale if needed
    if (image.channels() == 3) {
        cv::cvtColor(image, processed, cv::COLOR_BGR2GRAY);
    } else {
        processed = image.clone();
    }

    // Resize to standard face size (112x112 is common for face recognition)
    cv::resize(processed, processed, cv::Size(112, 112));

    // Normalize pixel values to [0, 1]
    processed.convertTo(processed, CV_32F, 1.0/255.0);

    // Apply histogram equalization for better feature extraction
    cv::Mat equalizedImage;
    processed.convertTo(equalizedImage, CV_8U, 255.0);
    cv::equalizeHist(equalizedImage, equalizedImage);
    equalizedImage.convertTo(processed, CV_32F, 1.0/255.0);

    return processed;
}

std::vector<float> FaceRecognizer::generateDummyEmbedding(const cv::Mat& image) {
    const int EMBEDDING_SIZE = 128;
    std::vector<float> embedding(EMBEDDING_SIZE);

    // Create a deterministic embedding based on image content
    // This ensures the same image always produces the same embedding

    // Calculate basic image statistics
    cv::Scalar meanVal = cv::mean(image);
    cv::Mat stdDev;
    cv::meanStdDev(image, meanVal, stdDev);

    // Use image statistics as seed for deterministic random generation
    uint32_t seed = static_cast<uint32_t>(meanVal[0] * 1000000) +
                   static_cast<uint32_t>(stdDev.at<double>(0) * 1000000);

    std::mt19937 generator(seed);
    std::normal_distribution<float> distribution(0.0f, 1.0f);

    // Generate embedding values
    for (int i = 0; i < EMBEDDING_SIZE; ++i) {
        embedding[i] = distribution(generator);
    }

    // Normalize the embedding vector
    float magnitude = 0.0f;
    for (float val : embedding) {
        magnitude += val * val;
    }
    magnitude = std::sqrt(magnitude);

    if (magnitude > 0.0f) {
        for (float& val : embedding) {
            val /= magnitude;
        }
    }

    return embedding;
}
