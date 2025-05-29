#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <memory>
#include <map>
#include <iomanip>
#include <opencv2/opencv.hpp>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

// Simple MJPEG HTTP server
class MJPEGServer {
private:
    int server_socket;
    int port;
    bool running;
    std::thread server_thread;
    cv::Mat current_frame;
    std::mutex frame_mutex;

public:
    MJPEGServer(int p) : server_socket(-1), port(p), running(false) {}
    
    ~MJPEGServer() {
        stop();
    }
    
    bool start() {
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (server_socket < 0) {
            std::cerr << "Failed to create socket" << std::endl;
            return false;
        }
        
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        struct sockaddr_in server_addr;
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(port);
        
        if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
            std::cerr << "Failed to bind socket to port " << port << std::endl;
            close(server_socket);
            return false;
        }
        
        if (listen(server_socket, 5) < 0) {
            std::cerr << "Failed to listen on socket" << std::endl;
            close(server_socket);
            return false;
        }
        
        running = true;
        server_thread = std::thread(&MJPEGServer::serverLoop, this);
        
        std::cout << "MJPEG server started on port " << port << std::endl;
        return true;
    }
    
    void stop() {
        if (running) {
            running = false;
            if (server_socket >= 0) {
                close(server_socket);
                server_socket = -1;
            }
            if (server_thread.joinable()) {
                server_thread.join();
            }
        }
    }
    
    void updateFrame(const cv::Mat& frame) {
        std::lock_guard<std::mutex> lock(frame_mutex);
        current_frame = frame.clone();
    }
    
private:
    void serverLoop() {
        while (running) {
            struct sockaddr_in client_addr;
            socklen_t client_len = sizeof(client_addr);
            
            int client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_len);
            if (client_socket < 0) {
                if (running) {
                    std::cerr << "Failed to accept client connection" << std::endl;
                }
                continue;
            }
            
            std::thread client_thread(&MJPEGServer::handleClient, this, client_socket);
            client_thread.detach();
        }
    }
    
    void handleClient(int client_socket) {
        // Send HTTP headers
        std::string headers = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: multipart/x-mixed-replace; boundary=frame\r\n"
            "Cache-Control: no-cache\r\n"
            "Connection: close\r\n"
            "\r\n";
        
        send(client_socket, headers.c_str(), headers.length(), 0);
        
        while (running) {
            cv::Mat frame;
            {
                std::lock_guard<std::mutex> lock(frame_mutex);
                if (!current_frame.empty()) {
                    frame = current_frame.clone();
                }
            }
            
            if (!frame.empty()) {
                std::vector<uchar> buffer;
                cv::imencode(".jpg", frame, buffer);
                
                std::string frame_header = 
                    "--frame\r\n"
                    "Content-Type: image/jpeg\r\n"
                    "Content-Length: " + std::to_string(buffer.size()) + "\r\n"
                    "\r\n";
                
                if (send(client_socket, frame_header.c_str(), frame_header.length(), 0) < 0) {
                    break;
                }
                
                if (send(client_socket, buffer.data(), buffer.size(), 0) < 0) {
                    break;
                }
                
                std::string frame_footer = "\r\n";
                if (send(client_socket, frame_footer.c_str(), frame_footer.length(), 0) < 0) {
                    break;
                }
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100)); // ~10 FPS
        }
        
        close(client_socket);
    }
};

// Simplified FFmpeg decoder
class SimpleFFmpegDecoder {
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
    SimpleFFmpegDecoder() : format_ctx(nullptr), codec_ctx(nullptr), sws_ctx(nullptr),
                           frame(nullptr), frame_rgb(nullptr), packet(nullptr),
                           buffer(nullptr), video_stream_index(-1) {}
    
    ~SimpleFFmpegDecoder() {
        cleanup();
    }
    
    bool initialize(const std::string& url) {
        // Open input
        format_ctx = avformat_alloc_context();
        
        AVDictionary* options = nullptr;
        av_dict_set(&options, "rtsp_transport", "tcp", 0);
        av_dict_set(&options, "stimeout", "5000000", 0);
        
        if (avformat_open_input(&format_ctx, url.c_str(), nullptr, &options) != 0) {
            av_dict_free(&options);
            return false;
        }
        av_dict_free(&options);
        
        if (avformat_find_stream_info(format_ctx, nullptr) < 0) {
            return false;
        }
        
        // Find video stream
        for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
            if (format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                video_stream_index = i;
                break;
            }
        }
        
        if (video_stream_index == -1) return false;
        
        // Setup codec
        AVCodecParameters* codec_params = format_ctx->streams[video_stream_index]->codecpar;
        AVCodec* codec = avcodec_find_decoder(codec_params->codec_id);
        if (!codec) return false;
        
        codec_ctx = avcodec_alloc_context3(codec);
        if (avcodec_parameters_to_context(codec_ctx, codec_params) < 0) return false;
        if (avcodec_open2(codec_ctx, codec, nullptr) < 0) return false;
        
        // Allocate frames
        frame = av_frame_alloc();
        frame_rgb = av_frame_alloc();
        packet = av_packet_alloc();
        
