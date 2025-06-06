/**
 * @file YOLOv8DetectorFactory.cpp
 * @brief Factory implementation for creating YOLOv8 detector instances
 */

#include "YOLOv8DetectorFactory.h"
#include "YOLOv8Detector.h"
#include "YOLOv8CPUDetector.h"

#ifdef HAVE_RKNN
#include "YOLOv8RKNNDetector.h"
#endif

#ifdef HAVE_TENSORRT
#include "YOLOv8TensorRTDetector.h"
#endif

#include <fstream>
#include <sstream>
#include <algorithm>

#ifdef HAVE_TENSORRT
#include <cuda_runtime.h>
#include <NvInfer.h>
#endif

#ifdef __linux__
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace AISecurityVision {

std::unique_ptr<YOLOv8Detector> YOLOv8DetectorFactory::createDetector(
    InferenceBackend preferredBackend) {
    
    // If AUTO, determine best backend
    if (preferredBackend == InferenceBackend::CPU) {
        auto backends = getAvailableBackends();
        
        // Priority order: TensorRT > RKNN > CPU
        if (std::find(backends.begin(), backends.end(), InferenceBackend::TENSORRT) != backends.end()) {
            preferredBackend = InferenceBackend::TENSORRT;
        } else if (std::find(backends.begin(), backends.end(), InferenceBackend::RKNN) != backends.end()) {
            preferredBackend = InferenceBackend::RKNN;
        }
        
        std::cout << "Auto-selected backend: " << getBackendName(preferredBackend) << std::endl;
    }
    
    // Create detector based on backend
    switch (preferredBackend) {
#ifdef HAVE_TENSORRT
        case InferenceBackend::TENSORRT:
            if (hasTensorRTSupport()) {
                std::cout << "Creating TensorRT detector..." << std::endl;
                return std::make_unique<YOLOv8TensorRTDetector>();
            }
            std::cerr << "TensorRT requested but not available, falling back to CPU" << std::endl;
            break;
#endif

#ifdef HAVE_RKNN
        case InferenceBackend::RKNN:
            if (hasRKNNSupport()) {
                std::cout << "Creating RKNN detector..." << std::endl;
                return std::make_unique<YOLOv8RKNNDetector>();
            }
            std::cerr << "RKNN requested but not available, falling back to CPU" << std::endl;
            break;
#endif

        case InferenceBackend::ONNX:
        case InferenceBackend::CPU:
        default:
            std::cout << "Creating CPU detector..." << std::endl;
            return std::make_unique<YOLOv8CPUDetector>();
    }
    
    // Should not reach here
    return std::make_unique<YOLOv8CPUDetector>();
}

std::vector<InferenceBackend> YOLOv8DetectorFactory::getAvailableBackends() {
    std::vector<InferenceBackend> backends;
    
#ifdef HAVE_TENSORRT
    if (hasTensorRTSupport()) {
        backends.push_back(InferenceBackend::TENSORRT);
    }
#endif

#ifdef HAVE_RKNN
    if (hasRKNNSupport()) {
        backends.push_back(InferenceBackend::RKNN);
    }
#endif

    // CPU is always available
    backends.push_back(InferenceBackend::CPU);
    
    return backends;
}

bool YOLOv8DetectorFactory::isBackendAvailable(InferenceBackend backend) {
    switch (backend) {
        case InferenceBackend::TENSORRT:
            return hasTensorRTSupport();
        case InferenceBackend::RKNN:
            return hasRKNNSupport();
        case InferenceBackend::CPU:
        case InferenceBackend::ONNX:
            return true;
        default:
            return false;
    }
}

std::string YOLOv8DetectorFactory::getBackendName(InferenceBackend backend) {
    switch (backend) {
        case InferenceBackend::TENSORRT:
            return "TensorRT GPU";
        case InferenceBackend::RKNN:
            return "RKNN NPU";
        case InferenceBackend::ONNX:
            return "ONNX Runtime";
        case InferenceBackend::CPU:
            return "CPU";
        default:
            return "Unknown";
    }
}

std::string YOLOv8DetectorFactory::getSystemInfo() {
    std::stringstream ss;
    
    ss << "=== System Information ===" << std::endl;
    
#ifdef __linux__
    // Get system info
    struct utsname unameData;
    if (uname(&unameData) == 0) {
        ss << "System: " << unameData.sysname << " " << unameData.release << std::endl;
        ss << "Machine: " << unameData.machine << std::endl;
    }
    
    // Get hostname
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        ss << "Hostname: " << hostname << std::endl;
    }
#endif
    
    // CPU info
    ss << std::endl << "=== CPU Information ===" << std::endl;
#ifdef __linux__
    std::ifstream cpuinfo("/proc/cpuinfo");
    if (cpuinfo.is_open()) {
        std::string line;
        bool foundModel = false;
        int coreCount = 0;
        
        while (std::getline(cpuinfo, line)) {
            if (!foundModel && line.find("model name") != std::string::npos) {
                size_t pos = line.find(':');
                if (pos != std::string::npos) {
                    ss << "CPU Model: " << line.substr(pos + 2) << std::endl;
                    foundModel = true;
                }
            }
            if (line.find("processor") != std::string::npos) {
                coreCount++;
            }
        }
        ss << "CPU Cores: " << coreCount << std::endl;
        cpuinfo.close();
    }
#endif
    
    // Check for Rockchip platform
    ss << std::endl << "=== Platform Detection ===" << std::endl;
    if (isRockchipPlatform()) {
        ss << "Rockchip platform detected" << std::endl;
        
        // Try to identify specific model
        std::ifstream deviceTree("/proc/device-tree/model");
        if (deviceTree.is_open()) {
            std::string model;
            std::getline(deviceTree, model);
            ss << "Device Model: " << model << std::endl;
            deviceTree.close();
        }
    }
    
    // GPU/NPU info
    ss << std::endl << "=== Acceleration Hardware ===" << std::endl;
    
#ifdef HAVE_TENSORRT
    if (hasCUDASupport()) {
        ss << "CUDA: Available" << std::endl;
        
        // Get CUDA device info
        int deviceCount = 0;
        if (cudaGetDeviceCount(&deviceCount) == cudaSuccess) {
            ss << "CUDA Devices: " << deviceCount << std::endl;
            
            for (int i = 0; i < deviceCount; ++i) {
                cudaDeviceProp prop;
                if (cudaGetDeviceProperties(&prop, i) == cudaSuccess) {
                    ss << "  Device " << i << ": " << prop.name << std::endl;
                    ss << "    Compute Capability: " << prop.major << "." << prop.minor << std::endl;
                    ss << "    Total Memory: " << (prop.totalGlobalMem / (1024 * 1024)) << " MB" << std::endl;
                }
            }
        }
        
        if (hasTensorRTSupport()) {
            ss << "TensorRT: Available" << std::endl;
        } else {
            ss << "TensorRT: Not Available" << std::endl;
        }
    } else {
        ss << "CUDA: Not Available" << std::endl;
    }
#else
    ss << "CUDA/TensorRT: Not compiled in" << std::endl;
#endif

#ifdef HAVE_RKNN
    if (hasRKNNSupport()) {
        ss << "RKNN NPU: Available" << std::endl;
    } else {
        ss << "RKNN NPU: Not Available" << std::endl;
    }
#else
    ss << "RKNN NPU: Not compiled in" << std::endl;
#endif
    
    // Available backends summary
    ss << std::endl << "=== Available Backends ===" << std::endl;
    auto backends = getAvailableBackends();
    for (const auto& backend : backends) {
        ss << "- " << getBackendName(backend) << std::endl;
    }
    
    return ss.str();
}

