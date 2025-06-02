#include "src/ai/ByteTracker.h"
#include "src/core/Logger.h"
#include <iostream>

using namespace AISecurityVision;

int main() {
    std::cout << "Starting ByteTracker test..." << std::endl;
    
    try {
        std::cout << "Creating ByteTracker..." << std::endl;
        auto tracker = std::make_unique<ByteTracker>();
        
        std::cout << "Initializing ByteTracker..." << std::endl;
        bool result = tracker->initialize();
        
        std::cout << "ByteTracker initialization result: " << (result ? "SUCCESS" : "FAILED") << std::endl;
        
        std::cout << "Test completed successfully!" << std::endl;
        return 0;
        
    } catch (const std::exception& e) {
        std::cout << "Exception caught: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cout << "Unknown exception caught!" << std::endl;
        return 1;
    }
}
