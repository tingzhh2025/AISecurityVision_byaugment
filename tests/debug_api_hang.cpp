#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <curl/curl.h>

// Simple callback to capture response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* data) {
    size_t totalSize = size * nmemb;
    data->append((char*)contents, totalSize);
    return totalSize;
}

// Test function to make HTTP request with timeout
bool testAPIEndpoint(const std::string& url, int timeoutSeconds = 5) {
    CURL* curl;
    CURLcode res;
    std::string response;
    
    curl = curl_easy_init();
    if (!curl) {
        std::cerr << "Failed to initialize curl" << std::endl;
        return false;
    }
    
    // Set URL
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // Set callback function to capture response
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    
    // Set timeout
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeoutSeconds);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, timeoutSeconds);
    
    // Disable SSL verification for localhost
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    std::cout << "Testing: " << url << std::endl;
    auto start = std::chrono::steady_clock::now();
    
    // Perform the request
    res = curl_easy_perform(curl);
    
    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    if (res != CURLE_OK) {
        std::cerr << "Request failed: " << curl_easy_strerror(res) << std::endl;
        std::cerr << "Duration: " << duration.count() << "ms" << std::endl;
        curl_easy_cleanup(curl);
        return false;
    }
    
    long response_code;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
    
    std::cout << "Response code: " << response_code << std::endl;
    std::cout << "Duration: " << duration.count() << "ms" << std::endl;
    std::cout << "Response: " << response.substr(0, 200) << "..." << std::endl;
    
    curl_easy_cleanup(curl);
    return response_code == 200;
}

int main() {
    std::cout << "=== API Endpoint Debugging Tool ===" << std::endl;
    
    // Initialize curl globally
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    // Test different endpoints to isolate the hanging issue
    std::vector<std::string> endpoints = {
        "http://localhost:8080/api/cameras",
        "http://localhost:8080/api/system/status",
        "http://localhost:8080/api/cameras/test_camera/person-stats/config"
    };
    
    for (const auto& endpoint : endpoints) {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        
        bool success = testAPIEndpoint(endpoint, 10);
        
        if (success) {
            std::cout << "✅ SUCCESS" << std::endl;
        } else {
            std::cout << "❌ FAILED" << std::endl;
        }
        
        // Wait between requests
        std::this_thread::sleep_for(std::chrono::seconds(2));
    }
    
    // Test POST request
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << "Testing POST request..." << std::endl;
    
    CURL* curl = curl_easy_init();
    if (curl) {
        std::string response;
        std::string postData = R"({"enabled":true,"gender_threshold":0.8,"age_threshold":0.7,"batch_size":4,"enable_caching":true})";
        
        curl_easy_setopt(curl, CURLOPT_URL, "http://localhost:8080/api/cameras/test_camera/person-stats/config");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postData.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        auto start = std::chrono::steady_clock::now();
        CURLcode res = curl_easy_perform(curl);
        auto end = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            std::cout << "POST Response code: " << response_code << std::endl;
            std::cout << "POST Duration: " << duration.count() << "ms" << std::endl;
            std::cout << "POST Response: " << response.substr(0, 200) << "..." << std::endl;
        } else {
            std::cout << "POST Request failed: " << curl_easy_strerror(res) << std::endl;
        }
        
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }
    
    // Cleanup curl globally
    curl_global_cleanup();
    
    std::cout << "\n=== Debug Complete ===" << std::endl;
    return 0;
}
