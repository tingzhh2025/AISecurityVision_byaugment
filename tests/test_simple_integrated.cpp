#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <iomanip>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <mutex>
#include <atomic>
#include <random>

// Include core components
#include "../src/ai/YOLOv8DetectorOptimized.h"
#include <opencv2/opencv.hpp>

// Simple structures for this test
struct VideoSource {
    std::string id;
    std::string name;
    std::string url;
    std::string protocol;
    std::string username;
    std::string password;
    int width = 1920;
    int height = 1080;
    int fps = 25;
    bool enabled = true;
};

struct SimpleFrameResult {
    cv::Mat frame;
    int64_t timestamp;
    std::vector<cv::Rect> detections;
    std::vector<std::string> labels;
};

// Simple MJPEG streamer for this test
class SimpleMJPEGStreamer {
private:
    std::atomic<bool> m_running{false};
    std::thread m_serverThread;
    int m_serverSocket = -1;
    int m_port;
    cv::Mat m_currentFrame;
    std::mutex m_frameMutex;
    std::atomic<size_t> m_connectedClients{0};

public:
    SimpleMJPEGStreamer(int port = 8161) : m_port(port) {}

    ~SimpleMJPEGStreamer() {
        stop();
    }

    bool start() {
        if (m_running.load()) return true;

        m_serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (m_serverSocket < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }

        int opt = 1;
        setsockopt(m_serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        struct sockaddr_in address;
        address.sin_family = AF_INET;
        address.sin_addr.s_addr = INADDR_ANY;
        address.sin_port = htons(m_port);

        if (bind(m_serverSocket, (struct sockaddr*)&address, sizeof(address)) < 0) {
            std::cerr << "Failed to bind socket to port " << m_port << std::endl;
            close(m_serverSocket);
            return false;
        }

        if (listen(m_serverSocket, 10) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            close(m_serverSocket);
            return false;
        }

        m_running.store(true);
        m_serverThread = std::thread(&SimpleMJPEGStreamer::serverLoop, this);

        std::cout << "MJPEG server started on port " << m_port << std::endl;
        return true;
    }

    void stop() {
        if (!m_running.load()) return;

        m_running.store(false);
        if (m_serverSocket >= 0) {
            close(m_serverSocket);
            m_serverSocket = -1;
        }

        if (m_serverThread.joinable()) {
            m_serverThread.join();
        }
    }

    void updateFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(m_frameMutex);
        m_currentFrame = frame.clone();
    }

    size_t getConnectedClients() const {
        return m_connectedClients.load();
    }

private:
    void serverLoop() {
        while (m_running.load()) {
            struct sockaddr_in clientAddr;
            socklen_t clientLen = sizeof(clientAddr);

            int clientSocket = accept(m_serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
            if (clientSocket < 0) {
                if (m_running.load()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                continue;
            }

            std::thread clientThread(&SimpleMJPEGStreamer::handleClient, this, clientSocket);
            clientThread.detach();
        }
    }

    void handleClient(int clientSocket) {
        m_connectedClients.fetch_add(1);

        // Send HTTP headers
        std::string headers =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n\r\n";

        send(clientSocket, headers.c_str(), headers.length(), 0);

        while (m_running.load()) {
            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lock(m_frameMutex);
                if (m_currentFrame.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(33));
                    continue;
                }
                frame = m_currentFrame.clone();
            }

            // Encode frame as JPEG
            std::vector<uchar> buffer;
            std::vector<int> params = {cv::IMWRITE_JPEG_QUALITY, 80};
            if (!cv::imencode(".jpg", frame, buffer, params)) {
                break;
            }

            // Send frame
            std::string frameHeader =
                "--frame\r\n"
                "Content-Type: image/jpeg\r\n"
                "Content-Length: " + std::to_string(buffer.size()) + "\r\n\r\n";

            if (send(clientSocket, frameHeader.c_str(), frameHeader.length(), 0) < 0) break;
            if (send(clientSocket, buffer.data(), buffer.size(), 0) < 0) break;
            if (send(clientSocket, "\r\n", 2, 0) < 0) break;

            std::this_thread::sleep_for(std::chrono::milliseconds(33)); // ~30 FPS
        }

        close(clientSocket);
        m_connectedClients.fetch_sub(1);
    }
};

