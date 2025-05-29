#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <map>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include "../../src/ai/YOLOv8Detector.h"
#include "../../src/video/FFmpegDecoder.h"
#include "../../src/output/Streamer.h"
#include "../../src/core/VideoPipeline.h"

struct CameraConfig {
    std::string id;
    std::string name;
    std::string rtsp_url;
    int mjpeg_port;
    bool enabled;
};

class RealCameraTest {
private:
    std::vector<CameraConfig> cameras;
    std::unique_ptr<YOLOv8Detector> detector;
    std::vector<std::unique_ptr<FFmpegDecoder>> decoders;
    std::vector<std::unique_ptr<Streamer>> streamers;
    std::vector<std::thread> processing_threads;
    bool running;

public:
    RealCameraTest() : running(false) {
        // Configure real cameras
        cameras = {
            {"camera_01", "Real Camera 1", "rtsp://admin:sharpi1688@192.168.1.2:554/1/1", 8161, true},
            {"camera_02", "Real Camera 2", "rtsp://admin:sharpi1688@192.168.1.3:554/1/1", 8162, true}
        };

        // Initialize RKNN detector
        detector = std::make_unique<YOLOv8Detector>();
    }

    bool initialize() {
        std::cout << "=== Real Camera RKNN Test Initialization ===" << std::endl;

        // Initialize YOLOv8 detector with RKNN backend
        std::cout << "Initializing YOLOv8 detector with RKNN..." << std::endl;
        if (!detector->initialize("../models/yolov8n.rknn", InferenceBackend::RKNN)) {
            std::cerr << "Failed to initialize YOLOv8 detector with RKNN" << std::endl;
            return false;
        }

        std::cout << "YOLOv8 detector initialized successfully!" << std::endl;
        std::cout << "Backend: " << detector->getBackendName() << std::endl;
        std::cout << "Input size: " << detector->getInputSize().width << "x" << detector->getInputSize().height << std::endl;

        // Initialize decoders and streamers for each camera
        for (const auto& camera : cameras) {
            if (!camera.enabled) continue;

            std::cout << "\nInitializing " << camera.name << "..." << std::endl;
            std::cout << "RTSP URL: " << camera.rtsp_url << std::endl;
            std::cout << "MJPEG Port: " << camera.mjpeg_port << std::endl;

            // Create decoder
            auto decoder = std::make_unique<FFmpegDecoder>();
            VideoSource source;
            source.url = camera.rtsp_url;
            source.protocol = "rtsp";
            source.id = camera.id;
            source.name = camera.name;
            source.username = "admin";
            source.password = "sharpi1688";
            if (!decoder->initialize(source)) {
                std::cerr << "Failed to initialize decoder for " << camera.name << std::endl;
                continue;
            }

            // Create MJPEG streamer
            auto streamer = std::make_unique<Streamer>();
            if (!streamer->initialize(camera.id)) {
                std::cerr << "Failed to initialize MJPEG streamer for " << camera.name << std::endl;
                continue;
            }

            // Configure streamer for MJPEG
            StreamConfig config;
            config.protocol = StreamProtocol::MJPEG;
            config.port = camera.mjpeg_port;
            config.width = 1280;
            config.height = 720;
            config.fps = 25;
            config.quality = 80;
            config.enableOverlays = true;
            streamer->setConfig(config);

            // Start MJPEG server
            if (!streamer->startServer()) {
                std::cerr << "Failed to start MJPEG server for " << camera.name << std::endl;
                continue;
            }

            decoders.push_back(std::move(decoder));
            streamers.push_back(std::move(streamer));

            std::cout << camera.name << " initialized successfully!" << std::endl;
        }

        if (decoders.empty()) {
            std::cerr << "No cameras initialized successfully!" << std::endl;
            return false;
        }

        std::cout << "\nInitialization completed. " << decoders.size() << " cameras ready." << std::endl;
        return true;
    }

