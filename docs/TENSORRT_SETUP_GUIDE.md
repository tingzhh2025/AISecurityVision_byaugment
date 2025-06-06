# TensorRT Setup Guide for AISecurityVision

This guide explains how to set up NVIDIA TensorRT support for GPU-accelerated YOLOv8 inference in AISecurityVision.

## Prerequisites

### Hardware Requirements
- NVIDIA GPU with Compute Capability 6.1 or higher
- For desktop/server: GTX 1060 or newer
- For embedded: Jetson Nano, TX2, Xavier, or Orin

### Software Requirements
- CUDA 11.0 or higher
- cuDNN 8.0 or higher
- TensorRT 8.0 or higher
- CMake 3.16 or higher

## Installation

### 1. Install CUDA (if not already installed)

```bash
# Check if CUDA is installed
nvidia-smi
nvcc --version

# If not installed, download from NVIDIA website
# Ubuntu example:
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2004/x86_64/cuda-keyring_1.0-1_all.deb
sudo dpkg -i cuda-keyring_1.0-1_all.deb
sudo apt-get update
sudo apt-get install cuda
```

### 2. Install cuDNN

```bash
# Download cuDNN from NVIDIA Developer website (requires registration)
# Extract and install:
tar -xzvf cudnn-linux-x86_64-8.x.x.x_cuda11.x-archive.tar.xz
sudo cp cudnn-linux-x86_64-8.x.x.x_cuda11.x-archive/include/cudnn*.h /usr/local/cuda/include
sudo cp cudnn-linux-x86_64-8.x.x.x_cuda11.x-archive/lib/libcudnn* /usr/local/cuda/lib64
sudo chmod a+r /usr/local/cuda/include/cudnn*.h /usr/local/cuda/lib64/libcudnn*
```

### 3. Install TensorRT

#### Option A: Using NVIDIA Package Manager
```bash
# For Ubuntu
sudo apt-get install tensorrt

# Or download specific version from NVIDIA website
dpkg -i nv-tensorrt-repo-ubuntu2004-cuda11.x-trt8.x.x.x-ea-yyyymmdd_1-1_amd64.deb
sudo apt-get update
sudo apt-get install tensorrt
```

#### Option B: Using pip (for Python tools)
```bash
pip install tensorrt
```

### 4. Verify Installation

```bash
# Check TensorRT
dpkg -l | grep TensorRT

# Test with sample
cd /usr/src/tensorrt/samples/sampleMNIST
make
../../bin/sample_mnist
```

## Building AISecurityVision with TensorRT

### 1. Configure CMake

```bash
cd /home/rogers/source/custom/AISecurityVision_byaugment
mkdir -p build && cd build

# Enable TensorRT support
cmake .. \
    -DENABLE_CUDA_TENSORRT=ON \
    -DENABLE_RKNN_NPU=OFF \
    -DCMAKE_BUILD_TYPE=Release

# Or specify custom paths if needed
cmake .. \
    -DENABLE_CUDA_TENSORRT=ON \
    -DTENSORRT_ROOT=/path/to/tensorrt \
    -DCUDA_TOOLKIT_ROOT_DIR=/usr/local/cuda \
    -DCMAKE_BUILD_TYPE=Release
```

### 2. Build

```bash
make -j$(nproc)
```

## Model Preparation

### 1. Export YOLOv8 to ONNX

```python
from ultralytics import YOLO

# Load YOLOv8 model
model = YOLO('yolov8n.pt')  # or yolov8s.pt, yolov8m.pt, etc.

# Export to ONNX
model.export(format='onnx', imgsz=640, simplify=True)
```

### 2. Convert ONNX to TensorRT

```bash
# Use the provided conversion script
cd /home/rogers/source/custom/AISecurityVision_byaugment
python3 scripts/convert_yolov8_to_tensorrt.py models/yolov8n.onnx \
    --output models/yolov8n_fp16.engine \
    --precision FP16 \
    --batch-size 1 \
    --workspace 1024 \
    --verify

# For INT8 quantization (requires calibration data)
python3 scripts/convert_yolov8_to_tensorrt.py models/yolov8n.onnx \
    --output models/yolov8n_int8.engine \
    --precision INT8 \
    --batch-size 1
```

