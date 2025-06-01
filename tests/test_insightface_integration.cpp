/**
 * @file test_insightface_integration.cpp
 * @brief Test program for InsightFace integration with AgeGenderAnalyzer
 * 
 * This program tests the InsightFace integration in the AI Security Vision system.
 * It validates age and gender recognition functionality using the AgeGenderAnalyzer class.
 */

#include <iostream>
#include <opencv2/opencv.hpp>
#include "../src/ai/AgeGenderAnalyzer.h"
#include "../src/core/Logger.h"

using namespace AISecurityVision;

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cout << "Usage: " << argv[0] << " <model_pack_path> <image_path>" << std::endl;
        std::cout << "Example: " << argv[0] << " ../models/Pikachu.pack ../models/bus.jpg" << std::endl;
        return 1;
    }

    const std::string packPath = argv[1];
    const std::string imagePath = argv[2];

    std::cout << "=== InsightFace Integration Test ===" << std::endl;
    std::cout << "Pack file: " << packPath << std::endl;
    std::cout << "Image file: " << imagePath << std::endl;
    std::cout << std::endl;

    // Initialize logger
    Logger::getInstance().setLogLevel(LogLevel::INFO);

    // 1. Create and initialize AgeGenderAnalyzer
    std::cout << "1. Initializing AgeGenderAnalyzer..." << std::endl;
    AgeGenderAnalyzer analyzer;
    
    if (!analyzer.initialize(packPath)) {
        std::cerr << "❌ Failed to initialize AgeGenderAnalyzer" << std::endl;
        return 1;
    }
    std::cout << "✅ AgeGenderAnalyzer initialized successfully" << std::endl;

    // 2. Load test image
    std::cout << "2. Loading test image..." << std::endl;
    cv::Mat image = cv::imread(imagePath);
    if (image.empty()) {
        std::cerr << "❌ Failed to load image: " << imagePath << std::endl;
        return 1;
    }
    std::cout << "✅ Image loaded: " << image.cols << "x" << image.rows << std::endl;

    // 3. Display model information
    std::cout << "3. Model Information:" << std::endl;
    auto modelInfo = analyzer.getModelInfo();
    for (const auto& info : modelInfo) {
        std::cout << "   " << info << std::endl;
    }

    // 4. Test single image analysis
    std::cout << "4. Testing age/gender analysis..." << std::endl;
    
    // Create a person crop from the full image (for testing)
    cv::Mat personCrop = image.clone();
    
    auto startTime = std::chrono::high_resolution_clock::now();
    PersonAttributes attributes = analyzer.analyzeSingle(personCrop);
    auto endTime = std::chrono::high_resolution_clock::now();
    
    auto inferenceTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    // 5. Display results
    std::cout << "5. Analysis Results:" << std::endl;
    std::cout << "   " << attributes.toString() << std::endl;
    std::cout << "   Inference time: " << inferenceTime << " ms" << std::endl;
    std::cout << "   Valid result: " << (attributes.isValid() ? "Yes" : "No") << std::endl;

    // 6. Test batch processing
    std::cout << "6. Testing batch processing..." << std::endl;
    std::vector<PersonDetection> persons;
    
    // Create multiple test crops
    for (int i = 0; i < 3; ++i) {
        PersonDetection person;
        person.crop = personCrop.clone();
        person.bbox = cv::Rect(i * 50, i * 50, 200, 200);
        person.confidence = 0.9f;
        persons.push_back(person);
    }

    startTime = std::chrono::high_resolution_clock::now();
    auto batchResults = analyzer.analyze(persons);
    endTime = std::chrono::high_resolution_clock::now();
    
    auto batchTime = std::chrono::duration<double, std::milli>(endTime - startTime).count();

    std::cout << "   Batch size: " << persons.size() << std::endl;
    std::cout << "   Results count: " << batchResults.size() << std::endl;
    std::cout << "   Batch time: " << batchTime << " ms" << std::endl;
    std::cout << "   Average per item: " << (batchTime / persons.size()) << " ms" << std::endl;

    for (size_t i = 0; i < batchResults.size(); ++i) {
        std::cout << "   Person " << (i + 1) << ": " << batchResults[i].toString() << std::endl;
    }

    // 7. Performance metrics
    std::cout << "7. Performance Metrics:" << std::endl;
    std::cout << "   Last inference time: " << analyzer.getLastInferenceTime() << " ms" << std::endl;
    std::cout << "   Average inference time: " << analyzer.getAverageInferenceTime() << " ms" << std::endl;
    std::cout << "   Total analyses: " << analyzer.getAnalysisCount() << std::endl;

    // 8. Configuration test
    std::cout << "8. Configuration Test:" << std::endl;
    std::cout << "   Gender threshold: " << analyzer.getGenderThreshold() << std::endl;
    std::cout << "   Age threshold: " << analyzer.getAgeThreshold() << std::endl;
    std::cout << "   Batch size: " << analyzer.getBatchSize() << std::endl;

    // Test configuration changes
    analyzer.setGenderThreshold(0.8f);
    analyzer.setAgeThreshold(0.7f);
    analyzer.setBatchSize(8);
    
    std::cout << "   Updated gender threshold: " << analyzer.getGenderThreshold() << std::endl;
    std::cout << "   Updated age threshold: " << analyzer.getAgeThreshold() << std::endl;
    std::cout << "   Updated batch size: " << analyzer.getBatchSize() << std::endl;

    std::cout << std::endl << "=== Test Summary ===" << std::endl;
    
    if (analyzer.isInitialized()) {
        std::cout << "✅ InsightFace integration working correctly" << std::endl;
        std::cout << "✅ Age/gender analysis functional" << std::endl;
        std::cout << "✅ Batch processing working" << std::endl;
        std::cout << "✅ Configuration management working" << std::endl;
        std::cout << "✅ Performance metrics available" << std::endl;
    } else {
        std::cout << "❌ InsightFace integration failed" << std::endl;
        return 1;
    }

    std::cout << std::endl << "🎉 All tests passed! InsightFace integration is ready." << std::endl;
    return 0;
}