        // Setup conversion
        int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, codec_ctx->width, codec_ctx->height, 1);
        buffer = (uint8_t*)av_malloc(num_bytes * sizeof(uint8_t));
        av_image_fill_arrays(frame_rgb->data, frame_rgb->linesize, buffer, AV_PIX_FMT_BGR24, codec_ctx->width, codec_ctx->height, 1);
        
        sws_ctx = sws_getContext(codec_ctx->width, codec_ctx->height, codec_ctx->pix_fmt,
                                codec_ctx->width, codec_ctx->height, AV_PIX_FMT_BGR24,
                                SWS_BILINEAR, nullptr, nullptr, nullptr);
        
        return true;
    }
    
    bool getNextFrame(cv::Mat& frame_out) {
        while (av_read_frame(format_ctx, packet) >= 0) {
            if (packet->stream_index == video_stream_index) {
                if (avcodec_send_packet(codec_ctx, packet) >= 0) {
                    if (avcodec_receive_frame(codec_ctx, frame) == 0) {
                        sws_scale(sws_ctx, frame->data, frame->linesize, 0, codec_ctx->height,
                                 frame_rgb->data, frame_rgb->linesize);
                        
                        frame_out = cv::Mat(codec_ctx->height, codec_ctx->width, CV_8UC3, 
                                          frame_rgb->data[0], frame_rgb->linesize[0]).clone();
                        
                        av_packet_unref(packet);
                        return true;
                    }
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

struct CameraConfig {
    std::string id;
    std::string name;
    std::string rtsp_url;
    int mjpeg_port;
};

class RealCameraMJPEGTest {
private:
    std::vector<CameraConfig> cameras;
    std::vector<std::unique_ptr<SimpleFFmpegDecoder>> decoders;
    std::vector<std::unique_ptr<MJPEGServer>> servers;
    std::vector<std::thread> processing_threads;
    bool running;

public:
    RealCameraMJPEGTest() : running(false) {
        cameras = {
            {"camera_01", "Real Camera 1", "rtsp://admin:sharpi1688@192.168.1.2:554/1/1", 8161},
            {"camera_02", "Real Camera 2", "rtsp://admin:sharpi1688@192.168.1.3:554/1/1", 8162}
        };
    }

    bool initialize() {
        std::cout << "=== Real Camera MJPEG Test ===" << std::endl;
        
        for (const auto& camera : cameras) {
            std::cout << "\nInitializing " << camera.name << "..." << std::endl;
            
            auto decoder = std::make_unique<SimpleFFmpegDecoder>();
            if (!decoder->initialize(camera.rtsp_url)) {
                std::cerr << "Failed to initialize decoder for " << camera.name << std::endl;
                continue;
            }
            
            auto server = std::make_unique<MJPEGServer>(camera.mjpeg_port);
            if (!server->start()) {
                std::cerr << "Failed to start MJPEG server for " << camera.name << std::endl;
                continue;
            }
            
            decoders.push_back(std::move(decoder));
            servers.push_back(std::move(server));
            
            std::cout << camera.name << " initialized successfully!" << std::endl;
            std::cout << "MJPEG stream: http://localhost:" << camera.mjpeg_port << std::endl;
        }
        
        return !decoders.empty();
    }

    void processCameraStream(size_t camera_index) {
        if (camera_index >= decoders.size()) return;
        
        const auto& camera = cameras[camera_index];
        auto& decoder = decoders[camera_index];
        auto& server = servers[camera_index];
        
        std::cout << "Starting processing thread for " << camera.name << std::endl;
        
        cv::Mat frame;
        int frame_count = 0;
        auto last_stats_time = std::chrono::steady_clock::now();
        
        while (running) {
            try {
                if (!decoder->getNextFrame(frame)) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    continue;
                }
                
                if (frame.empty()) continue;
                
                frame_count++;
                
                // Add simple overlay
                cv::putText(frame, camera.name, cv::Point(10, 30), 
                           cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
                
                std::string frame_text = "Frame: " + std::to_string(frame_count);
                cv::putText(frame, frame_text, cv::Point(10, frame.rows - 20), 
                           cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 0), 2);
                
                // Update MJPEG server
                server->updateFrame(frame);
                
                // Print statistics every 5 seconds
                auto current_time = std::chrono::steady_clock::now();
                auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(current_time - last_stats_time).count();
                if (elapsed >= 5) {
                    double fps = frame_count / 5.0;
                    std::cout << "[" << camera.name << "] FPS: " << std::fixed << std::setprecision(1) << fps
                              << ", Frames: " << frame_count << std::endl;
                    
                    frame_count = 0;
                    last_stats_time = current_time;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "[" << camera.name << "] Error: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
        }
        
        std::cout << "Processing thread for " << camera.name << " stopped." << std::endl;
    }

    void start() {
        running = true;
        
        std::cout << "\n=== Starting Real Camera Processing ===" << std::endl;
        std::cout << "MJPEG streams available at:" << std::endl;
        for (size_t i = 0; i < cameras.size() && i < decoders.size(); ++i) {
            std::cout << "- " << cameras[i].name << ": http://localhost:" << cameras[i].mjpeg_port << std::endl;
        }
        std::cout << std::endl;
        
        for (size_t i = 0; i < decoders.size(); ++i) {
            processing_threads.emplace_back(&RealCameraMJPEGTest::processCameraStream, this, i);
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
        
        for (auto& server : servers) {
            server->stop();
        }
        
        std::cout << "All processing threads stopped." << std::endl;
    }
};

int main() {
    std::cout << "=== Real Camera MJPEG Test ===" << std::endl;
    std::cout << "Testing real RTSP cameras with MJPEG HTTP streaming" << std::endl;
    
    RealCameraMJPEGTest test;
    
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
