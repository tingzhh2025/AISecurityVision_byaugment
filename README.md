# AI Security Vision System

A comprehensive C++17 AI Security Visual Analysis System that processes real-time video streams from RTSP/ONVIF/GB28181 sources with object detection, face recognition, license plate recognition, and behavior analysis capabilities.

## Features

- **Multi-stream Processing**: Concurrent processing of multiple video streams
- **AI-Powered Detection**: YOLOv8 object detection with TensorRT optimization
- **Object Tracking**: ByteTracker for consistent object tracking across frames
- **Face Recognition**: Real-time face detection and recognition
- **License Plate Recognition**: OCR-based license plate detection and reading
- **Behavior Analysis**: Intrusion detection, crowd analysis, loitering detection
- **Event Recording**: Automatic recording of security events with pre/post buffers
- **Real-time Streaming**: MJPEG/RTMP streaming with overlay annotations
- **RESTful API**: Complete API for system control and configuration
- **Web Interface**: Browser-based management and monitoring

## Architecture

### Core Components
- **TaskManager**: Singleton managing multiple VideoPipeline instances
- **VideoPipeline**: Per-stream processing chain (decode→detect→track→analyze→output)
- **AI Modules**: YOLOv8Detector, FaceRecognizer, LicensePlateRecognizer, BehaviorAnalyzer
- **Output Systems**: Recorder, Streamer, AlarmTrigger
- **API Service**: RESTful endpoints using httplib
- **Database**: SQLite3 with ORM for event storage

### Key Technologies
- **C++17** with modern threading and memory management
- **CUDA/TensorRT** for GPU-accelerated AI inference
- **FFmpeg** for video processing and streaming
- **OpenCV** for computer vision operations
- **SQLite3** for data persistence
- **httplib** for REST API server

## Prerequisites

### System Requirements
- NVIDIA GPU with CUDA support (Compute Capability 6.1+)
- Ubuntu 20.04+ or similar Linux distribution
- Minimum 8GB RAM, 16GB recommended
- 50GB+ free disk space for models and recordings

### Dependencies
- **CUDA Toolkit** (11.8+ recommended)
- **TensorRT** (8.5+ recommended)
- **FFmpeg** with CUDA support
- **OpenCV** (4.5+ with CUDA support)
- **SQLite3**
- **CMake** (3.18+)
- **GCC** (9+ with C++17 support)

## Installation

### 1. Install CUDA and TensorRT
```bash
# Install CUDA Toolkit
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2004/x86_64/cuda-ubuntu2004.pin
sudo mv cuda-ubuntu2004.pin /etc/apt/preferences.d/cuda-repository-pin-600
wget https://developer.download.nvidia.com/compute/cuda/11.8.0/local_installers/cuda-repo-ubuntu2004-11-8-local_11.8.0-520.61.05-1_amd64.deb
sudo dpkg -i cuda-repo-ubuntu2004-11-8-local_11.8.0-520.61.05-1_amd64.deb
sudo cp /var/cuda-repo-ubuntu2004-11-8-local/cuda-*-keyring.gpg /usr/share/keyrings/
sudo apt-get update
sudo apt-get -y install cuda

# Install TensorRT (requires NVIDIA Developer account)
# Download TensorRT from https://developer.nvidia.com/tensorrt
```

### 2. Install System Dependencies
```bash
sudo apt update
sudo apt install -y \
    build-essential \
    cmake \
    git \
    pkg-config \
    libopencv-dev \
    libsqlite3-dev \
    libavformat-dev \
    libavcodec-dev \
    libswscale-dev \
    libavutil-dev \
    libavdevice-dev
```

### 3. Build the Project
```bash
git clone https://github.com/tingzhh2025/AISecurityVision_byaugment.git
cd AISecurityVision_byaugment
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

### Basic Usage
```bash
# Run with default settings
./AISecurityVision

# Run with custom API port
./AISecurityVision --port 8080

# Run in test mode with sample configuration
./AISecurityVision --test

# Show help
./AISecurityVision --help
```

### API Endpoints

#### System Status
```bash
# Get system status
curl http://localhost:8080/api/system/status
```

#### Video Source Management
```bash
# Add RTSP video source
curl -X POST http://localhost:8080/api/source/add \
  -H "Content-Type: application/json" \
  -d '{
    "id": "camera_01",
    "url": "rtsp://admin:password@192.168.1.100:554/stream1",
    "protocol": "rtsp",
    "width": 1920,
    "height": 1080,
    "fps": 25
  }'

# List active video sources
curl http://localhost:8080/api/source/list

# Remove video source
curl -X DELETE http://localhost:8080/api/source/camera_01
```

#### ONVIF Device Discovery
```bash
# Discover ONVIF cameras on network
curl http://localhost:8080/api/source/discover
```

## Development Status

This project is currently in **Phase 1: Foundation & Core Infrastructure** development.

### Completed ✅
- Project structure and CMake build system
- Core architecture (TaskManager, VideoPipeline)
- Basic FFmpeg video decoding
- Stub implementations for all major components
- API service framework

### In Progress 🚧
- YOLOv8 + TensorRT integration
- RTSP stream handling
- Object tracking implementation

### Planned 📋
- Face recognition system
- License plate recognition
- Behavior analysis engine
- Event recording system
- Web management interface

See [tasks.md](tasks.md) for detailed development roadmap and task breakdown.

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Support

For questions and support, please open an issue on GitHub or contact the development team.
