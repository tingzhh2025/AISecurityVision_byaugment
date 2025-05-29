#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <map>
#include <iomanip>
#include <opencv2/opencv.hpp>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

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

class FFmpegDecoder {
private:
    AVFormatContext* format_ctx;
    AVCodecContext* codec_ctx;
    SwsContext* sws_ctx;
    AVFrame* frame;
    AVFrame* frame_rgb;
    AVPacket* packet;
    uint8_t* buffer;
    int video_stream_index;
    
public:
    FFmpegDecoder() : format_ctx(nullptr), codec_ctx(nullptr), sws_ctx(nullptr),
                      frame(nullptr), frame_rgb(nullptr), packet(nullptr),
                      buffer(nullptr), video_stream_index(-1) {}
    
    ~FFmpegDecoder() {
        cleanup();
    }
    
    bool initialize(const std::string& url) {
        // Initialize FFmpeg
        av_register_all();
        avformat_network_init();
        
        // Open input
        format_ctx = avformat_alloc_context();
        
        // Set options for RTSP
        AVDictionary* options = nullptr;
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "stimeout", "5000000", 0); // 5 second timeout
        
        if (avformat_open_input(&format_ctx, url.c_str(), nullptr, &options) != 0) {
            std::cerr << "Could not open input: " << url << std::endl;
            av_dict_free(&options);
            return false;
        }
        av_dict_free(&options);
        
        // Find stream info
        if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
            std::cerr << "Could not find stream info" << std::endl;
            return false;
        }
        
        // Find video stream
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = i;
                break;
            }
        }
        
        if (video_stream_index == -1) {
            std::cerr << "Could not find video stream" << std::endl;
            return false;
        }
        
        // Get codec
        AVCodecParameters* codec_params = format_ctx->streams[video_stream_index]->codecpar;
        AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
        if (!codec) {
            std::cerr << "Codec not found" << std::endl;
            return false;
        }
        
        // Allocate codec context
        codec_ctx = avcodec_alloc_context3(codec);
        if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) {
            std::cerr << "Could not copy codec parameters" << std::endl;
            return false;
        }
        
        // Open codec
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) {
            std::cerr << "Could not open codec" << std::endl;
            return false;
        }
        
        // Allocate frames
        frame = av_frame_alloc();
        frame_rgb = av_frame_alloc();
        packet = av_packet_alloc();
        
        // Allocate buffer for RGB frame
        int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, codec_ctx->width, codec_ctx->height, 1);
        buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
        av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, buffer, AV_PIX_FMT_BGR24, codec_ctx->width, codec_ctx->height, 1);
        
        // Initialize SWS context
        sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                codec_ctx->width, codec_ctx->height, AV_PIX_FMT_BGR24,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
        
        std::cout << "FFmpeg decoder initialized successfully" << std::endl;
        std::cout << "Video: " << codec_ctx->width << "x" << codec_ctx->height << std::endl;
        
        return true;
    }
    
    bool getNextFrame(cv::Mat& frame_out) {
        while (av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_index) {
                int ret = avcodec_send_packet(codec_ctx, packet);
                if (ret < 0) {
                    av_packet_unref(packet);
                    continue;
                }
                
                ret = avcodec_receive_frame(codec_ctx, frame);
                if (ret == 0) {
                    // Convert to BGR
                    sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height,
                             frame_rgb->data, frame_rgb->linesize);
                    
                    // Create OpenCV Mat
                    frame_out = cv::Mat(codec_ctx->height, codec_ctx->width, CV_8UC3, frame_rgb->data[0], frame_rgb->linesize[0]).clone();
                    
                    av_packet_unref(packet);
                    return true;
                }
            }
            av_packet_unref(packet);
        }
        return false;
    }
    
private:
    void cleanup() {
        if (buffer) av_free(buffer);
        if (frame_rgb) av_frame_free(&frame_rgb);
        if (frame) av_frame_free(&frame);
        if (packet) av_packet_free(&packet);
        if (sws_ctx) sws_freeContext(sws_ctx);
        if (codec_ctx) avcodec_free_context(&codec_ctx);
        if (format_ctx) avformat_close_input(&format_ctx);
    }
};

class RealCameraFFmpegTest {
private:
    std::vector<CameraConfig> cameras;
    std::unique_ptr<SimpleYOLOv8Detector> detector;
    std::vector<std::unique_ptr<FFmpegDecoder>> decoders;
    std::vector<std::thread> processing_threads;
    bool running;

public:
    RealCameraFFmpegTest() : running(false) {
        // Configure real cameras
        cameras = {
            {"camera_01", "Real Camera 1", "rtsp://admin:sharpi1688@192.168.1.2:554/1/1", 8161, true},
            {"camera_02", "Real Camera 2", "rtsp://admin:sharpi1688@192.168.1.3:554/1/1", 8162, true}
        };
        
        // Initialize RKNN detector
        detector = std::make_unique<SimpleYOLOv8Detector>();
    }

