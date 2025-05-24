# AI Security Vision System - Detailed Development Plan

## Project Overview
This is a comprehensive C++17 AI Security Visual Analysis System that processes real-time video streams from RTSP/ONVIF/GB28181 sources. The system provides object detection, face recognition, license plate recognition, behavior analysis, and real-time alerting capabilities using GPU-accelerated AI models.

## Development Phases & Timeline

### Phase 1: Foundation & Core Infrastructure (Weeks 1-3)
**Goal**: Establish basic system architecture and video processing pipeline
- Set up CMake build system and dependencies
- Implement basic video input handling
- Create core AI processing pipeline
- Establish database schema and basic API structure

### Phase 2: AI Processing & Recognition (Weeks 4-6)
**Goal**: Implement core AI capabilities
- Integrate YOLOv8 with TensorRT optimization
- Implement face and license plate recognition
- Add object tracking with ByteTracker
- Create behavior analysis engine

### Phase 3: Advanced Features & Integration (Weeks 7-9)
**Goal**: Add advanced monitoring and management features
- Implement event recording system
- Add real-time streaming outputs
- Create comprehensive API endpoints
- Build system monitoring and health checks

### Phase 4: User Interface & Deployment (Weeks 10-12)
**Goal**: Complete user-facing features and deployment readiness
- Develop web-based management interface
- Implement alarm delivery systems
- Add multi-ROI and cross-camera tracking
- Performance optimization and testing

## Technical Architecture Summary

### Core Components:
1. **TaskManager**: Singleton managing multiple VideoPipeline instances
2. **VideoPipeline**: Per-stream processing chain (decode→detect→track→analyze→output)
3. **AI Modules**: YOLOv8Detector, FaceRecognizer, LicensePlateRecognizer, BehaviorAnalyzer
4. **Output Systems**: Recorder, Streamer, AlarmTrigger
5. **API Service**: RESTful endpoints using httplib
6. **Database**: SQLite3 with ORM for event storage

### Key Technologies:
- **C++17** with modern threading and memory management
- **CUDA/TensorRT** for GPU-accelerated AI inference
- **FFmpeg** for video processing and streaming
- **OpenCV** for computer vision operations
- **SQLite3** for data persistence
- **httplib** for REST API server

## Development Priorities

### Critical Path Items:
1. Video input handling (RTSP/ONVIF support)
2. YOLOv8 + TensorRT integration
3. Basic behavior analysis (intrusion detection)
4. Event recording and alerting
5. API endpoints for system control

### Secondary Features:
1. Advanced behavior analysis (crowd, loitering, falls)
2. Cross-camera tracking with ReID
3. Web interface for configuration
4. MQTT/WebSocket alarm delivery
5. Performance monitoring dashboard

## Implementation Roadmap

### Week 1-2: Project Setup & Foundation
**Deliverables**:
- CMake build system with all dependencies
- Basic project structure and core classes
- SQLite database schema
- Basic HTTP API server

**Key Tasks**:
- Set up development environment with CUDA/TensorRT
- Create CMakeLists.txt with dependency management
- Implement TaskManager singleton pattern
- Create basic VideoPipeline class structure
- Set up SQLite database with ORM

### Week 3-4: Video Input & Processing
**Deliverables**:
- RTSP stream input handling
- FFmpeg integration for video decoding
- Basic ONVIF device discovery
- Video source management API

**Key Tasks**:
- Implement FFmpegDecoder with GPU acceleration
- Create RTSP stream handler
- Add ONVIF device discovery using gSOAP
- Build video source management endpoints

### Week 5-6: Core AI Pipeline
**Deliverables**:
- YOLOv8 object detection with TensorRT
- ByteTracker integration
- Basic behavior analysis
- Real-time processing pipeline

**Key Tasks**:
- Integrate YOLOv8 with TensorRT optimization
- Implement ByteTracker for object tracking
- Create BehaviorAnalyzer for intrusion detection
- Add debug visualization with bounding boxes

### Week 7-8: Recognition & Recording
**Deliverables**:
- Face recognition system
- License plate recognition
- Event recording with pre/post buffers
- Database integration for events

**Key Tasks**:
- Implement face recognition with ResNet
- Add license plate OCR with Tesseract
- Create event-triggered recording system
- Build face/plate management APIs

### Week 9-10: Streaming & Monitoring
**Deliverables**:
- Real-time MJPEG/RTMP streaming
- System monitoring and health checks
- Advanced behavior analysis
- ROI configuration system

**Key Tasks**:
- Implement streaming output modules
- Add system metrics collection
- Create advanced behavior rules
- Build ROI management interface

### Week 11-12: Integration & Deployment
**Deliverables**:
- Complete web management interface
- Alarm delivery systems
- Cross-camera tracking
- Production-ready deployment

**Key Tasks**:
- Develop web dashboard
- Implement alarm routing (HTTP/MQTT/WebSocket)
- Add ReID for cross-camera tracking
- Performance optimization and testing

## Task Priority Matrix

### P0 (Critical - Must Have):
- Video input handling (Tasks 1-7)
- Core AI pipeline (Tasks 8-12)
- Basic behavior analysis (Tasks 13-18)
- API endpoints (Tasks 19-23)
- Event recording (Tasks 30-35)

### P1 (High - Should Have):
- Face/plate recognition (Tasks 24-29)
- Streaming outputs (Tasks 36-40)
- System monitoring (Tasks 41-46)
- Alarm delivery (Tasks 63-67)

### P2 (Medium - Could Have):
- Advanced ROI management (Tasks 47-51, 68-73)
- ONVIF discovery (Tasks 52-56)
- Face database management (Tasks 57-62)

### P3 (Low - Nice to Have):
- Cross-camera tracking (Tasks 74-78)
- Advanced behavior analysis
- Performance optimizations

## Immediate Next Steps (Week 1)

### 1. Development Environment Setup
**Priority**: P0 - Critical
**Estimated Time**: 2-3 days

**Tasks**:
1. Install CUDA Toolkit (11.8+ recommended)
2. Install TensorRT (8.5+ recommended)
3. Set up FFmpeg with CUDA support
4. Install OpenCV with CUDA support
5. Configure development IDE (VS Code/CLion)

**Verification**:
- `nvidia-smi` shows GPU information
- `nvcc --version` shows CUDA compiler
- FFmpeg shows CUDA encoders: `ffmpeg -encoders | grep nvenc`

### 2. Project Structure Creation
**Priority**: P0 - Critical
**Estimated Time**: 1-2 days

