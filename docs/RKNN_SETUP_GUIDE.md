# RKNN NPU Support Setup Guide

本指南将帮助您在RK3588 Ubuntu环境中设置RKNN NPU支持，以实现YOLOv8的硬件加速推理。

## 系统要求

- **硬件**: RK3588开发板
- **操作系统**: Ubuntu 20.04/22.04 (aarch64)
- **内存**: 至少4GB RAM
- **存储**: 至少16GB可用空间

## 1. 安装RKNN运行时库

### 方法1: 从官方源安装 (推荐)

```bash
# 更新包管理器
sudo apt update

# 安装RKNN运行时库
sudo apt install -y librknn-api librknn-api-dev

# 验证安装
ls /usr/lib/librknn_api.so
ls /usr/include/rknn_api.h
```

### 方法2: 手动安装

如果官方源不可用，可以手动下载安装：

```bash
# 下载RKNN运行时库 (根据实际版本调整)
wget https://github.com/airockchip/rknn-toolkit2/releases/download/v2.3.0/rknn_toolkit2-2.3.0-cp38-cp38-linux_aarch64.whl

# 创建安装目录
sudo mkdir -p /opt/rknn/lib /opt/rknn/include

# 解压并安装库文件 (需要根据实际包结构调整)
# 将librknn_api.so复制到 /opt/rknn/lib/
# 将rknn_api.h复制到 /opt/rknn/include/

# 添加库路径
echo '/opt/rknn/lib' | sudo tee /etc/ld.so.conf.d/rknn.conf
sudo ldconfig
```

## 2. 安装RKNN-Toolkit2 (用于模型转换)

RKNN-Toolkit2用于将ONNX模型转换为RKNN格式：

```bash
# 安装Python依赖
sudo apt install -y python3-pip python3-dev

# 安装RKNN-Toolkit2
pip3 install rknn-toolkit2

# 安装其他依赖
pip3 install opencv-python numpy
```

## 3. 编译项目

确保RKNN库已正确安装后，编译项目：

```bash
cd /path/to/AISecurityVision_byaugment

# 创建构建目录
mkdir -p build
cd build

# 配置CMake
cmake ..

# 检查RKNN是否被检测到
# 应该看到: "RKNN found: TRUE"

# 编译
make -j$(nproc)
```

## 4. 准备YOLOv8 RKNN模型

### 下载预训练的ONNX模型

```bash
# 创建模型目录
mkdir -p models

# 下载YOLOv8n ONNX模型
wget https://github.com/ultralytics/assets/releases/download/v0.0.0/yolov8n.onnx -O models/yolov8n.onnx
```

### 转换为RKNN格式

```bash
# 使用提供的转换脚本
python3 scripts/convert_yolov8_to_rknn.py \
    --input models/yolov8n.onnx \
    --output models/yolov8n.rknn \
    --platform rk3588 \
    --quantize INT8

# 转换完成后，您将得到 models/yolov8n.rknn 文件
```

### 转换参数说明

- `--platform`: 目标平台 (rk3562, rk3566, rk3568, rk3576, rk3588)
- `--quantize`: 量化类型 (FP16, INT8)
- `--dataset`: 量化数据集路径 (可选，用于INT8量化)
- `--input-size`: 输入尺寸 (默认640,640)

## 5. 使用RKNN加速

### 在代码中使用

```cpp
#include "src/ai/YOLOv8Detector.h"

// 创建检测器
YOLOv8Detector detector;

// 方法1: 自动选择最佳后端 (推荐)
detector.initialize("models/yolov8n.rknn", InferenceBackend::AUTO);

// 方法2: 强制使用RKNN后端
detector.initialize("models/yolov8n.rknn", InferenceBackend::RKNN);

// 方法3: 使用OpenCV后端作为备选
detector.initialize("models/yolov8n.onnx", InferenceBackend::OPENCV);

// 检查当前使用的后端
std::cout << "Using backend: " << detector.getBackendName() << std::endl;

// 进行检测
cv::Mat frame = cv::imread("test_image.jpg");
auto detections = detector.detectObjects(frame);
```

### 运行时配置

程序会自动检测可用的后端并选择最佳选项：

1. **RKNN**: 如果RKNN库可用且模型为.rknn格式
2. **OpenCV**: 如果ONNX模型可用
3. **模拟**: 作为最后的备选方案

## 6. 性能优化

### NPU频率设置

```bash
# 查看当前NPU频率
cat /sys/class/devfreq/fdab0000.npu/cur_freq

# 设置最高频率 (需要root权限)
echo performance | sudo tee /sys/class/devfreq/fdab0000.npu/governor

# 或者设置具体频率
echo 1000000000 | sudo tee /sys/class/devfreq/fdab0000.npu/max_freq
```

### 内存优化

```bash
# 增加CMA内存 (在/boot/config.txt或内核参数中)
cma=256M

# 或者在运行时调整
echo 268435456 | sudo tee /proc/sys/vm/min_free_kbytes
```

## 7. 故障排除

### 常见问题

1. **RKNN库未找到**
   ```
   解决方案: 检查库路径，运行 sudo ldconfig
   ```

2. **模型加载失败**
   ```
   解决方案: 确保模型文件完整，检查文件权限
   ```

3. **推理速度慢**
   ```
   解决方案: 检查NPU频率设置，确保使用INT8量化
   ```

4. **内存不足**
   ```
   解决方案: 增加swap空间或调整CMA设置
   ```

### 调试信息

启用详细日志输出：

```cpp
// 在初始化前设置环境变量
setenv("RKNN_LOG_LEVEL", "5", 1);  // 0-5, 5为最详细
```

### 性能测试

```bash
# 运行性能测试
./AISecurityVision --test-performance --model models/yolov8n.rknn

# 比较不同后端性能
./AISecurityVision --benchmark --backends rknn,opencv
```

## 8. 支持的模型

当前支持的YOLOv8模型：

- **YOLOv8n**: 轻量级，适合实时应用
- **YOLOv8s**: 平衡性能和精度
- **YOLOv8m**: 中等模型，更高精度
- **YOLOv8l**: 大模型，最高精度

推荐在RK3588上使用YOLOv8n或YOLOv8s以获得最佳性能。

## 9. 参考资源

- [RKNN-Toolkit2 官方文档](https://github.com/airockchip/rknn-toolkit2)
- [RK3588 NPU 开发指南](https://wiki.t-firefly.com/en/ROC-RK3588S-PC/usage_npu.html)
- [YOLOv8 官方文档](https://docs.ultralytics.com/)
- [RKNN Model Zoo](https://github.com/airockchip/rknn_model_zoo)