    bool initialize() {
        std::cout << "=== Real Camera FFmpeg Test ===" << std::endl;
        
        // Initialize YOLOv8 detector
        std::cout << "Initializing YOLOv8 detector..." << std::endl;
        if (!detector->initialize("../models/yolov8n.rknn", InferenceBackend::RKNN)) {
            std::cerr << "Failed to initialize YOLOv8 detector" << std::endl;
            return false;
        }
        
        std::cout << "YOLOv8 detector initialized successfully!" << std::endl;
        
        // Initialize FFmpeg decoders for each camera
        for (const auto& camera : cameras) {
            if (!camera.enabled) continue;
            
            std::cout << "\nInitializing " << camera.name << "..." << std::endl;
            std::cout << "RTSP URL: " << camera.rtsp_url << std::endl;
            
            auto decoder = std::make_unique<FFmpegDecoder>();
            if (!decoder->initialize(camera.rtsp_url)) {
                std::cerr << "Failed to initialize FFmpeg decoder for " << camera.name << std::endl;
                continue;
            }
            
            decoders.push_back(std::move(decoder));
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
        if (camera_index >= decoders.size()) {
            return;
        }
        
        const auto& camera = cameras[camera_index];
        auto& decoder = decoders[camera_index];
        
        std::cout << "Starting processing thread for " << camera.name << std::endl;
        
        cv::Mat frame;
        int frame_count = 0;
        auto last_stats_time = std::chrono::steady_clock::now();
        double total_inference_time = 0.0;
        int detection_count = 0;
        
        while (running) {
            try {
                // Decode frame
                if (!decoder->getNextFrame(frame)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                if (frame.empty()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                frame_count++;
                
                // Run inference (simulated)
                auto start_time = std::chrono::high_resolution_clock::now();
                auto detections = detector->detectObjects(frame);
                auto end_time = std::chrono::high_resolution_clock::now();
                
                double inference_time = std::chrono::duration<double, std::milli>(end_time - start_time).count();
                total_inference_time += inference_time;
                detection_count++;
                
                // Draw detection results
                cv::Mat display_frame = frame.clone();
                drawDetections(display_frame, detections, camera.name);
                drawPerformanceInfo(display_frame, inference_time, detections.size(), frame_count);
                
                // Save frame to file for visualization
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
        };
        cv::Scalar default_color(255, 255, 0); // Yellow
        
        for (const auto& detection : detections) {
            cv::Scalar color = default_color;
            auto it = colors.find(detection.className);
            if (it != colors.end()) {
                color = it->second;
            }
            
            cv::rectangle(frame, detection.bbox, color, 2);
            
            std::string label = detection.className + " " + 
                               std::to_string(static_cast<int>(detection.confidence * 100)) + "%";
            
            cv::putText(frame, label, cv::Point(detection.bbox.x, detection.bbox.y - 10), 
                       cv::FONT_HERSHEY_SIMPLEX, 0.6, color, 1);
        }
        
        cv::putText(frame, camera_name, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
    }

    void drawPerformanceInfo(cv::Mat& frame, double inference_time, size_t detection_count, int frame_count) {
        int y_offset = frame.rows - 80;
        
        cv::rectangle(frame, cv::Point(10, y_offset - 5), cv::Point(400, frame.rows - 10), cv::Scalar(0, 0, 0, 128), -1);
        
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
        
        for (size_t i = 0; i < decoders.size(); ++i) {
            processing_threads.emplace_back(&RealCameraFFmpegTest::processCameraStream, this, i);
        }
    }

    void stop() {
        std::cout << "\nStopping camera processing..." << std::endl;
        running = false;
        
        for (auto& thread : processing_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        std::cout << "All processing threads stopped." << std::endl;
    }
};

int main() {
    std::cout << "=== Real Camera FFmpeg Test ===" << std::endl;
    std::cout << "Testing real RTSP cameras with FFmpeg decoder" << std::endl;
    
    RealCameraFFmpegTest test;
    
    if (!test.initialize()) {
        std::cerr << "Failed to initialize test" << std::endl;
        return -1;
    }
    
    test.start();
    
    std::cout << "\nPress Enter to stop the test..." << std::endl;
    std::cin.get();
    
    test.stop();
    
    std::cout << "\n=== Test Completed ===" << std::endl;
    return 0;
}
