# AISecurityVision Quick Start Guide

## Overview
AISecurityVision now supports multiple AI acceleration backends:
- **TensorRT**: For NVIDIA GPUs (x86_64, Jetson)
- **RKNN**: For Rockchip NPUs (RK3588)
- **CPU**: Fallback for any platform

## Quick Start

### 1. Build the Project

```bash
# Standard build with all backends
./build.sh

# Build with specific options
./build.sh --debug              # Debug build
./build.sh --no-tensorrt        # Disable TensorRT
./build.sh --no-rknn           # Disable RKNN
./build.sh --clean             # Clean build
```

### 2. Prepare Models

#### For TensorRT (NVIDIA GPU):
```bash
# First, export YOLOv8 to ONNX
python3 -c "from ultralytics import YOLO; model = YOLO('yolov8n.pt'); model.export(format='onnx', imgsz=640)"

# Convert ONNX to TensorRT
python3 scripts/convert_yolov8_to_tensorrt.py models/yolov8n.onnx \
    --output models/yolov8n_fp16.engine \
    --precision FP16
```

#### For RKNN (RK3588):
```bash
# Use the existing RKNN conversion script
python3 scripts/convert_yolov8_to_rknn.py
```

### 3. Test the System

```bash
# Test all available backends
./build/tests/test_yolov8_backends test_image.jpg

# Test with specific model
./build/tests/test_yolov8_backends test_image.jpg models/yolov8n
```

### 4. Run the Main Application

```bash
# Using configuration file
./build/AISecurityVision --config config/config_tensorrt.json

# The system will automatically select the best available backend
```

## Configuration

### Auto Backend Selection
Set `backend: "AUTO"` in config to automatically use the best available backend:

```json
{
    "ai_config": {
        "backend": "AUTO",
        "backend_priority": ["TENSORRT", "RKNN", "CPU"]
    }
}
```

### Force Specific Backend
```json
{
    "ai_config": {
        "backend": "TENSORRT"  // or "RKNN", "CPU"
    }
}
```

## Platform-Specific Notes

### NVIDIA Desktop/Server
- Ensure CUDA and TensorRT are installed
- Use FP16 precision for best performance/accuracy balance
- Consider INT8 for maximum performance (requires calibration)

### NVIDIA Jetson
- Enable maximum performance mode: `sudo nvpmodel -m 0 && sudo jetson_clocks`
- Consider using DLA cores on Xavier/Orin for power efficiency

### Rockchip RK3588
- RKNN backend will be automatically selected
- Ensure RKNN drivers are properly installed
- Multi-core NPU is enabled by default

## Performance Expectations

| Platform | Model | Backend | Precision | Expected FPS |
|----------|-------|---------|-----------|--------------|
| RTX 3090 | YOLOv8n | TensorRT | FP16 | 400+ |
| RTX 3060 | YOLOv8n | TensorRT | FP16 | 250+ |
| Jetson Orin | YOLOv8n | TensorRT | FP16 | 150+ |
| RK3588 | YOLOv8n | RKNN | INT8 | 60+ |

## Troubleshooting

### "TensorRT not found"
```bash
# Check CUDA installation
nvidia-smi
nvcc --version

# Add to LD_LIBRARY_PATH
export LD_LIBRARY_PATH=/usr/local/cuda/lib64:$LD_LIBRARY_PATH
```

### "RKNN initialization failed"
```bash
# Check NPU status
sudo cat /sys/kernel/debug/rknpu/load
sudo dmesg | grep rknpu
```

### "Model loading failed"
- Ensure model file exists and has correct format
- Check model path in configuration
- Verify model was converted for the correct backend

## Docker Usage

### Build Docker image with TensorRT support:
```bash
docker build -f Dockerfile.tensorrt -t aisecurityvision:tensorrt .
```

### Run with GPU support:
```bash
docker run --gpus all -v $(pwd)/models:/app/models aisecurityvision:tensorrt
```

## API Usage Example

```cpp
#include "ai/YOLOv8DetectorFactory.h"

// Auto-select best backend
auto detector = YOLOv8DetectorFactory::createDetector();

// Or specify backend
auto detector = YOLOv8DetectorFactory::createDetector(InferenceBackend::TENSORRT);

// Initialize and use
if (detector->initialize("models/yolov8n_fp16.engine")) {
    cv::Mat frame = cv::imread("test.jpg");
    auto detections = detector->detectObjects(frame);
    
    for (const auto& det : detections) {
        std::cout << "Detected " << det.className 
                  << " with confidence " << det.confidence << std::endl;
    }
}
```

## Advanced Features

### Multi-Stream Processing (TensorRT)
```cpp
detector->setMaxBatchSize(4);  // Process 4 streams simultaneously
```

### Category Filtering
```cpp
detector->setEnabledCategories({"person", "car", "truck"});
```

### Performance Monitoring
```cpp
std::cout << "Last inference time: " << detector->getLastInferenceTime() << " ms" << std::endl;
std::cout << "Average FPS: " << 1000.0 / detector->getAverageInferenceTime() << std::endl;
```

## Next Steps
- Check the full documentation in `docs/TENSORRT_SETUP_GUIDE.md`
- Review example configurations in `config/`
- Run performance benchmarks with `test_yolov8_backends`
