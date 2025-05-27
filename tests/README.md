# Tests Directory

This directory contains all test files, scripts, and related resources for the AI Security Vision System.

## Directory Structure

```
tests/
├── cpp/           # C++ test files
├── python/        # Python test scripts and utilities
├── shell/         # Shell script tests
├── html/          # HTML test pages and demos
├── data/          # Test data files (images, configs, etc.)
├── scripts/       # Utility scripts
└── README.md      # This file
```

## Test Categories

### C++ Tests (`cpp/`)
- Unit tests for core components
- Integration tests
- Performance benchmarks
- RKNN NPU tests

### Python Tests (`python/`)
- API testing scripts
- RKNN detection demos
- Stream processing utilities
- Camera integration tests

### Shell Tests (`shell/`)
- System integration tests
- API endpoint tests
- Service health checks
- Build and deployment tests

### HTML Demos (`html/`)
- Web interface demos
- Real-time visualization pages
- Dashboard prototypes

### Test Data (`data/`)
- Test images and videos
- Configuration files
- Ground truth data
- Test results

## Running Tests

### C++ Tests
```bash
cd build
make test
```

### Python Tests
```bash
cd tests/python
python3 test_*.py
```

### Shell Tests
```bash
cd tests/shell
./test_*.sh
```

### RKNN Tests
```bash
cd tests/python
python3 demo_rknn_detection.py
```

## Key Test Files

### RKNN NPU Tests
- `cpp/test_rknn_yolov8.cpp` - Core RKNN testing
- `python/demo_rknn_detection.py` - Complete RKNN demo
- `python/simulate_rtsp_with_rknn.py` - RKNN with simulated cameras

### Integration Tests
- `shell/test_enhanced_api.sh` - API integration tests
- `python/test_real_cameras.py` - Real camera testing
- `cpp/test_cross_camera_tracking.cpp` - Multi-camera tests

### Performance Tests
- `cpp/test_gpu_monitoring.cpp` - GPU performance monitoring
- `shell/test_monitoring_performance.sh` - System performance tests

## Notes

- All test files have been moved from the project root to maintain clean project structure
- Tests are organized by type and functionality
- Each test category has its own subdirectory for better organization
- Test data and configuration files are centralized in the `data/` directory
