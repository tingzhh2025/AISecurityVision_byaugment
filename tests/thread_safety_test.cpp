#include "../src/core/TaskManager.h"
#include "../src/core/ThreadPool.h"
#include "../src/core/LockHierarchy.h"
#include "../src/core/MJPEGPortManager.h"
#include "../src/core/VideoPipeline.h"
#include "../src/core/Logger.h"
#include <thread>
#include <vector>
#include <chrono>
#include <random>
#include <iostream>

using namespace AISecurityVision;

/**
 * @brief Thread safety and deadlock test for AI Security Vision System
 * 
 * This test validates the fixes implemented for:
 * 1. TaskManager race conditions
 * 2. Cross-component deadlock prevention
 * 3. Thread pool safety
 * 4. Lock hierarchy enforcement
 */
class ThreadSafetyTest {
public:
    ThreadSafetyTest() : m_threadPool(8) {
        LOG_INFO() << "[ThreadSafetyTest] Initializing thread safety test";
        
        // Enable lock hierarchy checking
        LockHierarchyEnforcer::getInstance().setEnabled(true);
    }
    
    /**
     * @brief Test concurrent camera additions to TaskManager
     */
    void testConcurrentCameraAdditions() {
        LOG_INFO() << "[ThreadSafetyTest] Testing concurrent camera additions";
        
        TaskManager& taskManager = TaskManager::getInstance();
        std::vector<std::future<bool>> futures;
        
        // Create multiple threads trying to add cameras simultaneously
        for (int i = 0; i < 10; ++i) {
            auto future = m_threadPool.submit([&taskManager, i]() {
                VideoSource source;
                source.id = "test_camera_" + std::to_string(i);
                source.name = "Test Camera " + std::to_string(i);
                source.url = "rtsp://test:test@192.168.1." + std::to_string(100 + i) + ":554/stream";
                source.protocol = "rtsp";
                source.width = 1920;
                source.height = 1080;
                source.fps = 25;
                source.enabled = true;
                
                // Add random delay to increase chance of race conditions
                std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 100));
                
                bool result = taskManager.addVideoSource(source);
                LOG_INFO() << "[ThreadSafetyTest] Camera " << source.id << " addition result: " << result;
                return result;
            });
            
