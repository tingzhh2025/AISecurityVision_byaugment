# RKNN NPU Integration Summary

本文档总结了为AI视觉分析系统添加的RKNN RKNPU2支持。

## 新增功能

### 1. 多后端支持
- **RKNN**: 使用Rockchip NPU进行硬件加速
- **OpenCV**: CPU推理作为备选
- **TensorRT**: 为未来NVIDIA GPU支持预留
- **自动选择**: 根据可用硬件自动选择最佳后端

### 2. 核心文件修改

#### YOLOv8Detector.h
- 添加了`InferenceBackend`枚举
- 新增RKNN相关成员变量
- 添加多后端支持方法

#### YOLOv8Detector.cpp
- 实现了RKNN初始化和推理
- 添加了后端自动检测
- 实现了RKNN特定的预处理和后处理

#### CMakeLists.txt
- 添加RKNN库检测和链接
- 新增编译定义`HAVE_RKNN`
- 创建测试程序构建配置

### 3. 新增工具和脚本

#### scripts/convert_yolov8_to_rknn.py
- YOLOv8 ONNX到RKNN模型转换工具
- 支持INT8和FP16量化
- 支持多种RK平台

#### test_rknn_yolov8.cpp
- 独立的RKNN测试程序
- 支持性能测试和结果可视化
- 命令行参数配置

#### build_and_test_rknn.sh
- 自动化构建和测试脚本
- 检查系统依赖
- 下载模型和运行测试

#### docs/RKNN_SETUP_GUIDE.md
- 详细的安装和配置指南
- 故障排除说明
- 性能优化建议

## 使用方法

### 快速开始

1. **运行自动化脚本**:
   ```bash
   ./build_and_test_rknn.sh
   ```

2. **手动构建**:
   ```bash
   mkdir build && cd build
   cmake ..
   make -j$(nproc)
   ```

3. **转换模型**:
   ```bash
   python3 scripts/convert_yolov8_to_rknn.py \
       --input models/yolov8n.onnx \
       --output models/yolov8n.rknn \
       --platform rk3588
   ```

4. **运行测试**:
   ```bash
   ./build/test_rknn_yolov8 -m models/yolov8n.rknn -i test.jpg
   ```

### 在代码中使用

```cpp
#include "src/ai/YOLOv8Detector.h"

YOLOv8Detector detector;

// 自动选择最佳后端
detector.initialize("models/yolov8n.rknn", InferenceBackend::AUTO);

// 或指定特定后端
detector.initialize("models/yolov8n.rknn", InferenceBackend::RKNN);

// 检查当前后端
std::cout << "Using: " << detector.getBackendName() << std::endl;

// 进行检测
auto detections = detector.detectObjects(frame);
```

## 性能优化

### RK3588上的预期性能
- **YOLOv8n + RKNN INT8**: ~60-80 FPS
- **YOLOv8s + RKNN INT8**: ~30-40 FPS
- **YOLOv8n + OpenCV CPU**: ~5-10 FPS

### 优化建议
1. 使用INT8量化以获得最佳性能
2. 设置NPU为performance模式
3. 使用较小的模型(YOLOv8n/s)以获得实时性能
4. 确保充足的CMA内存分配

## 兼容性

### 支持的平台
- RK3588 (主要目标)
- RK3576, RK3568, RK3566, RK3562
- 其他支持RKNN的Rockchip平台

### 支持的模型格式
- **RKNN**: .rknn文件 (推荐用于NPU)
- **ONNX**: .onnx文件 (用于OpenCV后端)

### 系统要求
- Ubuntu 20.04/22.04 (aarch64)
- RKNN运行时库 >= 2.0.0
- OpenCV >= 4.0
- CMake >= 3.16

## 故障排除

### 常见问题

1. **RKNN库未找到**
   - 检查`/usr/lib/librknn_api.so`是否存在
   - 运行`sudo ldconfig`更新库缓存

2. **模型转换失败**
   - 确保安装了`rknn-toolkit2`
   - 检查ONNX模型是否有效

3. **推理速度慢**
   - 检查是否使用了RKNN后端
   - 确认NPU频率设置
   - 使用INT8量化

4. **内存不足**
   - 增加CMA内存: `cma=256M`
   - 检查可用内存: `free -h`

### 调试信息

启用详细日志:
```bash
export RKNN_LOG_LEVEL=5
./build/test_rknn_yolov8 -m models/yolov8n.rknn -i test.jpg -v
```

## 下一步计划

1. **性能优化**
   - 实现批处理推理
   - 优化内存使用
   - 添加多线程支持

2. **功能扩展**
   - 支持更多YOLO版本
   - 添加分割模型支持
   - 实现动态模型切换

3. **工具改进**
   - GUI模型转换工具
   - 实时性能监控
   - 自动化基准测试

## 参考资源

- [RKNN Model Zoo](https://github.com/airockchip/rknn_model_zoo)
- [RKNN-Toolkit2](https://github.com/airockchip/rknn-toolkit2)
- [RK3588 NPU文档](https://wiki.t-firefly.com/en/ROC-RK3588S-PC/usage_npu.html)
- [YOLOv8官方文档](https://docs.ultralytics.com/)

---

**注意**: 这是一个初始实现，建议在生产环境使用前进行充分测试和优化。
