#include <iostream>
#include <opencv2/opencv.hpp>
#include <chrono>
#include <thread>

int main() {
    std::cout << "=== OpenCV RTSP Connection Test ===" << std::endl;
    std::cout << "OpenCV Version: " << CV_VERSION << std::endl;
    
    // Test different RTSP URLs
    std::vector<std::string> rtsp_urls = {
        "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
        "rtsp://admin:sharpi1688@192.168.1.3:554/1/1",
        "rtsp://admin:sharpi1688@192.168.1.2:554/",
        "rtsp://admin:sharpi1688@192.168.1.3:554/",
        "rtsp://admin:sharpi1688@192.168.1.2:554/stream1",
        "rtsp://admin:sharpi1688@192.168.1.3:554/stream1"
    };
    
    for (const auto& url : rtsp_urls) {
        std::cout << "\n--- Testing URL: " << url << " ---" << std::endl;
        
        cv::VideoCapture cap;
        
        // Set various properties before opening
        cap.set(cv::CAP_PROP_BUFFERSIZE, 1);
        cap.set(cv::CAP_PROP_FOURCC, cv::VideoWriter::fourcc('H','2','6','4'));
        
        std::cout << "Opening stream..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        bool opened = cap.open(url);
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
        
        std::cout << "Open attempt took: " << duration.count() << "ms" << std::endl;
        
        if (opened) {
            std::cout << "✅ Successfully opened stream!" << std::endl;
            
            // Get stream properties
            double fps = cap.get(cv::CAP_PROP_FPS);
            double width = cap.get(cv::CAP_PROP_FRAME_WIDTH);
            double height = cap.get(cv::CAP_PROP_FRAME_HEIGHT);
            double fourcc = cap.get(cv::CAP_PROP_FOURCC);
            
            std::cout << "Stream properties:" << std::endl;
            std::cout << "  FPS: " << fps << std::endl;
            std::cout << "  Resolution: " << width << "x" << height << std::endl;
            std::cout << "  FOURCC: " << fourcc << std::endl;
            
            // Try to read a few frames
            cv::Mat frame;
            int frame_count = 0;
            int max_frames = 5;
            
            std::cout << "Attempting to read " << max_frames << " frames..." << std::endl;
            
            for (int i = 0; i < max_frames; ++i) {
                auto frame_start = std::chrono::high_resolution_clock::now();
                
                bool success = cap.read(frame);
                
                auto frame_end = std::chrono::high_resolution_clock::now();
                auto frame_duration = std::chrono::duration_cast<std::chrono::milliseconds>(frame_end - frame_start);
                
                if (success && !frame.empty()) {
                    frame_count++;
                    std::cout << "  Frame " << (i+1) << ": " << frame.cols << "x" << frame.rows 
                              << " (" << frame_duration.count() << "ms)" << std::endl;
                    
                    // Save first frame as test
                    if (i == 0) {
                        std::string filename = "test_frame_" + std::to_string(i+1) + ".jpg";
                        cv::imwrite(filename, frame);
                        std::cout << "    Saved as: " << filename << std::endl;
                    }
                } else {
                    std::cout << "  Frame " << (i+1) << ": Failed to read (" 
                              << frame_duration.count() << "ms)" << std::endl;
                }
                
                // Small delay between frames
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            
            std::cout << "Successfully read " << frame_count << "/" << max_frames << " frames" << std::endl;
            
            cap.release();
            
            if (frame_count > 0) {
                std::cout << "🎉 This URL works! Use this for your application." << std::endl;
                break; // Found working URL, stop testing
            }
            
        } else {
            std::cout << "❌ Failed to open stream" << std::endl;
            
            // Try with different backend
            std::cout << "Trying with FFMPEG backend..." << std::endl;
            cap.open(url, cv::CAP_FFMPEG);
            if (cap.isOpened()) {
                std::cout << "✅ FFMPEG backend worked!" << std::endl;
                cap.release();
            } else {
                std::cout << "❌ FFMPEG backend also failed" << std::endl;
            }
        }
        
        std::cout << "Waiting 2 seconds before next test..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Check the saved frame files to verify video quality." << std::endl;
    
    return 0;
}
