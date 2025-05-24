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

**状态**: todo

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

**状态**: todo

---

### 任务 33: Implement manual recording API endpoint (POST /api/record/start and /api/record/stop) with duration parameter.

**测试说明**: Use curl to start/stop manual recording, verify MP4 file creation and duration matches requested time.

**所属子史诗**: 6

**状态**: todo

---

### 任务 34: Add timestamp overlay rendering to video frames using OpenCV putText with system clock synchronization.

**测试说明**: Review recorded video to confirm human-readable timestamps appear in corner of each frame.

**所属子史诗**: 6

**状态**: todo

---

### 任务 35: Create configuration API for pre/post-event durations (POST /api/recording/config) with validation (min 10s, max 300s).

**测试说明**: Update config via Postman with different values, trigger event and measure recording duration with stopwatch.

**所属子史诗**: 6

**状态**: todo

---

### 任务 36: Implement MJPEG streaming server with resolution/fps configuration in VideoPipeline class

**测试说明**: 1. Start application with test stream 2. Open http://localhost:8000/stream.mjpg in VLC/web browser 3. Verify live video appears at 640x480@15fps with detection overlays

**所属子史诗**: 7

**状态**: todo

---

### 任务 37: Add RTMP output support using FFmpeg encoding to external servers

**测试说明**: 1. Configure RTMP endpoint in API 2. Start local nginx-rtmp server 3. Verify stream appears at rtmp://localhost/live/test with 1280x720@25fps using VLC

**所属子史诗**: 7

**状态**: todo

---

### 任务 38: Create API endpoints for streaming configuration (protocol selection/resolution/fps)

**测试说明**: 1. Use POST /api/source/add with different stream_config parameters 2. Verify response contains active_stream_url 3. Check configured resolution/fps in output stream metadata

**所属子史诗**: 7

**状态**: todo

---

### 任务 39: Integrate BBOX overlays into streaming outputs

**测试说明**: 1. Enable object detection in test stream 2. Verify bounding boxes appear in both MJPEG/RTMP outputs 3. Check overlay coordinates match actual object positions

**所属子史诗**: 7

**状态**: todo

---

### 任务 40: Implement stream health monitoring and fallback mechanisms

**测试说明**: 1. Simulate network failure during streaming 2. Verify application logs show reconnection attempts 3. Check API status endpoint reports stream health metrics

**所属子史诗**: 7

**状态**: todo

---

### 任务 41: Implement system status API endpoint returning basic CPU/GPU metrics

**测试说明**: Call GET /api/system/status using curl or Postman. Verify response contains numeric cpu_usage, gpu_mem string, and active_pipelines count. Basic validation: values should be >0 when system is running.

**所属子史诗**: 8

**状态**: todo

---

### 任务 42: Add CPU usage monitoring implementation for Linux systems

**测试说明**: Run stress-ng in background and call status API. Verify cpu_usage increases proportionally to load. Stop stress-ng and confirm usage drops.

**所属子史诗**: 8

**状态**: todo

---

### 任务 43: Integrate NVIDIA Management Library (NVML) for GPU metrics collection

**测试说明**: Run nvidia-smi in parallel with API calls. Verify gpu_mem values in API response match nvidia-smi output within 5% tolerance.

**所属子史诗**: 8

**状态**: todo

---

### 任务 44: Add pipeline statistics tracking in TaskManager

**测试说明**: Add/remove video sources via API while monitoring status endpoint. Confirm active_pipelines count matches number of configured streams.

**所属子史诗**: 8

**状态**: todo

---

### 任务 45: Create web dashboard page showing real-time metrics graphs

**测试说明**: Access /dashboard in browser. Verify live-updating charts for CPU/GPU usage and pipeline count appear. Confirm data matches API responses.

**所属子史诗**: 8

**状态**: todo

---

### 任务 46: Implement metrics collection thread with 1s refresh interval

**测试说明**: Monitor API response timestamps. Confirm metrics update every 1-1.5 seconds even during high system load.

**所属子史诗**: 8

**状态**: todo

---

### 任务 47: Implement REST API endpoints for behavior rule CRUD operations (POST/GET/PUT/DELETE) with JSON payload handling

**测试说明**: Use Postman to create a new intrusion rule via POST /api/rules with JSON body containing ROI polygon coordinates. Verify successful creation with GET /api/rules/{id} and check SQLite database for stored rule.

**所属子史诗**: 9

**状态**: todo

---

### 任务 48: Add ROI polygon validation logic for rule creation/update endpoints

**测试说明**: Attempt to create rule with invalid polygon (non-closed path, <3 points) via POST /api/rules. Verify API returns 400 error with validation details in response body.

**所属子史诗**: 9

**状态**: todo

---

### 任务 49: Integrate rule configuration with VideoPipeline's BehaviorAnalyzer

**测试说明**: Create intrusion rule via API, trigger motion in configured ROI area using test stream, verify alarm generation through WebSocket messages and database event records.

**所属子史诗**: 9

**状态**: todo

---

### 任务 50: Create web interface component for ROI polygon drawing using HTML5 Canvas

**测试说明**: Access web interface, draw polygon on video preview, submit form. Verify coordinates appear in API requests and stored rule contains correct normalized coordinates.

**所属子史诗**: 9

**状态**: todo

---

### 任务 51: Implement rule priority handling and conflict resolution for overlapping ROIs

**测试说明**: Create two overlapping rules with different priorities. Trigger event in overlap area, verify only higher priority alarm is generated through WebSocket notifications.

**所属子史诗**: 9

**状态**: todo

---

### 任务 52: Implement ONVIF device discovery core functionality using gSOAP/ONVIF-CPP library

**测试说明**: Run the compiled binary with discovery command, verify console output shows IP addresses of ONVIF cameras found on local network using Wireshark to confirm WS-Discovery packets are being sent/received.

**所属子史诗**: 10

**状态**: todo

---

### 任务 53: Add API endpoint GET /api/source/discover for triggering device discovery

**测试说明**: Call endpoint via curl 'http://localhost:8080/api/source/discover' and verify JSON response contains discovered devices with manufacturer/model fields from actual test cameras.

**所属子史诗**: 10

**状态**: todo

---

### 任务 54: Implement automatic camera configuration for discovered ONVIF devices

**测试说明**: After discovery, verify system automatically creates RTSP URLs using ONVIF MediaService::GetStreamUri and adds them to TaskManager by checking debug logs or system/status API response.

**所属子史诗**: 10

**状态**: todo

---

### 任务 55: Create web interface component for device discovery with scan button and results list

**测试说明**: Load web interface in browser, click 'Scan Devices' button, verify discovered cameras appear in list with model/IP within 5 seconds. Visually confirm UI matches mockups.

**所属子史诗**: 10

**状态**: todo

---

### 任务 56: Add device authentication handling for ONVIF cameras

**测试说明**: Test with camera requiring credentials - verify system prompts for username/password in web UI before adding, and successfully connects using ONVIF's WS-UsernameToken authentication.

**所属子史诗**: 10

**状态**: todo

---

### 任务 57: Implement SQLite face database schema with necessary fields (id, name, embedding vector, created_at)

**测试说明**: Run application and verify 'faces' table exists in SQLite DB using DB browser, checking for required columns and indexes.

**所属子史诗**: 11

**状态**: todo

---

### 任务 58: Create POST /api/faces/add endpoint for face registration with image upload

**测试说明**: Use Postman to send multipart/form-data request with test image and verify 201 response with face_id. Check SQLite for new entry.

**所属子史诗**: 11

**状态**: todo

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

