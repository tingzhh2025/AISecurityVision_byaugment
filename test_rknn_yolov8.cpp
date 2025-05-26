/**
 * @file test_rknn_yolov8.cpp
 * @brief Test program for RKNN YOLOv8 detection
 * 
 * This program tests the RKNN YOLOv8 detector implementation.
 * 
 * Usage:
 *   ./test_rknn_yolov8 --model models/yolov8n.rknn --image test_image.jpg
 *   ./test_rknn_yolov8 --model models/yolov8n.onnx --image test_image.jpg --backend opencv
 */

#include "src/ai/YOLOv8Detector.h"
#include <opencv2/opencv.hpp>
#include <iostream>
#include <chrono>
#include <getopt.h>

void printUsage(const char* programName) {
    std::cout << "Usage: " << programName << " [OPTIONS]\n"
              << "Options:\n"
              << "  -m, --model PATH     Model file path (.rknn or .onnx)\n"
              << "  -i, --image PATH     Input image path\n"
              << "  -b, --backend TYPE   Backend type (auto, rknn, opencv, tensorrt)\n"
              << "  -c, --confidence F   Confidence threshold (0.0-1.0, default: 0.5)\n"
              << "  -n, --nms F          NMS threshold (0.0-1.0, default: 0.4)\n"
              << "  -o, --output PATH    Output image path (optional)\n"
              << "  -v, --verbose        Verbose output\n"
              << "  -h, --help           Show this help\n"
              << "\nExamples:\n"
              << "  " << programName << " -m models/yolov8n.rknn -i test.jpg\n"
              << "  " << programName << " -m models/yolov8n.onnx -i test.jpg -b opencv\n"
              << "  " << programName << " -m models/yolov8n.rknn -i test.jpg -o result.jpg -v\n";
}

InferenceBackend parseBackend(const std::string& backend) {
    if (backend == "auto") return InferenceBackend::AUTO;
    if (backend == "rknn") return InferenceBackend::RKNN;
    if (backend == "opencv") return InferenceBackend::OPENCV;
    if (backend == "tensorrt") return InferenceBackend::TENSORRT;
    
    std::cerr << "Unknown backend: " << backend << std::endl;
    return InferenceBackend::AUTO;
}

void drawDetections(cv::Mat& image, const std::vector<YOLOv8Detector::Detection>& detections) {
    for (const auto& detection : detections) {
        // Draw bounding box
        cv::rectangle(image, detection.bbox, cv::Scalar(0, 255, 0), 2);
        
        // Prepare label text
        std::string label = detection.className + " " + 
                           std::to_string(static_cast<int>(detection.confidence * 100)) + "%";
        
        // Get text size
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);
        
        // Draw label background
        cv::Point labelPos(detection.bbox.x, detection.bbox.y - textSize.height - 5);
        cv::rectangle(image, 
                     cv::Point(labelPos.x, labelPos.y), 
                     cv::Point(labelPos.x + textSize.width, labelPos.y + textSize.height + baseline),
                     cv::Scalar(0, 255, 0), cv::FILLED);
        
        // Draw label text
        cv::putText(image, label, 
                   cv::Point(labelPos.x, labelPos.y + textSize.height),
                   cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }
}