**Directory Structure**:
```
AISecurityVision/
├── CMakeLists.txt
├── src/
│   ├── core/
│   │   ├── TaskManager.h/cpp
│   │   ├── VideoPipeline.h/cpp
│   │   └── Config.h/cpp
│   ├── video/
│   │   ├── FFmpegDecoder.h/cpp
│   │   └── StreamHandler.h/cpp
│   ├── ai/
│   │   ├── YOLOv8Detector.h/cpp
│   │   ├── ByteTracker.h/cpp
│   │   └── BehaviorAnalyzer.h/cpp
│   ├── recognition/
│   │   ├── FaceRecognizer.h/cpp
│   │   └── LicensePlateRecognizer.h/cpp
│   ├── output/
│   │   ├── Recorder.h/cpp
│   │   ├── Streamer.h/cpp
│   │   └── AlarmTrigger.h/cpp
│   ├── api/
│   │   └── APIService.h/cpp
│   ├── database/
│   │   └── DatabaseManager.h/cpp
│   └── main.cpp
├── include/
├── models/
├── config/
├── tests/
└── docs/
```

### 3. CMake Build System
**Priority**: P0 - Critical
**Estimated Time**: 1 day

**Key Dependencies**:
- CUDA/TensorRT
- FFmpeg
- OpenCV
- SQLite3
- httplib
- gSOAP (for ONVIF)

### 4. Basic Class Interfaces
**Priority**: P0 - Critical
**Estimated Time**: 1-2 days

**Core Classes to Define**:
- TaskManager (singleton)
- VideoPipeline (main processing chain)
- FFmpegDecoder (video input)
- YOLOv8Detector (object detection)
- APIService (REST endpoints)

## Development Guidelines

### Code Standards:
- C++17 standard compliance
- Modern C++ practices (smart pointers, RAII)
- Thread-safe design patterns
- Comprehensive error handling
- Extensive logging for debugging

### Testing Strategy:
- Unit tests for core components
- Integration tests for AI pipeline
- Performance benchmarks
- Memory leak detection
- GPU utilization monitoring

### Documentation Requirements:
- API documentation (OpenAPI/Swagger)
- Architecture diagrams
- Deployment guides
- User manuals

## 子史诗

### 1. Implement video stream input handling with ONVIF auto-discovery and RTSP/GB28181 support, including concurrent stream management in TaskManager. Users can add/remove video sources via API and see live device status in web interface.

### 2. Develop core AI processing pipeline with GPU-accelerated decoding, YOLOv8 detection, and ByteTrack tracking. Users can verify object detection through bounding box overlays in test streams.

### 3. Create behavior analysis engine with configurable rules for intrusion detection and other scenarios. Users can trigger test alarms by simulating events in designated ROI areas.

### 4. Build video source and AI model management APIs with endpoints for adding streams, updating models, and viewing pipeline status. Users can configure through Postman/curl with JSON payloads.

### 5. Implement face and license plate recognition modules integrated with SQLite database. Users can register test faces/plates and see recognition results in sample streams.

### 6. Develop event recording system with pre/post-event video clips and BBOX overlays. Users can trigger manual recordings and review saved MP4 files with timestamped events.

### 7. Create real-time streaming output modules (MJPEG/RTMP) with resolution/fps configuration. Users can view live analysis results through VLC player or web browser.

### 8. Implement system monitoring API with CPU/GPU metrics and pipeline statistics. Users can check health status via dashboard with live resource usage graphs.

### 9. Build behavior rule configuration API with endpoints for managing ROIs and alarm thresholds. Users can draw polygons in web interface to define intrusion zones.

### 10. Develop ONVIF device discovery implementation using gSOAP/ONVIF-CPP. Users can scan network and auto-add compatible cameras through web interface.

### 11. Create face database management API with CRUD operations. Users can upload face images through web form and verify recognition against test photos.

### 12. Implement alarm delivery system with HTTP/MQTT/WebSocket integrations. Users can trigger test alarms through simulated events and verify all notification channels.

### 13. Develop multi-ROI support with priority levels and time-based rules. Users can configure complex monitoring scenarios with overlapping zones through API.

### 14. Integrate ReID module with ByteTrack for cross-camera tracking. Users can verify object continuity between multiple test streams through unique ID persistence.

## 任务列表

### 任务 1: Implement ONVIF device discovery using gSOAP/ONVIF-CPP library

**测试说明**: Run discovery command via API endpoint, verify discovered devices appear in response. Test with 1+ ONVIF cameras on network using Postman: POST /api/source/discover with timeout=2000, check returned IP addresses/model names.

**所属子史诗**: 1

**状态**: todo

---

### 任务 2: Create RTSP stream handler with FFmpeg decoding

**测试说明**: Add RTSP stream via API: POST /api/source/add with valid RTSP URL. Check TaskManager's active_pipelines count increases. Verify FFmpeg logs show successful connection and frame decoding.

**所属子史诗**: 1

**状态**: todo

---

### 任务 3: Implement GB28181 protocol support with device registration

**测试说明**: Connect GB28181 device, use Wireshark to verify SIP REGISTER/INVITE messages. Check system logs for successful device registration and stream setup.

**所属子史诗**: 1

**状态**: todo

---

### 任务 4: Develop video source management API endpoints (add/remove/list)

**测试说明**: Using Postman: 1) POST /api/source/add with test RTSP URL 2) GET /api/source/list 3) DELETE /api/source/{id}. Verify added source appears in list and disappears after deletion.

**所属子史诗**: 1

**状态**: todo

---

### 任务 5: Create concurrent stream management in TaskManager

**测试说明**: Add 3+ simultaneous video sources via API. Check system monitor for multiple ffmpeg processes and GPU decoder utilization. Verify no frame drops in logs for 5min operation.

**所属子史诗**: 1

**状态**: todo

---

### 任务 6: Build basic device status web interface

**测试说明**: Access /status page, verify connected devices show live status (online/offline), resolution, and FPS. Disconnect camera physically, check status changes within 10s.

**所属子史诗**: 1

**状态**: todo

---

### 任务 7: Implement stream validation and error handling

**测试说明**: Add invalid RTSP URL via API, verify system returns 400 error and logs authentication failure. Check TaskManager doesn't create pipeline for bad streams.

**所属子史诗**: 1

**状态**: todo

---

### 任务 8: Implement FFmpegDecoder class with GPU-accelerated H264/H265 decoding using CUDA. Integrate with VideoPipeline to output OpenCV matrices.

