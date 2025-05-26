# AI Models Directory

This directory contains AI models for the vision system.

## YOLOv8 Models

To use real YOLOv8 models, place the following files here:

### ONNX Models (for OpenCV DNN backend)
- `yolov8n.onnx` - YOLOv8 Nano (fastest)
- `yolov8s.onnx` - YOLOv8 Small
- `yolov8m.onnx` - YOLOv8 Medium
- `yolov8l.onnx` - YOLOv8 Large
- `yolov8x.onnx` - YOLOv8 Extra Large

### RKNN Models (for RK3588 NPU backend)
- `yolov8n.rknn` - YOLOv8 Nano for RK3588
- `yolov8s.rknn` - YOLOv8 Small for RK3588

## Download Instructions

### Method 1: Using Python ultralytics
```bash
pip install ultralytics
python3 -c "
from ultralytics import YOLO
model = YOLO('yolov8n.pt')
model.export(format='onnx')
"
```

### Method 2: Direct download
```bash
# Download from official releases
wget https://github.com/ultralytics/assets/releases/download/v8.2.0/yolov8n.onnx
```

### Method 3: Convert to RKNN
```bash
# Use the conversion script
python3 ../scripts/convert_yolov8_to_rknn.py --input yolov8n.onnx --output yolov8n.rknn --platform rk3588
```

## Model Configuration

The system will automatically detect and use available models:
1. First tries RKNN models (if RKNN backend available)
2. Falls back to ONNX models (OpenCV DNN)
3. Falls back to simulation if no models found

## Performance

### Expected FPS on RK3588
- YOLOv8n + RKNN: 60-80 FPS
- YOLOv8s + RKNN: 30-40 FPS
- YOLOv8n + OpenCV: 5-10 FPS