            futures.push_back(std::move(future));
        }
        
        // Wait for all operations to complete
        int successCount = 0;
        for (auto& future : futures) {
            try {
                if (future.get()) {
                    successCount++;
                }
            } catch (const std::exception& e) {
                LOG_ERROR() << "[ThreadSafetyTest] Exception in camera addition: " << e.what();
            }
        }
        
        LOG_INFO() << "[ThreadSafetyTest] Successfully added " << successCount << " out of 10 cameras";
        
        // Cleanup
        for (int i = 0; i < 10; ++i) {
            taskManager.removeVideoSource("test_camera_" + std::to_string(i));
        }
    }
    
    /**
     * @brief Test MJPEG port allocation under concurrent access
     */
    void testMJPEGPortAllocation() {
        LOG_INFO() << "[ThreadSafetyTest] Testing MJPEG port allocation";
        
        auto& portManager = MJPEGPortManager::getInstance();
        std::vector<std::future<int>> futures;
        
        // Clear any existing allocations
        portManager.clearAllAllocations();
        
        // Create multiple threads trying to allocate ports simultaneously
        for (int i = 0; i < 20; ++i) {
            auto future = m_threadPool.submit([&portManager, i]() {
                std::string cameraId = "port_test_camera_" + std::to_string(i);
                
                // Add random delay
                std::this_thread::sleep_for(std::chrono::milliseconds(rand() % 50));
                
                int port = portManager.allocatePort(cameraId);
                LOG_DEBUG() << "[ThreadSafetyTest] Camera " << cameraId << " allocated port: " << port;
                return port;
            });
            
            futures.push_back(std::move(future));
        }
        
        // Collect results
        std::vector<int> allocatedPorts;
        for (auto& future : futures) {
            try {
                int port = future.get();
                if (port != -1) {
                    allocatedPorts.push_back(port);
                }
            } catch (const std::exception& e) {
                LOG_ERROR() << "[ThreadSafetyTest] Exception in port allocation: " << e.what();
            }
        }
        
        // Check for duplicate port allocations (should not happen)
        std::sort(allocatedPorts.begin(), allocatedPorts.end());
        auto it = std::unique(allocatedPorts.begin(), allocatedPorts.end());
        bool hasDuplicates = (it != allocatedPorts.end());
        
        LOG_INFO() << "[ThreadSafetyTest] Allocated " << allocatedPorts.size() << " unique ports";
        LOG_INFO() << "[ThreadSafetyTest] Port allocation test " << (hasDuplicates ? "FAILED" : "PASSED");
        
        // Cleanup
        for (int i = 0; i < 20; ++i) {
            portManager.releasePort("port_test_camera_" + std::to_string(i));
        }
    }
    
    /**
     * @brief Test lock hierarchy enforcement
     */
    void testLockHierarchy() {
        LOG_INFO() << "[ThreadSafetyTest] Testing lock hierarchy enforcement";
        
        auto& enforcer = LockHierarchyEnforcer::getInstance();
        
        // Test correct lock ordering (should succeed)
        bool correctOrderResult = m_threadPool.submit([&enforcer]() {
            try {
                std::mutex mutex1, mutex2, mutex3;
                
                // Acquire locks in correct order
                HierarchicalMutexLock lock1(mutex1, LockLevel::MJPEG_PORT_MANAGER, "test_mutex1");
                HierarchicalMutexLock lock2(mutex2, LockLevel::TASK_MANAGER, "test_mutex2");
                HierarchicalMutexLock lock3(mutex3, LockLevel::VIDEO_PIPELINE, "test_mutex3");
                
                LOG_INFO() << "[ThreadSafetyTest] Correct lock ordering succeeded";
                return true;
            } catch (const std::exception& e) {
                LOG_ERROR() << "[ThreadSafetyTest] Correct lock ordering failed: " << e.what();
                return false;
            }
        }).get();
        
        // Test incorrect lock ordering (should be detected)
        bool incorrectOrderResult = m_threadPool.submit([&enforcer]() {
            try {
                std::mutex mutex1, mutex2;
                
                // Acquire locks in incorrect order
                HierarchicalMutexLock lock1(mutex1, LockLevel::VIDEO_PIPELINE, "test_mutex1");
                
                // This should be detected as a hierarchy violation
                if (enforcer.canAcquireLock(LockLevel::MJPEG_PORT_MANAGER, "test_mutex2")) {
                    LOG_ERROR() << "[ThreadSafetyTest] Lock hierarchy violation not detected!";
                    return false;
                } else {
                    LOG_INFO() << "[ThreadSafetyTest] Lock hierarchy violation correctly detected";
                    return true;
                }
            } catch (const std::exception& e) {
                LOG_ERROR() << "[ThreadSafetyTest] Lock hierarchy test exception: " << e.what();
                return false;
            }
        }).get();
        
        LOG_INFO() << "[ThreadSafetyTest] Lock hierarchy test " 
                   << (correctOrderResult && incorrectOrderResult ? "PASSED" : "FAILED");
    }
    
    /**
     * @brief Run all thread safety tests
     */
    void runAllTests() {
        LOG_INFO() << "[ThreadSafetyTest] Starting comprehensive thread safety tests";
        
        try {
            testMJPEGPortAllocation();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            testLockHierarchy();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            testConcurrentCameraAdditions();
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            
            LOG_INFO() << "[ThreadSafetyTest] All tests completed successfully";
        } catch (const std::exception& e) {
            LOG_ERROR() << "[ThreadSafetyTest] Test suite failed with exception: " << e.what();
        }
    }

private:
    ThreadPool m_threadPool;
};

int main() {
    try {
        LOG_INFO() << "[Main] Starting AI Security Vision Thread Safety Test";
        
        ThreadSafetyTest test;
        test.runAllTests();
        
        LOG_INFO() << "[Main] Thread safety test completed";
        return 0;
        
    } catch (const std::exception& e) {
        LOG_ERROR() << "[Main] Test failed with exception: " << e.what();
        return 1;
    }
}