**测试说明**: Run test stream through decoder, verify successful frame extraction using OpenCV's imshow() with timestamps. Check GPU utilization via nvidia-smi during decoding.

**所属子史诗**: 2

**状态**: todo

---

### 任务 9: Create YOLOv8Detector class with TensorRT engine loading and 640x640 inference. Implement frame preprocessing/resizing and confidence threshold filtering.

**测试说明**: Process test image through detector, verify bounding box outputs in debug output. Measure inference latency stays under 0.5s using high-precision timer.

**所属子史诗**: 2

**状态**: todo

---

### 任务 10: Integrate ByteTracker with YOLOv8 outputs using Kalman filter-based tracking. Maintain object IDs across frames with minimum 15fps tracking performance.

**测试说明**: Process video sequence, verify consistent object IDs in console output. Check frame processing rate exceeds 15fps using performance metrics.

**所属子史诗**: 2

**状态**: todo

---

### 任务 11: Implement VideoPipeline::process_frame() method that chains decoder->detector->tracker. Add frame metadata passing between components.

**测试说明**: Process sample frame through full pipeline, verify in logs that all components executed sequentially with timing metrics preserved.

**所属子史诗**: 2

**状态**: todo

---

### 任务 12: Create debug visualization overlay with bounding boxes and tracking IDs using OpenCV. Add temporary output window for quality verification.

**测试说明**: Run live camera feed, observe real-time bounding boxes and tracking IDs in OpenCV window. Verify detection stability and ID persistence between frames.

**所属子史诗**: 2

**状态**: todo

---

### 任务 13: Implement BehaviorAnalyzer class with rule parsing and basic intrusion detection logic. Create JSON configuration schema for intrusion rules (ROI polygon, min duration).

**测试说明**: POST valid intrusion rule JSON to API, send test frames with object entering ROI for 5+ seconds. Verify alarm payload appears in system logs with correct event_type.

**所属子史诗**: 3

**状态**: done

---

### 任务 14: Add crowd detection logic with density/time thresholds. Implement people counting in ROI areas and duration tracking.

**测试说明**: Configure crowd rule (4+ people in 5m²), simulate 4 bounding boxes in test stream for 10+ seconds. Check if alarm triggers exactly at 10s mark.

**所属子史诗**: 3

**状态**: todo

---

### 任务 15: Develop loitering detection by tracking object dwell time in specific zones. Implement moving average position tracking.

**测试说明**: Set loitering rule with 60s threshold, keep test object in ROI for 60+ seconds. Verify alarm triggers at 60s with snapshot containing bounding box.

**所属子史诗**: 3

**状态**: todo

---

### 任务 16: Create fall detection using YOLO pose estimation. Implement angle calculations between body joints for fall recognition.

**测试说明**: Simulate fall posture in test video frame, verify system logs 'fall' event with confidence >0.7 within 1 second of detection.

**所属子史诗**: 3

**状态**: todo

---

### 任务 17: Implement fight detection through motion analysis (sudden movement patterns and close proximity tracking).

**测试说明**: Submit 2 overlapping bounding boxes with rapid movement vectors. Check if 'fight' alarm triggers within 3 consecutive frames of conflict.

**所属子史诗**: 3

**状态**: todo

---

### 任务 18: Integrate behavior rules with AlarmTrigger system. Connect detected events to HTTP/MQTT/WebSocket outputs.

**测试说明**: Trigger any test event and verify alarm payload appears simultaneously in: 1) WebSocket client 2) HTTP endpoint 3) MQTT broker queue.

**所属子史诗**: 3

**状态**: todo

---

### 任务 19: Implement API endpoint for adding video sources with protocol validation and TaskManager integration. Support RTSP/ONVIF/GB28181 protocols.

**测试说明**: Send POST request to /api/source/add with valid RTSP URL in JSON body using Postman. Verify new pipeline appears in TaskManager via debug logs and successful 201 response.

**所属子史诗**: 4

**状态**: todo

---

### 任务 20: Create GET endpoint for pipeline status reporting showing active streams and processing statistics.

**测试说明**: Call GET /api/system/status after adding streams. Verify response contains correct active_pipelines count and GPU memory usage from TensorRT context.

**所属子史诗**: 4

**状态**: todo

---

### 任务 21: Develop AI model update endpoint with TensorRT engine reload capability. Validate engine file compatibility before activation.

**测试说明**: POST to /api/ai/update_model with test YOLOv8 engine path. Verify detector loads new model by observing different class IDs in debug output and successful 200 response.

**所属子史诗**: 4

**状态**: todo

---

### 任务 22: Implement input validation and error handling for all API endpoints with meaningful HTTP status codes.

**测试说明**: Send malformed JSON, invalid URLs, and incorrect model paths to endpoints. Verify 400 errors with error descriptions in responses and no system crashes.

**所属子史诗**: 4

**状态**: todo

---

### 任务 23: Add video source removal endpoint with proper pipeline teardown and resource cleanup.

**测试说明**: DELETE to /api/source/remove with existing camera_id. Confirm pipeline stops in TaskManager logs and GPU memory decreases via status endpoint.

**所属子史诗**: 4

**状态**: todo

---

### 任务 24: Create SQLite database schema for face and license plate storage with necessary tables (faces, license_plates) and ORM integration

**测试说明**: Run application and verify creation of 'faces' table (columns: id, name, embedding, image_path) and 'license_plates' table (columns: id, number, region, image_path) using SQLite browser

**所属子史诗**: 5

**状态**: done

---

### 任务 25: Implement face recognition module with face detection (OpenCV) and 1:N matching using pre-trained ResNet model

**测试说明**: Use test image with multiple faces via API, verify detected faces in response and correct matches against registered faces in database

**所属子史诗**: 5

**状态**: done

---

### 任务 26: Develop license plate recognition module with OCR (Tesseract) and region classification for Chinese/European formats

**测试说明**: Process sample plate images through API, verify extracted plate numbers and regions appear in response JSON with >90% accuracy

**所属子史诗**: 5

**状态**: done

---

### 任务 27: Integrate recognition modules into VideoPipeline with frame processing and BBOX annotations

**测试说明**: Run test stream with known faces/plates, check console logs for recognition events and verify BBOX labels in debug output frames

**所属子史诗**: 5

**状态**: done

---

### 任务 28: Create API endpoints for face/plate management: POST /api/faces/add, GET /api/faces, POST /api/plates/add, GET /api/plates

**测试说明**: Use Postman to upload face image with name, verify entry appears in GET response and SQLite database

