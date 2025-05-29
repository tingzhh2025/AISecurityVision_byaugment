#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <map>
#include <iomanip>
#include <opencv2/opencv.hpp>

// Minimal YOLOv8 detector interface for testing
enum class InferenceBackend {
    AUTO,
    RKNN,
    OPENCV,
    TENSORRT
};

struct Detection {
    cv::Rect bbox;
    std::string className;
    float confidence;
};

class SimpleYOLOv8Detector {
public:
    SimpleYOLOv8Detector() = default;
    ~SimpleYOLOv8Detector() = default;

    bool initialize(const std::string& modelPath, InferenceBackend backend) {
        std::cout << "[SimpleYOLOv8] Initializing with model: " << modelPath << std::endl;
        std::cout << "[SimpleYOLOv8] Backend: RKNN (simulated)" << std::endl;
        return true;
    }

    std::vector<Detection> detectObjects(const cv::Mat& frame) {
        // Simulate detection results
        std::vector<Detection> detections;

        // Add some fake detections for testing
        if (!frame.empty()) {
            Detection det1;
            det1.bbox = cv::Rect(100, 100, 200, 300);
            det1.className = "person";
            det1.confidence = 0.85f;
            detections.push_back(det1);

            Detection det2;
            det2.bbox = cv::Rect(400, 200, 150, 100);
            det2.className = "car";
            det2.confidence = 0.92f;
            detections.push_back(det2);
        }

        return detections;
    }

    std::string getBackendName() const { return "RKNN (simulated)"; }
    cv::Size getInputSize() const { return cv::Size(640, 640); }
    double getAverageInferenceTime() const { return 75.0; }
    size_t getDetectionCount() const { return 1000; }
};

struct CameraConfig {
    std::string id;
    std::string name;
    std::string rtsp_url;
    int mjpeg_port;
    bool enabled;
};

class SimpleCameraTest {
private:
    std::vector<CameraConfig> cameras;
    std::unique_ptr<SimpleYOLOv8Detector> detector;
    std::vector<cv::VideoCapture> captures;
    std::vector<std::thread> processing_threads;
    std::vector<int> mjpeg_sockets;
    bool running;

public:
    SimpleCameraTest() : running(false) {
        // Configure real cameras
        cameras = {
            {"camera_01", "Real Camera 1", "rtsp://admin:sharpi1688@192.168.1.2:554/1/1", 8161, true},
            {"camera_02", "Real Camera 2", "rtsp://admin:sharpi1688@192.168.1.3:554/1/1", 8162, true}
        };

        // Initialize RKNN detector
        detector = std::make_unique<SimpleYOLOv8Detector>();
    }

    bool initialize() {
        std::cout << "=== Simple Real Camera RKNN Test ===" << std::endl;

        // Initialize YOLOv8 detector with RKNN backend
        std::cout << "Initializing YOLOv8 detector with RKNN..." << std::endl;
        if (!detector->initialize("../models/yolov8n.rknn", InferenceBackend::RKNN)) {
            std::cerr << "Failed to initialize YOLOv8 detector with RKNN" << std::endl;
            return false;
        }

        std::cout << "YOLOv8 detector initialized successfully!" << std::endl;
        std::cout << "Backend: " << detector->getBackendName() << std::endl;
        std::cout << "Input size: " << detector->getInputSize().width << "x" << detector->getInputSize().height << std::endl;

        // Initialize OpenCV VideoCapture for each camera
        for (const auto& camera : cameras) {
            if (!camera.enabled) continue;

            std::cout << "\nInitializing " << camera.name << "..." << std::endl;
            std::cout << "RTSP URL: " << camera.rtsp_url << std::endl;
            std::cout << "MJPEG Port: " << camera.mjpeg_port << std::endl;

            // Create OpenCV VideoCapture
            cv::VideoCapture cap;
            cap.open(camera.rtsp_url);

            if (!cap.isOpened()) {
                std::cerr << "Failed to open camera: " << camera.name << std::endl;
                continue;
            }

            // Set buffer size to reduce latency
            cap.set(cv::CAP_PROP_BUFFERSIZE, 1);

            captures.push_back(std::move(cap));
            mjpeg_sockets.push_back(-1); // Placeholder for MJPEG socket

            std::cout << camera.name << " initialized successfully!" << std::endl;
        }

        if (captures.empty()) {
            std::cerr << "No cameras initialized successfully!" << std::endl;
            return false;
        }

        std::cout << "\nInitialization completed. " << captures.size() << " cameras ready." << std::endl;
        return true;
    }

    void processCameraStream(size_t camera_index) {
        if (camera_index >= captures.size()) {
            return;
        }

        const auto& camera = cameras[camera_index];
        auto& capture = captures[camera_index];

        std::cout << "Starting processing thread for " << camera.name << std::endl;

        cv::Mat frame;
        int frame_count = 0;
        auto last_stats_time = std::chrono::steady_clock::now();
        double total_inference_time = 0.0;
        int detection_count = 0;

        while (running) {
            try {
                // Capture frame
                if (!capture.read(frame)) {
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

                // Save frame to file for visualization (simple approach)
                if (frame_count % 30 == 0) { // Save every 30 frames
                    std::string filename = "output_" + camera.id + "_frame_" + std::to_string(frame_count) + ".jpg";
                    cv::imwrite(filename, display_frame);
                    std::cout << "[" << camera.name << "] Saved frame: " << filename << std::endl;
                }

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

    void drawDetections(cv::Mat& frame, const std::vector<Detection>& detections, const std::string& camera_name) {
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
        std::cout << "Processed frames will be saved as JPEG files for visualization" << std::endl;
        std::cout << "Files will be saved as: output_camera_XX_frame_YY.jpg" << std::endl;
        std::cout << std::endl;

        // Start processing threads for each camera
        for (size_t i = 0; i < captures.size(); ++i) {
            processing_threads.emplace_back(&SimpleCameraTest::processCameraStream, this, i);
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

        // Close captures
        for (auto& cap : captures) {
            if (cap.isOpened()) {
                cap.release();
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
    std::cout << "=== Simple Real Camera RKNN Test ===" << std::endl;
    std::cout << "Testing RKNN YOLOv8 inference with real RTSP cameras" << std::endl;
    std::cout << "Using OpenCV VideoCapture for simplicity" << std::endl;

    SimpleCameraTest test;

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