## Usage

### 1. Configuration

Edit your configuration file to use TensorRT:

```json
{
    "ai_config": {
        "backend": "TENSORRT",
        "model_path": "models/yolov8n_fp16.engine",
        "confidence_threshold": 0.25,
        "nms_threshold": 0.45,
        "input_size": [640, 640],
        "tensorrt_config": {
            "precision": "FP16",
            "max_batch_size": 1,
            "workspace_size": 1073741824,
            "enable_dla": false,
            "dla_core": -1
        }
    }
}
```

### 2. Runtime Selection

The system automatically selects the best available backend:

```cpp
// In your code
auto detector = YOLOv8DetectorFactory::createDetector();
// Will use TensorRT if available, fallback to RKNN or CPU
```

Or force a specific backend:

```cpp
auto detector = YOLOv8DetectorFactory::createDetector(InferenceBackend::TENSORRT);
```

## Performance Optimization

### 1. Precision Selection

- **FP32**: Highest accuracy, slowest performance
- **FP16**: Good balance of accuracy and speed (recommended)
- **INT8**: Fastest performance, requires calibration

### 2. Batch Processing

For multiple streams, use batch processing:

```json
{
    "tensorrt_config": {
        "max_batch_size": 4
    }
}
```

### 3. Multi-Stream Processing

Enable CUDA streams for concurrent inference:

```cpp
detector->enableMultiStream(true);
detector->setStreamCount(2);
```

## Troubleshooting

### Common Issues

1. **"Cannot find TensorRT libraries"**
   ```bash
   export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
   ```

2. **"CUDA out of memory"**
   - Reduce batch size
   - Use smaller model (yolov8n instead of yolov8x)
   - Close other GPU applications

3. **"TensorRT version mismatch"**
   - Rebuild the engine with your TensorRT version
   - Ensure ONNX model is compatible

### Performance Metrics

Expected performance on common GPUs:

| GPU | Model | Precision | Batch Size | FPS |
|-----|-------|-----------|------------|-----|
| RTX 3090 | YOLOv8n | FP16 | 1 | ~400 |
| RTX 3090 | YOLOv8s | FP16 | 1 | ~300 |
| RTX 3060 | YOLOv8n | FP16 | 1 | ~250 |
| RTX 3060 | YOLOv8s | FP16 | 1 | ~180 |
| Jetson Orin | YOLOv8n | FP16 | 1 | ~150 |
| Jetson Xavier | YOLOv8n | FP16 | 1 | ~80 |

## Platform-Specific Notes

### Jetson Platforms

For NVIDIA Jetson devices:

```bash
# Enable maximum performance
sudo nvpmodel -m 0
sudo jetson_clocks

# Use DLA cores (Xavier/Orin only)
{
    "tensorrt_config": {
        "enable_dla": true,
        "dla_core": 0
    }
}
```

### Docker Support

Dockerfile with TensorRT:

```dockerfile
FROM nvcr.io/nvidia/tensorrt:22.12-py3

# Install dependencies
RUN apt-get update && apt-get install -y \
    cmake \
    build-essential \
    libopencv-dev

# Copy and build application
COPY . /app
WORKDIR /app
RUN mkdir build && cd build && \
    cmake .. -DENABLE_CUDA_TENSORRT=ON && \
    make -j$(nproc)
```

## Integration with AISecurityVision

The TensorRT backend integrates seamlessly with the existing system:

1. **Automatic Backend Selection**: The system detects available acceleration hardware
2. **Unified Interface**: Same API regardless of backend (RKNN, TensorRT, CPU)
3. **Hot Swapping**: Switch between backends without restarting
4. **Performance Monitoring**: Built-in metrics for optimization

## Further Resources

- [NVIDIA TensorRT Documentation](https://docs.nvidia.com/deeplearning/tensorrt/)
- [YOLOv8 Export Guide](https://docs.ultralytics.com/modes/export/)
- [CUDA Programming Guide](https://docs.nvidia.com/cuda/cuda-c-programming-guide/)