    void processCameraStream(size_t camera_index) {
        if (camera_index >= decoders.size() || camera_index >= streamers.size()) {
            return;
        }

        const auto& camera = cameras[camera_index];
        auto& decoder = decoders[camera_index];
        auto& streamer = streamers[camera_index];

        std::cout << "Starting processing thread for " << camera.name << std::endl;

        cv::Mat frame;
        int frame_count = 0;
        auto last_stats_time = std::chrono::steady_clock::now();
        double total_inference_time = 0.0;
        int detection_count = 0;

        while (running) {
            try {
                // Decode frame
                int64_t timestamp;
                if (!decoder->getNextFrame(frame, timestamp)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                if (frame.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }

                frame_count++;

                // Run RKNN inference
                auto start_time = std::chrono::high_resolution_clock::now();
                auto detections = detector->detectObjects(frame);
                auto end_time = std::chrono::high_resolution_clock::now();

                double inference_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
                total_inference_time += inference_time;
                detection_count++;

                // Draw detection results
                cv::Mat display_frame = frame.clone();
                drawDetections(display_frame, detections, camera.name);

                // Add performance info
                drawPerformanceInfo(display_frame, inference_time, detections.size(), frame_count);

                // Stream via MJPEG - create FrameResult
                FrameResult result;
                result.frame = display_frame;
                result.timestamp = timestamp;

                // Convert YOLOv8 detections to cv::Rect for FrameResult
                result.detections.clear();
                result.labels.clear();
                for (const auto& det : detections) {
                    result.detections.push_back(det.bbox);
                    result.labels.push_back(det.className);
                }

                streamer->processFrame(result);

                // Print statistics every 5 seconds
                auto current_time = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_stats_time).count();
                if (elapsed >= 5) {
                    double avg_inference = detection_count > 0 ? total_inference_time / detection_count : 0.0;
                    double fps = frame_count / 5.0;

                    std::cout << "[" << camera.name << "] "
                              << "FPS: " << std::fixed << std::setprecision(1) << fps
                              << ", Avg Inference: " << std::setprecision(1) << avg_inference << "ms"
                              << ", Detections: " << detections.size()
                              << ", Frames: " << frame_count << std::endl;

                    frame_count = 0;
                    total_inference_time = 0.0;
                    detection_count = 0;
                    last_stats_time = current_time;
                }

            } catch (const std::exception& e) {
                std::cerr << "[" << camera.name << "] Error: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }

        std::cout << "Processing thread for " << camera.name << " stopped." << std::endl;
    }

    void drawDetections(cv::Mat& frame, const std::vector<YOLOv8Detector::Detection>& detections, const std::string& camera_name) {
        // Define colors for different classes
        std::map<std::string, cv::Scalar> colors = {
            {"person", cv::Scalar(0, 255, 0)},      // Green
            {"car", cv::Scalar(255, 0, 0)},         // Blue
            {"bicycle", cv::Scalar(0, 0, 255)},     // Red
            {"motorcycle", cv::Scalar(255, 255, 0)}, // Cyan
            {"bus", cv::Scalar(255, 0, 255)},       // Magenta
            {"truck", cv::Scalar(0, 255, 255)}      // Yellow
        };
        cv::Scalar default_color(255, 255, 0); // Yellow

        for (const auto& detection : detections) {
            // Get color for this class
            cv::Scalar color = default_color;
            auto it = colors.find(detection.className);
            if (it != colors.end()) {
                color = it->second;
            }

            // Draw bounding box
            cv::rectangle(frame, detection.bbox, color, 2);

            // Prepare label text
            std::string label = detection.className + " " +
                               std::to_string(static_cast<int>(detection.confidence * 100)) + "%";

            // Get text size for background
            int baseline = 0;
            cv::Size text_size = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.6, 1, &baseline);

            // Draw label background
            cv::Point label_pos(detection.bbox.x, detection.bbox.y - 10);
            cv::rectangle(frame,
                         cv::Point(label_pos.x, label_pos.y - text_size.height - baseline),
                         cv::Point(label_pos.x + text_size.width, label_pos.y + baseline),
                         color, -1);

            // Draw label text
            cv::putText(frame, label, label_pos, cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 0), 1);
        }

        // Draw camera name
        cv::putText(frame, camera_name, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    }

    void drawPerformanceInfo(cv::Mat& frame, double inference_time, size_t detection_count, int frame_count) {
        int y_offset = frame.rows - 80;

        // Draw performance info background
        cv::rectangle(frame, cv::Point(10, y_offset - 5), cv::Point(400, frame.rows - 10), cv::Scalar(0, 0, 0, 128), -1);

        // Draw performance text
        std::string perf_text = "Inference: " + std::to_string(static_cast<int>(inference_time)) + "ms";
        cv::putText(frame, perf_text, cv::Point(15, y_offset + 15), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

        std::string det_text = "Detections: " + std::to_string(detection_count);
        cv::putText(frame, det_text, cv::Point(15, y_offset + 35), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);

        std::string frame_text = "Frame: " + std::to_string(frame_count);
        cv::putText(frame, frame_text, cv::Point(15, y_offset + 55), cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 0), 1);
    }

    void start() {
        running = true;

        std::cout << "\n=== Starting Real Camera Processing ===" << std::endl;
        std::cout << "MJPEG streams will be available at:" << std::endl;
        for (size_t i = 0; i < cameras.size() && i < decoders.size(); ++i) {
            std::cout << "- " << cameras[i].name << ": http://localhost:" << cameras[i].mjpeg_port << std::endl;
        }
        std::cout << std::endl;

        // Start processing threads for each camera
        for (size_t i = 0; i < decoders.size(); ++i) {
            processing_threads.emplace_back(&RealCameraTest::processCameraStream, this, i);
        }
    }

    void stop() {
        std::cout << "\nStopping camera processing..." << std::endl;
        running = false;

        // Wait for all threads to finish
        for (auto& thread : processing_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }

        std::cout << "All processing threads stopped." << std::endl;
    }

    void printStatistics() {
        std::cout << "\n=== Final Statistics ===" << std::endl;
        std::cout << "YOLOv8 Detector Performance:" << std::endl;
        std::cout << "- Backend: " << detector->getBackendName() << std::endl;
        std::cout << "- Average inference time: " << detector->getAverageInferenceTime() << "ms" << std::endl;
        std::cout << "- Total detections: " << detector->getDetectionCount() << std::endl;
    }
};

int main() {
    std::cout << "=== Real Camera RKNN Test ===" << std::endl;
    std::cout << "Testing RKNN YOLOv8 inference with real RTSP cameras" << std::endl;
    std::cout << "MJPEG visualization streams will be available on ports 8161 and 8162" << std::endl;

    RealCameraTest test;

    // Initialize
    if (!test.initialize()) {
        std::cerr << "Failed to initialize test" << std::endl;
        return -1;
    }

    // Start processing
    test.start();

    // Run for specified duration or until interrupted
    std::cout << "\nPress Enter to stop the test..." << std::endl;
    std::cin.get();

    // Stop processing
    test.stop();

    // Print final statistics
    test.printStatistics();

    std::cout << "\n=== Test Completed ===" << std::endl;
    return 0;
}