**所属子史诗**: 5

**状态**: done

---

### 任务 29: Implement recognition results overlay in streaming output with face/plate labels

**测试说明**: View MJPEG stream in browser, verify recognized faces show name labels and plates show number/region annotations

**所属子史诗**: 5

**状态**: done

---

### 任务 30: Implement circular buffer system for pre/post-event video frames using FFmpeg. Store 30s before and after events in memory.

**测试说明**: Trigger test event via API, verify recorded MP4 contains 30s before and after event timestamps using media player.

**所属子史诗**: 6

**状态**: done

---

### 任务 31: Create event-triggered MP4 recording module with BBOX overlay rendering using OpenCV. Save clips to ./recordings with timestamped filenames.

**测试说明**: Generate test detection event, check recordings folder for new MP4 file with bounding boxes visible in video playback.

**所属子史诗**: 6

**状态**: done

---

### 任务 32: Develop database integration for event recordings using SQLite ORM. Store event metadata (camera_id, timestamp, video_path) in events table.

**测试说明**: After recording event, query database via SQLite CLI to verify entry exists with correct video_path and timestamps.

**所属子史诗**: 6

**状态**: done

---

### 任务 33: Implement manual recording API endpoint (POST /api/record/start and /api/record/stop) with duration parameter.

**测试说明**: Use curl to start/stop manual recording, verify MP4 file creation and duration matches requested time.

**所属子史诗**: 6

**状态**: done

---

### 任务 34: Add timestamp overlay rendering to video frames using OpenCV putText with system clock synchronization.

**测试说明**: Review recorded video to confirm human-readable timestamps appear in corner of each frame.

**所属子史诗**: 6

**状态**: done

---

### 任务 35: Create configuration API for pre/post-event durations (POST /api/recording/config) with validation (min 10s, max 300s).

**测试说明**: Update config via Postman with different values, trigger event and measure recording duration with stopwatch.

**所属子史诗**: 6

**状态**: done

---

### 任务 36: Implement MJPEG streaming server with resolution/fps configuration in VideoPipeline class

**测试说明**: 1. Start application with test stream 2. Open http://localhost:8497/stream.mjpg in VLC/web browser 3. Verify live video appears at 640x480@15fps with detection overlays

**所属子史诗**: 7

**状态**: done

---

### 任务 37: Add RTMP output support using FFmpeg encoding to external servers

**测试说明**: 1. Configure RTMP endpoint in API 2. Start local nginx-rtmp server 3. Verify stream appears at rtmp://localhost/live/test with 1280x720@25fps using VLC

**所属子史诗**: 7

**状态**: done

---

### 任务 38: Create API endpoints for streaming configuration (protocol selection/resolution/fps)

**测试说明**: 1. Use POST /api/source/add with different stream_config parameters 2. Verify response contains active_stream_url 3. Check configured resolution/fps in output stream metadata

**所属子史诗**: 7

**状态**: done

**实施完成**:
✅ 实现完整的流媒体配置API端点集成到APIService
✅ 替换所有TODO占位符为实际的VideoPipeline流媒体配置调用
✅ 实现POST /api/stream/config端点支持MJPEG和RTMP协议配置
✅ 实现GET /api/stream/config端点获取实际流媒体配置
✅ 实现POST /api/stream/start和/api/stream/stop端点控制流媒体启停
✅ 实现GET /api/stream/status端点获取实时流媒体状态
✅ 扩展VideoPipeline类添加流媒体配置方法：configureStreaming, getStreamConfig, startStreaming, stopStreaming
✅ 添加流媒体状态查询方法：isStreamingEnabled, getStreamUrl, getConnectedClients, getStreamFps
✅ 实现完整的参数验证：分辨率范围、帧率限制、协议验证、RTMP URL要求
✅ 增强错误处理：摄像头不存在、配置失败、启停失败等场景
✅ 创建综合测试脚本验证所有流媒体配置功能(test_streaming_api.sh)
✅ 支持动态流媒体参数更新和实时配置重载

---

### 任务 39: Integrate BBOX overlays into streaming outputs

**测试说明**: 1. Enable object detection in test stream 2. Verify bounding boxes appear in both MJPEG/RTMP outputs 3. Check overlay coordinates match actual object positions

**所属子史诗**: 7

**状态**: done

**实施完成**:
✅ 增强Streamer类的renderOverlays方法支持多层次覆盖渲染
✅ 实现增强的检测框绘制：置信度分数、类别标签、颜色编码
✅ 添加角标记和改进的边界框样式提高可视性
✅ 实现ROI多边形可视化：半透明填充、边界线、标签
✅ 集成人脸识别结果覆盖：识别姓名、置信度显示
✅ 集成车牌识别结果覆盖：车牌号码、区域信息
✅ 实现行为事件和报警覆盖：实时警报指示器、闪烁效果
✅ 添加系统信息覆盖：检测计数、跟踪统计、流媒体信息
✅ 增强时间戳覆盖：高精度时间显示、背景样式
✅ 实现基于对象类型的智能颜色分配系统
✅ 支持覆盖层的动态启用/禁用配置
✅ 创建综合测试脚本验证所有覆盖功能(test_bbox_overlays.sh)
✅ 优化覆盖渲染性能和视觉效果

---

### 任务 40: Implement stream health monitoring and fallback mechanisms

**测试说明**: 1. Simulate network failure during streaming 2. Verify application logs show reconnection attempts 3. Check API status endpoint reports stream health metrics

**所属子史诗**: 7

**状态**: done

**实施完成**:
✅ 增强VideoPipeline健康监控系统：updateHealthMetrics(), checkStreamHealth(), isStreamStable()
✅ 实现帧超时检测：FRAME_TIMEOUT_S = 30秒阈值检测无响应流
✅ 连续错误跟踪：MAX_CONSECUTIVE_ERRORS = 10错误阈值触发重连
✅ 帧率稳定性检查：STABLE_FRAME_RATE_THRESHOLD = 50%期望帧率阈值
✅ 自动重连机制增强：错误计数重置、重连统计、指数退避延迟
✅ 健康指标计算：指数移动平均帧间隔、实时帧率监控
✅ 流稳定性评估：综合帧超时、错误率、帧率稳定性判断
✅ API端点增强：/api/system/status添加monitoring_healthy字段
✅ 流状态API扩展：添加stream_stable, frame_rate, processed_frames, dropped_frames, last_error字段
✅ 详细健康状态日志：状态变化记录、诊断信息输出
✅ TaskManager集成：失败管道自动清理、健康状态传播
✅ 线程安全实现：原子变量保护健康指标、mutex保护状态更新
✅ 创建综合测试脚本验证健康监控功能(test_stream_health_monitoring.sh)
✅ 健康检查间隔优化：HEALTH_CHECK_INTERVAL_S = 10秒定期检查