bool YOLOv8DetectorFactory::isRockchipPlatform() {
#ifdef __linux__
    // Check for Rockchip-specific files
    std::ifstream rk3588("/sys/devices/platform/fd8d8000.npu/devfreq/fd8d8000.npu/cur_freq");
    if (rk3588.good()) {
        return true;
    }
    
    // Check device tree for Rockchip
    std::ifstream compatible("/proc/device-tree/compatible");
    if (compatible.is_open()) {
        std::string content;
        std::getline(compatible, content, '\0');
        compatible.close();
        
        if (content.find("rockchip") != std::string::npos) {
            return true;
        }
    }
    
    // Check for RKNN driver
    std::ifstream rknnDriver("/dev/rknpu");
    if (rknnDriver.good()) {
        return true;
    }
#endif
    
    return false;
}

bool YOLOv8DetectorFactory::hasCUDASupport() {
#ifdef HAVE_TENSORRT
    int deviceCount = 0;
    cudaError_t error = cudaGetDeviceCount(&deviceCount);
    return (error == cudaSuccess && deviceCount > 0);
#else
    return false;
#endif
}

bool YOLOv8DetectorFactory::hasTensorRTSupport() {
#ifdef HAVE_TENSORRT
    // Check if CUDA is available first
    if (!hasCUDASupport()) {
        return false;
    }
    
    // Try to create a simple TensorRT logger to verify library is loaded
    try {
        class TestLogger : public nvinfer1::ILogger {
            void log(Severity severity, const char* msg) noexcept override {}
        };
        TestLogger logger;
        
        // Try to create a builder
        auto builder = std::unique_ptr<nvinfer1::IBuilder>(nvinfer1::createInferBuilder(logger));
        if (builder) {
            return true;
        }
    } catch (...) {
        // Library not properly loaded
    }
#endif
    
    return false;
}

bool YOLOv8DetectorFactory::hasRKNNSupport() {
#ifdef HAVE_RKNN
    // Check if we're on a Rockchip platform
    if (!isRockchipPlatform()) {
        return false;
    }
    
    // Try to check RKNN runtime version
    try {
        // This is a simple check - actual implementation might need
        // to try creating an RKNN context
        std::ifstream rknnDevice("/dev/rknpu");
        return rknnDevice.good();
    } catch (...) {
        // RKNN not available
    }
#endif
    
    return false;
}

} // namespace AISecurityVision
