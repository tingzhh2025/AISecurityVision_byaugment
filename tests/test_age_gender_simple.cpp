/**
 * @file test_age_gender_simple.cpp
 * @brief Simple test for AgeGenderAnalyzer with InsightFace integration
 */

#include <iostream>
#include <opencv2/opencv.hpp>
#include "../src/ai/AgeGenderAnalyzer.h"
#include "../src/core/Logger.h"

using namespace AISecurityVision;

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cout << "Usage: " << argv[0] << " <image_path>" << std::endl;
        std::cout << "Example: " << argv[0] << " ../models/bus.jpg" << std::endl;
        return 1;
    }

    const std::string imagePath = argv[1];

    std::cout << "=== Age/Gender Recognition Test ===" << std::endl;
    std::cout << "Image: " << imagePath << std::endl;

    // Initialize logger
    Logger::getInstance().setLogLevel(LogLevel::INFO);

    // Create analyzer
    AgeGenderAnalyzer analyzer;
    
    // Initialize with model pack
    std::cout << "Initializing analyzer..." << std::endl;
    if (!analyzer.initialize("../models/Pikachu.pack")) {
        std::cerr << "❌ Failed to initialize analyzer" << std::endl;
        return 1;
    }
    std::cout << "✅ Analyzer initialized" << std::endl;

    // Load image
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "❌ Failed to load image: " << imagePath << std::endl;
        return 1;
    }
    std::cout << "✅ Image loaded: " << image.cols << "x" << image.rows << std::endl;

    // Test analysis
    std::cout << "Analyzing image..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();
    PersonAttributes result = analyzer.analyzeSingle(image);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Display results
    std::cout << "=== Results ===" << std::endl;
    std::cout << "Gender: " << result.gender << " (confidence: " << result.gender_confidence << ")" << std::endl;
    std::cout << "Age group: " << result.age_group << " (confidence: " << result.age_confidence << ")" << std::endl;
    std::cout << "Race: " << result.race << " (confidence: " << result.race_confidence << ")" << std::endl;
    std::cout << "Quality: " << result.quality_score << std::endl;
    std::cout << "Has mask: " << (result.has_mask ? "Yes" : "No") << std::endl;
    std::cout << "Analysis time: " << duration.count() << " ms" << std::endl;
    std::cout << "Valid result: " << (result.isValid() ? "Yes" : "No") << std::endl;

    // Model info
    std::cout << "\n=== Model Info ===" << std::endl;
    auto modelInfo = analyzer.getModelInfo();
    for (const auto& info : modelInfo) {
        std::cout << info << std::endl;
    }

    std::cout << "\n✅ Test completed successfully!" << std::endl;
    return 0;
}
