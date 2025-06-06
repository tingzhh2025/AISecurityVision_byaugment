/**
 * @file CUDAUtils.h
 * @brief CUDA utility functions for TensorRT integration
 */

#ifndef CUDA_UTILS_H
#define CUDA_UTILS_H

#ifdef HAVE_TENSORRT

#include <cuda_runtime.h>
#include <iostream>
#include <memory>
#include <vector>

namespace AISecurityVision {

// CUDA error checking macro
#define CUDA_CHECK(call) \
    do { \
        cudaError_t error = call; \
        if (error != cudaSuccess) { \
            std::cerr << "CUDA error at " << __FILE__ << ":" << __LINE__ \
                     << " code=" << error << " \"" << cudaGetErrorString(error) << "\"" << std::endl; \
            return false; \
        } \
    } while(0)

// CUDA stream wrapper
class CudaStream {
public:
    CudaStream() {
        cudaStreamCreate(&stream_);
    }
    
    ~CudaStream() {
        cudaStreamDestroy(stream_);
    }
    
    cudaStream_t get() const { return stream_; }
    
    void synchronize() {
        cudaStreamSynchronize(stream_);
    }
    
private:
    cudaStream_t stream_;
};

// CUDA event wrapper
class CudaEvent {
public:
    CudaEvent() {
        cudaEventCreate(&event_);
    }
    
    ~CudaEvent() {
        cudaEventDestroy(event_);
    }
    
    void record(cudaStream_t stream = 0) {
        cudaEventRecord(event_, stream);
    }
    
    void synchronize() {
        cudaEventSynchronize(event_);
    }
    
    float elapsedTime(const CudaEvent& start) {
        float ms;
        cudaEventElapsedTime(&ms, start.event_, event_);
        return ms;
    }
    
private:
    cudaEvent_t event_;
};

// CUDA memory management
template<typename T>
class CudaBuffer {
public:
    CudaBuffer() : size_(0), device_ptr_(nullptr) {}
    
    explicit CudaBuffer(size_t size) : size_(size) {
        allocate();
    }
    
    ~CudaBuffer() {
        free();
    }
    
    // Disable copy
    CudaBuffer(const CudaBuffer&) = delete;
    CudaBuffer& operator=(const CudaBuffer&) = delete;
    
    // Enable move
    CudaBuffer(CudaBuffer&& other) noexcept
        : size_(other.size_), device_ptr_(other.device_ptr_) {
        other.size_ = 0;
        other.device_ptr_ = nullptr;
    }
    
    CudaBuffer& operator=(CudaBuffer&& other) noexcept {
        if (this != &other) {
            free();
            size_ = other.size_;
            device_ptr_ = other.device_ptr_;
            other.size_ = 0;
            other.device_ptr_ = nullptr;
        }
        return *this;
    }
    
    bool allocate(size_t size) {
        free();
        size_ = size;
        return allocate();
    }
    
    bool copyFrom(const T* host_data, size_t count, cudaStream_t stream = 0) {
        if (!device_ptr_ || count > size_) return false;
        return cudaMemcpyAsync(device_ptr_, host_data, count * sizeof(T), 
                              cudaMemcpyHostToDevice, stream) == cudaSuccess;
    }
    
    bool copyTo(T* host_data, size_t count, cudaStream_t stream = 0) {
        if (!device_ptr_ || count > size_) return false;
        return cudaMemcpyAsync(host_data, device_ptr_, count * sizeof(T), 
                              cudaMemcpyDeviceToHost, stream) == cudaSuccess;
    }
    
    T* get() { return device_ptr_; }
    const T* get() const { return device_ptr_; }
    size_t size() const { return size_; }
    
private:
    size_t size_;
    T* device_ptr_;
    
    bool allocate() {
        if (size_ == 0) return true;
        return cudaMalloc(&device_ptr_, size_ * sizeof(T)) == cudaSuccess;
    }
    
    void free() {
        if (device_ptr_) {
            cudaFree(device_ptr_);
            device_ptr_ = nullptr;
        }
        size_ = 0;
    }
};

// CUDA device info utility
class CudaDeviceInfo {
public:
    static bool getDeviceInfo(int device = 0) {
        cudaDeviceProp prop;
        if (cudaGetDeviceProperties(&prop, device) != cudaSuccess) {
            return false;
        }
        
        std::cout << "CUDA Device " << device << ": " << prop.name << std::endl;
        std::cout << "  Compute capability: " << prop.major << "." << prop.minor << std::endl;
        std::cout << "  Total memory: " << prop.totalGlobalMem / (1024 * 1024) << " MB" << std::endl;
        std::cout << "  Multiprocessors: " << prop.multiProcessorCount << std::endl;
        std::cout << "  Max threads per block: " << prop.maxThreadsPerBlock << std::endl;
        
        return true;
    }
    
    static int getDeviceCount() {
        int count = 0;
        cudaGetDeviceCount(&count);
        return count;
    }
};

} // namespace AISecurityVision

#endif // HAVE_TENSORRT

#endif // CUDA_UTILS_H