// Global flag for graceful shutdown
volatile bool g_running = true;

void signalHandler(int signal) {
    std::cout << "\n🛑 Received signal " << signal << ", shutting down gracefully..." << std::endl;
    g_running = false;
}

// Generate synthetic test frames
cv::Mat generateTestFrame(int width = 640, int height = 480) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 255);

    cv::Mat frame(height, width, CV_8UC3);

    // Fill with random colors to simulate real camera data
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            frame.at<cv::Vec3b>(y, x) = cv::Vec3b(dis(gen), dis(gen), dis(gen));
        }
    }

    // Add some geometric shapes to make it more realistic
    cv::rectangle(frame, cv::Point(50, 50), cv::Point(150, 150), cv::Scalar(255, 0, 0), -1);
    cv::circle(frame, cv::Point(300, 300), 50, cv::Scalar(0, 255, 0), -1);
    cv::rectangle(frame, cv::Point(400, 200), cv::Point(500, 350), cv::Scalar(0, 0, 255), -1);

    return frame;
}

// Draw detection boxes on frame
cv::Mat drawDetections(const cv::Mat& frame, const std::vector<YOLOv8DetectorOptimized::Detection>& detections) {
    cv::Mat result = frame.clone();

    for (const auto& detection : detections) {
        // Draw bounding box
        cv::rectangle(result, detection.bbox, cv::Scalar(0, 255, 0), 2);

        // Draw label and confidence
        std::string label = detection.className + " " + std::to_string(int(detection.confidence * 100)) + "%";
        int baseline = 0;
        cv::Size textSize = cv::getTextSize(label, cv::FONT_HERSHEY_SIMPLEX, 0.5, 1, &baseline);

        cv::Point textOrg(detection.bbox.x, detection.bbox.y - 5);
        cv::rectangle(result, textOrg + cv::Point(0, baseline),
                     textOrg + cv::Point(textSize.width, -textSize.height),
                     cv::Scalar(0, 255, 0), cv::FILLED);
        cv::putText(result, label, textOrg, cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 0, 0), 1);
    }

    return result;
}