int main(int argc, char* argv[]) {
    std::string modelPath;
    std::string imagePath;
    std::string outputPath;
    std::string backendStr = "auto";
    float confidenceThreshold = 0.5f;
    float nmsThreshold = 0.4f;
    bool verbose = false;
    
    // Parse command line arguments
    static struct option long_options[] = {
        {"model", required_argument, 0, 'm'},
        {"image", required_argument, 0, 'i'},
        {"backend", required_argument, 0, 'b'},
        {"confidence", required_argument, 0, 'c'},
        {"nms", required_argument, 0, 'n'},
        {"output", required_argument, 0, 'o'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };
    
    int option_index = 0;
    int c;
    
    while ((c = getopt_long(argc, argv, "m:i:b:c:n:o:vh", long_options, &option_index)) != -1) {
        switch (c) {
            case 'm':
                modelPath = optarg;
                break;
            case 'i':
                imagePath = optarg;
                break;
            case 'b':
                backendStr = optarg;
                break;
            case 'c':
                confidenceThreshold = std::stof(optarg);
                break;
            case 'n':
                nmsThreshold = std::stof(optarg);
                break;
            case 'o':
                outputPath = optarg;
                break;
            case 'v':
                verbose = true;
                break;
            case 'h':
                printUsage(argv[0]);
                return 0;
            default:
                printUsage(argv[0]);
                return 1;
        }
    }
    
    // Validate required arguments
    if (modelPath.empty() || imagePath.empty()) {
        std::cerr << "Error: Model path and image path are required\n";
        printUsage(argv[0]);
        return 1;
    }
    
    // Parse backend
    InferenceBackend backend = parseBackend(backendStr);
    
    std::cout << "RKNN YOLOv8 Detection Test\n";
    std::cout << "==========================\n";
    std::cout << "Model: " << modelPath << "\n";
    std::cout << "Image: " << imagePath << "\n";
    std::cout << "Backend: " << backendStr << "\n";
    std::cout << "Confidence threshold: " << confidenceThreshold << "\n";
    std::cout << "NMS threshold: " << nmsThreshold << "\n";
    if (!outputPath.empty()) {
        std::cout << "Output: " << outputPath << "\n";
    }
    std::cout << "\n";
    
    try {
        // Load image
        cv::Mat image = cv::imread(imagePath);
        if (image.empty()) {
            std::cerr << "Error: Could not load image: " << imagePath << std::endl;
            return 1;
        }
        
        if (verbose) {
            std::cout << "Loaded image: " << image.cols << "x" << image.rows << std::endl;
        }
        
        // Initialize detector
        YOLOv8Detector detector;
        
        auto initStart = std::chrono::high_resolution_clock::now();
        bool success = detector.initialize(modelPath, backend);
        auto initEnd = std::chrono::high_resolution_clock::now();
        
        if (!success) {
            std::cerr << "Error: Failed to initialize detector" << std::endl;
            return 1;
        }
        
        auto initTime = std::chrono::duration<double, std::milli>(initEnd - initStart).count();
        
        std::cout << "Detector initialized successfully!\n";
        std::cout << "Backend: " << detector.getBackendName() << "\n";
        std::cout << "Initialization time: " << initTime << " ms\n";
        
        // Set thresholds
        detector.setConfidenceThreshold(confidenceThreshold);
        detector.setNMSThreshold(nmsThreshold);
        
        // Perform detection
        std::cout << "\nPerforming detection...\n";
        
        auto detectStart = std::chrono::high_resolution_clock::now();
        auto detections = detector.detectObjects(image);
        auto detectEnd = std::chrono::high_resolution_clock::now();
        
        auto detectTime = std::chrono::duration<double, std::milli>(detectEnd - detectStart).count();
        
        std::cout << "Detection completed!\n";
        std::cout << "Detection time: " << detectTime << " ms\n";
        std::cout << "Detected objects: " << detections.size() << "\n\n";
        
        // Print detection results
        for (size_t i = 0; i < detections.size(); ++i) {
            const auto& det = detections[i];
            std::cout << "Detection " << (i + 1) << ":\n";
            std::cout << "  Class: " << det.className << " (ID: " << det.classId << ")\n";
            std::cout << "  Confidence: " << (det.confidence * 100) << "%\n";
            std::cout << "  Bbox: [" << det.bbox.x << ", " << det.bbox.y << ", " 
                      << det.bbox.width << ", " << det.bbox.height << "]\n\n";
        }
        
        // Draw detections and save/display result
        cv::Mat resultImage = image.clone();
        drawDetections(resultImage, detections);
        
        if (!outputPath.empty()) {
            cv::imwrite(outputPath, resultImage);
            std::cout << "Result saved to: " << outputPath << std::endl;
        } else {
            cv::imshow("YOLOv8 Detection Result", resultImage);
            std::cout << "Press any key to exit..." << std::endl;
            cv::waitKey(0);
            cv::destroyAllWindows();
        }
        
        // Performance summary
        std::cout << "\nPerformance Summary:\n";
        std::cout << "  Initialization: " << initTime << " ms\n";
        std::cout << "  Detection: " << detectTime << " ms\n";
        std::cout << "  FPS: " << (1000.0 / detectTime) << "\n";
        std::cout << "  Backend: " << detector.getBackendName() << "\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