---

### 任务 41: Implement system status API endpoint returning basic CPU/GPU metrics

**测试说明**: Call GET /api/system/status using curl or Postman. Verify response contains numeric cpu_usage, gpu_mem string, and active_pipelines count. Basic validation: values should be >0 when system is running.

**所属子史诗**: 8

**状态**: done

---

### 任务 42: Add CPU usage monitoring implementation for Linux systems

**测试说明**: Run stress-ng in background and call status API. Verify cpu_usage increases proportionally to load. Stop stress-ng and confirm usage drops.

**所属子史诗**: 8

**状态**: done

---

### 任务 43: Integrate NVIDIA Management Library (NVML) for GPU metrics collection

**测试说明**: Run nvidia-smi in parallel with API calls. Verify gpu_mem values in API response match nvidia-smi output within 5% tolerance.

**所属子史诗**: 8

**状态**: done

---

### 任务 44: Add pipeline statistics tracking in TaskManager

**测试说明**: Add/remove video sources via API while monitoring status endpoint. Confirm active_pipelines count matches number of configured streams.

**所属子史诗**: 8

**状态**: done

**实施完成**:
✅ 扩展TaskManager类添加详细的管道统计信息结构体(PipelineStats, SystemStats)
✅ 实现每个VideoPipeline的性能指标收集(帧率、处理帧数、丢帧数、运行时间)
✅ 添加帧处理速率、错误计数、连接状态跟踪
✅ 更新API端点返回详细的管道统计信息(/api/system/pipeline-stats, /api/system/stats)
✅ 添加管道健康状态监控和系统级统计聚合
✅ 创建测试脚本验证统计功能(test_pipeline_stats.cpp, test_enhanced_api.sh)

---

### 任务 45: Create web dashboard page showing real-time metrics graphs

**测试说明**: Access /dashboard in browser. Verify live-updating charts for CPU/GPU usage and pipeline count appear. Confirm data matches API responses.

**所属子史诗**: 8

**状态**: done

**实施完成**:
✅ 创建完整的HTML/CSS/JavaScript web dashboard页面(web/templates/dashboard.html)
✅ 实现实时数据获取(2秒间隔API调用)
✅ 集成Chart.js图表库显示CPU/GPU/管道统计和性能图表
✅ 添加管道状态表格和系统概览卡片
✅ 实现响应式设计和实时数据更新功能
✅ 添加静态文件服务到APIService(CSS/JS/图片文件支持)
✅ 创建测试页面和脚本验证功能(test_dashboard.html, test_dashboard_api.sh)
✅ 实现暂停/恢复、重置图表、自动刷新等交互功能

---

### 任务 46: Implement metrics collection thread with 1s refresh interval

**测试说明**: Monitor API response timestamps. Confirm metrics update every 1-1.5 seconds even during high system load.

**所属子史诗**: 8

**状态**: done

**实施完成**:
✅ 优化TaskManager监控线程实现精确1000ms间隔(使用sleep_until)
✅ 添加高精度时间戳和性能监控(微秒级精度)
✅ 实现线程优先级优化(SCHED_FIFO和nice优先级)
✅ 添加监控线程健康检查和性能指标跟踪
✅ 优化内存使用和减少锁竞争(原子变量和指数移动平均)
✅ 创建性能测试验证1秒刷新间隔(test_monitoring_performance.sh)
✅ 增强API端点包含监控性能统计信息
✅ 实现监控周期计数、平均时间、最大时间和健康状态跟踪

---

### 任务 47: Implement REST API endpoints for behavior rule CRUD operations (POST/GET/PUT/DELETE) with JSON payload handling

**测试说明**: Use Postman to create a new intrusion rule via POST /api/rules with JSON body containing ROI polygon coordinates. Verify successful creation with GET /api/rules/{id} and check SQLite database for stored rule.

**所属子史诗**: 9

**状态**: done

---

### 任务 48: Add ROI polygon validation logic for rule creation/update endpoints

**测试说明**: Attempt to create rule with invalid polygon (non-closed path, <3 points) via POST /api/rules. Verify API returns 400 error with validation details in response body.

**所属子史诗**: 9

**状态**: done

**实施完成**:
✅ 创建综合PolygonValidator工具类，支持高级多边形验证
✅ 实现基础验证：点数量检查(最少3个点)、坐标范围验证(0-10000)
✅ 实现几何验证：自相交检测、面积计算(shoelace公式)、凸性检查
✅ 集成详细验证到API端点：POST/PUT /api/rules, POST /api/rois
✅ 增强错误响应包含详细验证信息和错误代码
✅ 创建全面测试套件验证各种无效多边形场景
✅ 实现线段相交检测算法(orientation-based)
✅ 支持可配置验证参数和灵活的验证策略
✅ 提供详细错误代码：INSUFFICIENT_POINTS, COORDINATE_OUT_OF_RANGE, AREA_TOO_SMALL, SELF_INTERSECTION等

---

### 任务 49: Integrate rule configuration with VideoPipeline's BehaviorAnalyzer

**测试说明**: Create intrusion rule via API, trigger motion in configured ROI area using test stream, verify alarm generation through WebSocket messages and database event records.

**所属子史诗**: 9

**状态**: done

**实施完成**:
✅ 扩展VideoPipeline类暴露BehaviorAnalyzer规则管理接口
✅ 实现完整的规则管理方法：addIntrusionRule, removeIntrusionRule, updateIntrusionRule, getIntrusionRules
✅ 实现ROI管理方法：addROI, removeROI, getROIs
✅ 更新APIService集成实际的BehaviorAnalyzer调用，替换TODO占位符
✅ 实现POST/PUT/DELETE /api/rules端点与VideoPipeline的完整集成
✅ 实现POST /api/rois端点与VideoPipeline的完整集成
✅ 添加线程安全的规则管理操作（mutex保护）
✅ 实现详细的错误处理和状态报告
✅ 创建综合集成测试脚本验证端到端工作流
✅ 实现规则序列化/反序列化支持JSON格式
✅ 集成增强的多边形验证到规则创建流程

---

### 任务 50: Create web interface component for ROI polygon drawing using HTML5 Canvas