int main() {
    // Set up signal handlers for graceful shutdown
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "🎉 === SIMPLE INTEGRATED OPTIMIZED AI VISION TEST ===" << std::endl;
    std::cout << "🧠 Multi-threaded RKNN YOLOv8 + MJPEG streaming with detection overlays" << std::endl;
    std::cout << "🎯 Testing core pipeline integration" << std::endl;

    try {
        // Initialize optimized RKNN YOLOv8 detector with 3 threads
        std::cout << "\n[Main] Initializing Optimized RKNN YOLOv8 detector..." << std::endl;
        YOLOv8DetectorOptimized detector(3); // 3 threads for 3 NPU cores

        if (!detector.initialize("models/yolov8n.rknn", InferenceBackend::RKNN)) {
            std::cerr << "❌ Failed to initialize optimized RKNN YOLOv8 detector" << std::endl;
            return -1;
        }

        std::cout << "✅ Optimized RKNN YOLOv8 detector initialized successfully!" << std::endl;
        auto inputSize = detector.getInputSize();
        std::cout << "📐 Input size: " << inputSize.width << "x" << inputSize.height << std::endl;
        detector.setMaxQueueSize(6);

        // Initialize OpenCV camera capture (fallback to webcam if RTSP fails)
        std::cout << "\n[Main] Initializing camera capture..." << std::endl;
        cv::VideoCapture cap;

        // Try RTSP first, fallback to webcam
        std::string rtspUrl = "rtsp://admin:sharpi1688@192.168.1.2:554/1/1";
        if (!cap.open(rtspUrl)) {
            std::cout << "⚠️  RTSP camera not available, trying webcam..." << std::endl;
            if (!cap.open(0)) {
                std::cout << "⚠️  No camera available, using synthetic frames..." << std::endl;
            }
        }

        if (cap.isOpened()) {
            cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
            cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
            cap.set(cv::CAP_PROP_FPS, 30);
            std::cout << "✅ Camera capture initialized" << std::endl;
        }

        // Initialize simple MJPEG streamer
        std::cout << "\n[Main] Initializing MJPEG streamer..." << std::endl;
        SimpleMJPEGStreamer streamer(8161);

        if (!streamer.start()) {
            std::cerr << "❌ Failed to start MJPEG server" << std::endl;
            return -1;
        }

        std::cout << "✅ MJPEG streamer initialized on port 8161" << std::endl;
        std::cout << "🌐 Stream URL: http://localhost:8161/stream" << std::endl;

        std::cout << "\n🎯 === System Status ===" << std::endl;
        std::cout << "✅ Optimized RKNN YOLOv8 detection active (3 threads)" << std::endl;
        std::cout << "✅ Camera source: " << (cap.isOpened() ? rtspUrl : "Synthetic frames") << std::endl;
        std::cout << "✅ MJPEG stream with detection overlays: http://localhost:8161/stream" << std::endl;
        std::cout << "\n⏸️  Press Ctrl+C to stop the test..." << std::endl;
        std::cout << "📊 Performance stats will be displayed every 5 seconds..." << std::endl;

        // Main processing loop
        auto lastStatsTime = std::chrono::steady_clock::now();
        int frameCount = 0;
        int totalDetections = 0;

        while (g_running) {
            cv::Mat frame;
            int64_t timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count();

            // Get frame from camera or generate synthetic frame
            if (cap.isOpened()) {
                if (!cap.read(frame)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
            } else {
                // Generate synthetic test frame
                frame = generateTestFrame(640, 480);
            }

            frameCount++;

            // Perform AI detection asynchronously
            auto detectionFuture = detector.detectAsync(frame);
            auto detections = detectionFuture.get();
            totalDetections += detections.size();

            // Draw detection boxes on frame
            cv::Mat frameWithDetections = drawDetections(frame, detections);

            // Update MJPEG stream
            streamer.updateFrame(frameWithDetections);

            // Print stats every 5 seconds
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration<double>(now - lastStatsTime).count();

            if (elapsed >= 5.0) {
                double fps = frameCount / elapsed;
                auto stats = detector.getPerformanceStats();

                std::cout << "📊 === Performance Stats ===" << std::endl;
                std::cout << "🎥 Camera FPS: " << std::fixed << std::setprecision(1) << fps << std::endl;
                std::cout << "🧠 Inference Time: " << std::setprecision(1) << stats.avgInferenceTime << "ms" << std::endl;
                std::cout << "⏳ Queue Time: " << std::setprecision(1) << stats.avgQueueTime << "ms" << std::endl;
                std::cout << "🎯 Detections/Frame: " << std::setprecision(1) << (double)totalDetections / frameCount << std::endl;
                std::cout << "📋 Queue Size: " << stats.queueSize << std::endl;
                std::cout << "🔄 Total Inferences: " << stats.totalInferences << std::endl;
                std::cout << "🌐 Stream Clients: " << streamer.getConnectedClients() << std::endl;
                std::cout << "================================" << std::endl;

                // Reset counters
                lastStatsTime = now;
                frameCount = 0;
                totalDetections = 0;
            }

            // Small delay to prevent overwhelming
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        // Graceful shutdown
        std::cout << "\n🛑 === Shutting Down ===" << std::endl;
        std::cout << "Stopping camera..." << std::endl;
        if (cap.isOpened()) {
            cap.release();
        }

        std::cout << "Stopping streamer..." << std::endl;
        streamer.stop();

        std::cout << "✅ Shutdown complete" << std::endl;
        std::cout << "🎯 === Test Completed Successfully ===" << std::endl;

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "❌ Fatal error: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "❌ Unknown fatal error occurred" << std::endl;
        return 1;
    }
}
