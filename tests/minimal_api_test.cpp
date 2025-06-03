#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include "../src/database/DatabaseManager.h"
#include "../src/utils/Logger.h"

int main() {
    std::cout << "=== Minimal API Test ===" << std::endl;
    
    try {
        // Test database access directly
        std::cout << "Testing database access..." << std::endl;
        
        DatabaseManager dbManager;
        std::string configKey = "person_stats_test_camera";
        
        // Try to read configuration
        std::cout << "Reading configuration from database..." << std::endl;
        std::string configValue = dbManager.getConfigValue("person_statistics", configKey);
        
        if (configValue.empty()) {
            std::cout << "No configuration found, creating default..." << std::endl;
            
            // Create default configuration
            nlohmann::json defaultConfig;
            defaultConfig["enabled"] = false;
            defaultConfig["gender_threshold"] = 0.7;
            defaultConfig["age_threshold"] = 0.6;
            defaultConfig["batch_size"] = 4;
            defaultConfig["enable_caching"] = true;
            
            std::string defaultConfigStr = defaultConfig.dump();
            
            if (dbManager.setConfigValue("person_statistics", configKey, defaultConfigStr)) {
                std::cout << "✅ Default configuration saved successfully" << std::endl;
            } else {
                std::cout << "❌ Failed to save default configuration" << std::endl;
                return 1;
            }
            
            configValue = defaultConfigStr;
        }
        
        std::cout << "Configuration: " << configValue << std::endl;
        
        // Parse the JSON
        nlohmann::json configJson = nlohmann::json::parse(configValue);
        
        bool enabled = configJson.value("enabled", false);
        float genderThreshold = configJson.value("gender_threshold", 0.7f);
        float ageThreshold = configJson.value("age_threshold", 0.6f);
        int batchSize = configJson.value("batch_size", 4);
        bool enableCaching = configJson.value("enable_caching", true);
        
        std::cout << "Parsed configuration:" << std::endl;
        std::cout << "  enabled: " << (enabled ? "true" : "false") << std::endl;
        std::cout << "  gender_threshold: " << genderThreshold << std::endl;
        std::cout << "  age_threshold: " << ageThreshold << std::endl;
        std::cout << "  batch_size: " << batchSize << std::endl;
        std::cout << "  enable_caching: " << (enableCaching ? "true" : "false") << std::endl;
        
        // Test updating configuration
        std::cout << "\nTesting configuration update..." << std::endl;
        
        configJson["enabled"] = true;
        configJson["gender_threshold"] = 0.8;
        
        std::string updatedConfigStr = configJson.dump();
        
        if (dbManager.setConfigValue("person_statistics", configKey, updatedConfigStr)) {
            std::cout << "✅ Configuration updated successfully" << std::endl;
        } else {
            std::cout << "❌ Failed to update configuration" << std::endl;
            return 1;
        }
        
        // Read back to verify
        std::string verifyConfig = dbManager.getConfigValue("person_statistics", configKey);
        std::cout << "Verified configuration: " << verifyConfig << std::endl;
        
        std::cout << "\n✅ Database test completed successfully" << std::endl;
        
    } catch (const std::exception& e) {
        std::cout << "❌ Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