**测试说明**: Access web interface, draw polygon on video preview, submit form. Verify coordinates appear in API requests and stored rule contains correct normalized coordinates.

**所属子史诗**: 9

**状态**: done

**实施完成**:
✅ 创建完整的ROI管理器Web界面(roi_manager.html)
✅ 实现HTML5 Canvas多边形绘制功能：点击式多边形创建、实时预览、自动闭合
✅ 开发交互式绘图控制：绘制模式切换、撤销点、清除画布、完成多边形
✅ 集成摄像头选择和视频流加载作为绘图背景参考
✅ 实现ROI配置表单：名称、类型、优先级、持续时间、置信度阈值
✅ 创建ROI列表管理界面：查看、选择、删除已配置的ROI
✅ 添加键盘快捷键支持：ESC退出、Enter完成、Backspace撤销
✅ 实现网格覆盖层提供精确定位辅助
✅ 开发实时坐标显示和绘图状态监控
✅ 集成API调用实现ROI持久化存储和检索
✅ 实现多边形可视化：填充、边框、点标记、选中高亮
✅ 添加响应式设计支持不同屏幕尺寸
✅ 创建通知系统提供用户操作反馈
✅ 实现完整的ROI生命周期管理：创建、查看、编辑、删除
✅ 创建综合测试脚本验证所有绘图功能(test_roi_drawing.sh)

---

### 任务 51: Implement rule priority handling and conflict resolution for overlapping ROIs

**测试说明**: Create two overlapping rules with different priorities. Trigger event in overlap area, verify only higher priority alarm is generated through WebSocket notifications.

**所属子史诗**: 9

**状态**: done

**实施完成**:
✅ 实现优先级冲突解决算法：checkIntrusionRulesWithPriority()方法
✅ 添加重叠ROI检测：getOverlappingROIs()辅助函数
✅ 实现最高优先级ROI选择：getHighestPriorityROI()辅助函数
✅ 增强事件元数据包含优先级信息和冲突解决标识
✅ 实现重复事件防止机制：processedObjects集合跟踪
✅ 支持1-5优先级等级：5(关键) > 4(高) > 3(中) > 2(中低) > 1(低)
✅ 线程安全实现：使用mutex保护所有优先级解决操作
✅ 详细日志记录：优先级冲突解决过程和结果
✅ 创建综合测试脚本验证优先级冲突解决(test_priority_conflict_resolution.sh)
✅ 创建单元测试验证优先级解决逻辑(test_priority_resolution.cpp)
✅ 集成到主要分析流程：替换原有checkIntrusionRules()调用
✅ 支持复杂重叠场景：多个ROI重叠时选择最高优先级
✅ 增强可视化：ROI绘制时根据优先级使用不同颜色编码

---

### 任务 52: Implement ONVIF device discovery core functionality using gSOAP/ONVIF-CPP library

**测试说明**: Run the compiled binary with discovery command, verify console output shows IP addresses of ONVIF cameras found on local network using Wireshark to confirm WS-Discovery packets are being sent/received.

**所属子史诗**: 10

**状态**: done

