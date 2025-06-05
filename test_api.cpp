#include <iostream>
#include <string>
#include <curl/curl.h>

// Callback function to write response data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t totalSize = size * nmemb;
    response->append((char*)contents, totalSize);
    return totalSize;
}

// Function to make HTTP GET request
std::string makeGetRequest(const std::string& url) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            return "Error: " + std::string(curl_easy_strerror(res));
        }
    }
    return response;
}

// Function to make HTTP PUT request
std::string makePutRequest(const std::string& url, const std::string& data) {
    CURL* curl;
    CURLcode res;
    std::string response;

    curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        
        struct curl_slist* headers = nullptr;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        res = curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        
        if (res != CURLE_OK) {
            return "Error: " + std::string(curl_easy_strerror(res));
        }
    }
    return response;
}

int main() {
    std::cout << "=== Testing AI Security Vision API Endpoints ===" << std::endl;
    
    // Test 1: Get cameras list
    std::cout << "\n1. Testing GET /api/cameras" << std::endl;
    std::string response = makeGetRequest("http://localhost:8080/api/cameras");
    std::cout << "Response: " << response.substr(0, 200) << "..." << std::endl;
    
    // Test 2: Get specific camera
    std::cout << "\n2. Testing GET /api/cameras/camera_ch2" << std::endl;
    response = makeGetRequest("http://localhost:8080/api/cameras/camera_ch2");
    std::cout << "Response: " << response.substr(0, 200) << "..." << std::endl;
    
    // Test 3: Get detection stats
    std::cout << "\n3. Testing GET /api/detection/stats" << std::endl;
    response = makeGetRequest("http://localhost:8080/api/detection/stats");
    std::cout << "Response: " << response.substr(0, 200) << "..." << std::endl;
    
    // Test 4: Update detection config
    std::cout << "\n4. Testing PUT /api/detection/config" << std::endl;
    std::string configData = R"({"confidence_threshold": 0.6, "nms_threshold": 0.5, "max_detections": 50, "detection_interval": 2, "detection_enabled": true})";

    response = makePutRequest("http://localhost:8080/api/detection/config", configData);
    std::cout << "Response: " << response.substr(0, 200) << "..." << std::endl;

    // Test 5: Update camera config
    std::cout << "\n5. Testing PUT /api/cameras/camera_ch2" << std::endl;
    std::string cameraData = R"({"name": "Updated Test Camera", "detection_enabled": true, "recording_enabled": true})";

    response = makePutRequest("http://localhost:8080/api/cameras/camera_ch2", cameraData);
    std::cout << "Response: " << response.substr(0, 200) << "..." << std::endl;
    
    std::cout << "\n=== API Testing Complete ===" << std::endl;
    return 0;
}