**实施完成**:
✅ 实现ONVIF设备发现核心功能：ONVIFDiscovery类和ONVIFManager类
✅ WS-Discovery协议实现：多播UDP通信(端口3702, 地址239.255.255.250)
✅ ONVIF设备结构定义：ONVIFDevice包含UUID、名称、制造商、型号、IP地址、服务URL等
✅ 网络发现功能：sendProbeMessage()发送WS-Discovery探测消息
✅ 设备响应解析：parseProbeMatch()解析ProbeMatch响应并提取设备信息
✅ 多播套接字管理：initializeNetwork()创建和配置UDP多播套接字
✅ 设备信息获取：getDeviceInformation(), getMediaProfiles(), getStreamUri()方法
✅ 线程安全设备缓存：使用mutex保护设备列表和操作
✅ API端点集成：GET /api/source/discover触发设备发现
✅ 设备添加端点：POST /api/source/add-discovered添加发现的设备到TaskManager
✅ 错误处理和超时管理：网络超时、设备验证、错误传播
✅ 网络接口处理：本地IP检测、接口枚举、多播绑定
✅ 设备认证支持：用户名/密码凭据管理和验证
✅ 自动配置集成：发现的设备自动转换为VideoSource并添加到系统
✅ 创建综合测试脚本验证ONVIF发现功能(test_onvif_discovery.sh)
✅ CMakeLists.txt更新：添加src/onvif/*.cpp到构建系统
✅ 性能优化：5秒发现超时、最大32设备限制、设备缓存机制

---

### 任务 53: Add API endpoint GET /api/source/discover for triggering device discovery

**测试说明**: Call endpoint via curl 'http://localhost:8080/api/source/discover' and verify JSON response contains discovered devices with manufacturer/model fields from actual test cameras.

**所属子史诗**: 10

**状态**: done

**实施完成**:
✅ 实现完整的HTTP服务器：使用cpp-httplib库替换占位符实现
✅ 添加httplib依赖到CMakeLists.txt，支持自动下载
✅ 实现真实的HTTP路由系统：setupRoutes()方法配置所有端点
✅ 完成GET /api/source/discover端点：触发ONVIF设备发现
✅ 完成POST /api/source/add-discovered端点：添加发现的设备
✅ 实现系统状态端点：GET /api/system/status, /api/system/metrics
✅ 实现视频源管理端点：POST /api/source/add, GET /api/source/list
✅ 修复编译错误：StreamConfig/StreamProtocol包含、BehaviorEvent声明
✅ 解决重复变量声明和私有方法访问问题
✅ 创建综合测试脚本验证HTTP服务器和API端点功能(test_onvif_api.sh)
✅ HTTP响应处理：JSON内容提取、Content-Type头、错误处理

---

### 任务 54: Implement automatic camera configuration for discovered ONVIF devices

**测试说明**: After discovery, verify system automatically creates RTSP URLs using ONVIF MediaService::GetStreamUri and adds them to TaskManager by checking debug logs or system/status API response.

**所属子史诗**: 10

**状态**: done

**实施完成**:
✅ 增强ONVIF SOAP通信：实现真实的GetDeviceInformation、GetProfiles、GetStreamUri SOAP请求
✅ 添加HTTP客户端功能：parseURL和sendHTTPRequest方法，支持HTTP/HTTPS通信
✅ 增强XML解析：extractXMLAttribute方法用于提取profile token等属性
✅ 添加WS-Security认证支持：ONVIF设备用户名密码认证
✅ SOAP请求失败时的回退机制：使用默认值确保系统稳定性
✅ 自动设备配置：ONVIFManager::configureDevice()方法实现自动配置
✅ 自动VideoSource创建：从发现的ONVIF设备自动创建视频源
✅ TaskManager集成：无缝管道创建和视频源管理
✅ 自动配置触发：设备发现时自动配置（可启用/禁用）
✅ 手动配置API：POST /api/source/add-discovered端点支持手动配置
✅ 真实设备信息提取：通过SOAP获取制造商、型号、固件版本
✅ 媒体配置文件令牌检索：用于正确的流URI请求
✅ 实际RTSP流URI提取：从ONVIF GetStreamUri响应中提取
✅ 智能回退机制：SOAP失败时使用标准RTSP URL
✅ API集成增强：handlePostAddDiscoveredDevice使用自动配置
✅ 凭据管理：发现设备的用户名密码管理
✅ 网络通信：HTTP/HTTPS支持、URL解析、超时处理、主机名解析

---

### 任务 55: Create web interface component for device discovery with scan button and results list

**测试说明**: Load web interface in browser, click 'Scan Devices' button, verify discovered cameras appear in list with model/IP within 5 seconds. Visually confirm UI matches mockups.

**所属子史诗**: 10

**状态**: done

**实施完成**:
✅ ONVIF发现Web界面：创建完整的HTML模板(onvif_discovery.html)，现代化UI设计
✅ 响应式设计：设备卡片、扫描控制、配置模态框，专业样式和平滑动画
✅ 移动友好：响应式布局，网格系统，适配各种屏幕尺寸
✅ 交互式JavaScript功能：完整的ONVIFDiscovery类，设备管理功能
✅ 实时设备扫描：进度指示器，动态设备卡片生成，设备信息显示
✅ 模态配置界面：基于模态框的设备配置，认证表单，用户友好交互
✅ API集成：发现和设备配置的完整API集成
✅ HTTP服务器集成：Web界面路由(/, /dashboard, /onvif-discovery)
✅ 静态文件服务：CSS、JS、图片文件服务，正确的MIME类型
✅ 文件加载工具：404错误处理，Content-Type头，缓存支持
✅ 用户界面功能：扫描按钮，超时选择(5-30秒)，实时进度条
✅ 设备卡片：显示制造商、型号、IP、流URI等信息
✅ 配置模态框：用户名密码认证，状态指示器
✅ 响应式网格布局：设备显示，发现vs已配置状态
✅ API端点集成：GET /api/source/discover, POST /api/source/add-discovered
✅ JSON响应处理：错误管理，加载状态，用户反馈
✅ 专业UI/UX：现代玻璃态设计，背景模糊效果，平滑动画
✅ 颜色编码状态指示器：专业排版，可访问表单控件

---

### 任务 56: Add device authentication handling for ONVIF cameras

**测试说明**: Test with camera requiring credentials - verify system prompts for username/password in web UI before adding, and successfully connects using ONVIF's WS-UsernameToken authentication.

**所属子史诗**: 10

**状态**: done

**实施完成**:
✅ 实现完整的testAuthentication()方法：使用GetDeviceInformation SOAP请求测试认证
✅ 增强WS-Security实现：添加nonce、timestamp和password digest支持
✅ 实现加密工具函数：generateNonce(), generateTimestamp(), generatePasswordDigest()
✅ 添加Base64编码/解码和SHA1哈希实现用于WS-Security
✅ 增强API端点认证验证：在设备配置前测试认证凭据
✅ 添加test_only标志支持：允许仅测试认证而不配置设备
✅ 改进Web界面：添加"Test Connection"按钮到配置模态框
✅ 实现JavaScript测试连接功能：实时认证验证和用户反馈
✅ 增强错误处理：详细的认证失败消息和状态码
✅ 添加CSS样式：专业的测试按钮样式和交互效果
✅ 创建综合测试脚本：验证所有认证功能(test_onvif_authentication.sh)
✅ 支持WS-UsernameToken认证：符合ONVIF标准的认证实现

---

### 任务 57: Implement SQLite face database schema with necessary fields (id, name, embedding vector, created_at)

**测试说明**: Run application and verify 'faces' table exists in SQLite DB using DB browser, checking for required columns and indexes.

**所属子史诗**: 11

**状态**: done

**实施完成**:
✅ 实现完整的SQLite面部数据库架构：faces表包含所有必需字段
✅ id字段：INTEGER PRIMARY KEY AUTOINCREMENT自动生成唯一标识符
✅ name字段：TEXT NOT NULL UNIQUE存储面部识别名称
✅ embedding字段：BLOB存储面部特征向量（支持任意维度）
✅ created_at字段：DATETIME DEFAULT CURRENT_TIMESTAMP自动时间戳
✅ image_path字段：TEXT存储面部图像文件路径
✅ 实现高性能索引：idx_faces_name提供快速名称查询
✅ 向量序列化/反序列化：vectorToBlob()和blobToVector()方法
✅ 完整CRUD操作：insertFace, getFaces, getFaceById, getFaceByName, updateFace, deleteFace
✅ 线程安全实现：mutex保护所有数据库操作
✅ 错误处理和验证：完整的错误消息和状态检查
✅ 数据完整性验证：embedding向量数据完整性测试通过
✅ 创建综合测试验证所有数据库架构功能(test_face_db_schema.cpp)

---

### 任务 58: Create POST /api/faces/add endpoint for face registration with image upload

**测试说明**: Use Postman to send multipart/form-data request with test image and verify 201 response with face_id. Check SQLite for new entry.

**所属子史诗**: 11

**状态**: done

**实施完成**:
✅ 实现完整的POST /api/faces/add端点：支持multipart/form-data图像上传
✅ 图像文件处理：支持JPG、PNG、BMP格式验证和存储
✅ 参数验证：name参数必需验证、图像文件存在性检查
✅ 文件存储系统：自动创建faces目录、唯一文件名生成（时间戳）
✅ 数据库集成：FaceRecord创建和SQLite存储
✅ 面部特征向量：实现dummy embedding生成（128维向量）
✅ 响应处理：201状态码、face_id返回、详细JSON响应
✅ 错误处理：文件类型验证、数据库错误、文件保存失败处理
✅ GET /api/faces端点：列出所有注册面部信息
✅ DELETE /api/faces/{id}端点：面部删除功能
✅ 完整的CRUD操作：创建、读取、删除面部记录
✅ 线程安全实现：DatabaseManager实例化和操作
✅ 图像文件管理：上传文件保存、删除时清理图像文件
✅ JSON序列化：面部记录转换为JSON格式
✅ HTTP路由配置：正确的multipart处理和RESTful端点
✅ 注意：面部识别模块集成待后续任务完成（当前使用dummy embedding）

---

### 任务 59: Implement GET /api/faces endpoint to list all registered faces

**测试说明**: Call endpoint via curl and verify JSON response contains array of face entries with IDs and metadata.

**所属子史诗**: 11

**状态**: todo

---

### 任务 60: Develop DELETE /api/faces/{id} endpoint for face removal

**测试说明**: Delete test entry via API call and confirm 204 response. Verify removal from SQLite and failed recognition attempts.

**所属子史诗**: 11

**状态**: todo

---

### 任务 61: Create web form UI for face upload with live preview

**测试说明**: Access form in browser, upload test photo and verify image preview. Check network tab for successful API call.

**所属子史诗**: 11

**状态**: todo

---

### 任务 62: Integrate face recognition verification endpoint GET /api/faces/verify

**测试说明**: Upload test face then send new image via API. Verify response contains match confidence score and correct face ID.

**所属子史诗**: 11

**状态**: todo

---

### 任务 63: Implement HTTP POST alarm delivery with JSON payload formatting. Create API endpoint for configuring HTTP alert destinations.

**测试说明**: 1. Configure HTTP endpoint using POST /api/alarms/config with {'method':'http','url':'http://test-server'} 2. Trigger test alarm via API 3. Verify test server receives JSON matching specification with valid snapshot and event data

**所属子史诗**: 12

**状态**: todo

---

### 任务 64: Add WebSocket alarm streaming support with persistent connections. Implement /ws/alarms endpoint for real-time updates.

**测试说明**: 1. Connect to ws://localhost/ws/alarms using WebSocket client 2. Trigger test alarm via simulated event 3. Verify client receives JSON message within 500ms with correct event_type and camera_id

**所属子史诗**: 12

**状态**: todo

---

### 任务 65: Integrate MQTT alarm publishing using Paho C++ client. Add MQTT broker configuration API endpoints.

**测试说明**: 1. Configure MQTT broker details via POST /api/alarms/config 2. Subscribe to aibox/alarms topic using MQTT client 3. Trigger test alarm and verify message appears on broker with correct payload structure

**所属子史诗**: 12

**状态**: todo

---

### 任务 66: Create alarm routing system that supports multiple simultaneous delivery methods with priority queuing.

**测试说明**: 1. Enable HTTP+WS+MQTT simultaneously 2. Trigger high-priority alarm 3. Verify all 3 channels receive alert within 1 second using network monitoring tools

**所属子史诗**: 12

**状态**: todo

---

### 任务 67: Implement test alarm trigger API endpoint for simulating events without real detection.

**测试说明**: 1. POST to /api/alarms/test with {'event_type':'intrusion','camera_id':'test'} 2. Verify all configured alert channels receive test payload with 'test_mode:true' flag

**所属子史诗**: 12

**状态**: todo

---

### 任务 68: Implement API endpoints for managing multiple ROIs with polygon coordinates and basic metadata (name, enabled status).

**测试说明**: Use Postman to send POST/PUT/DELETE requests to /api/rois endpoint with JSON payload containing polygon coordinates. Verify changes through GET /api/rois/{camera_id} and check SQLite database entries.

**所属子史诗**: 13

**状态**: todo

---

### 任务 69: Add priority level field to ROI configuration with validation (1-5 scale). Update API schema to include priority in requests/responses.

**测试说明**: Create ROIs with different priority levels via API. Verify higher priority ROIs appear first in responses. Attempt to set priority 6 should return validation error.

**所属子史诗**: 13

**状态**: todo

---

### 任务 70: Implement time-based rule support with start/end time fields in ROI configuration (ISO 8601 format).

**测试说明**: Create ROI with time window (e.g., 09:00-17:00). Trigger intrusion events inside/outside window using test stream. Verify alarms only during active period.

**所属子史诗**: 13

**状态**: todo

---

### 任务 71: Develop conflict resolution logic for overlapping ROIs considering priority and time rules.

**测试说明**: Create overlapping ROIs with different priorities and time rules. Simulate events in overlap area and verify alarm payloads contain highest priority ROI ID.

**所属子史诗**: 13

**状态**: todo

---

### 任务 72: Add API endpoint for bulk ROI configuration updates with atomic transaction support.

**测试说明**: POST array of ROIs to /api/rois/bulk with some invalid entries. Verify entire transaction fails and no partial updates occur in database.

**所属子史诗**: 13

**状态**: todo

---

### 任务 73: Implement visualization of active ROIs in test stream overlay with color-coding by priority.

**测试说明**: Access test stream via web interface. Verify ROIs display with correct shapes/colors. Toggle time-based ROIs and confirm overlay changes accordingly.

**所属子史诗**: 13

**状态**: todo

---

### 任务 74: Implement ReID feature extractor module using pre-trained ResNet50 model integrated with ByteTracker

**测试说明**: Run object detection on sample video stream and verify in logs that each tracked object receives ReID embedding vector (128-2048 dimensions) alongside bounding box coordinates.

**所属子史诗**: 14

**状态**: todo

---

### 任务 75: Add cross-camera tracking logic in TaskManager to share ReID features between VideoPipeline instances

**测试说明**: Start two video streams showing the same person/object moving between cameras. Check console logs for consistent unique ID tracking across both pipeline instances using 'track_id' field.

**所属子史诗**: 14

**状态**: todo

---

### 任务 76: Create ReID matching algorithm with configurable similarity threshold in BehaviorAnalyzer

**测试说明**: Set different similarity thresholds (0.5-0.95) via config file and verify through test scripts that object associations between cameras only occur when threshold is met.

**所属子史诗**: 14

**状态**: todo

---

### 任务 77: Add unique ID persistence in API output for alarm events and video streams

**测试说明**: Trigger cross-camera movement event and verify through API response (HTTP POST/WebSocket) that the same 'reid_id' appears in alarms from different camera streams within 5-second window.

**所属子史诗**: 14

**状态**: todo

---

### 任务 78: Implement test video sequences with known object transitions between camera views

**测试说明**: Run dedicated test mode with prerecorded multi-camera sequences and verify in system logs that at least 90% of ground truth transitions maintain consistent ReID tracking IDs.

**所属子史诗**: 14

**状态**: todo

---

--- 任务拆分文档生成完毕 ---

