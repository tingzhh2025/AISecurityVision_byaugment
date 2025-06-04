# AI Security Vision System

基于RK3588平台的AI安全视觉系统，支持YOLOv8 RKNN推理、人员统计、年龄性别识别等功能。

## 🚀 快速开始

### 编译和运行

```bash
# 编译项目
mkdir -p build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 运行后端服务
./AISecurityVision

# 运行前端服务（新终端）
cd ../web-ui
npm run dev
```

### 访问系统

- **前端界面**: http://localhost:3000
- **API服务**: http://localhost:8080
- **MJPEG流**: http://localhost:8161, 8162 (添加摄像头后)

## 📁 项目结构

```
├── src/                    # 源代码
│   ├── ai/                # AI推理模块
│   ├── api/               # API服务
│   ├── core/              # 核心组件
│   ├── database/          # 数据库管理
│   └── ...
├── web-ui/                # Vue3前端界面
├── models/                # AI模型文件
├── config/                # 配置文件
├── docs/                  # 项目文档
├── scripts/               # 部署脚本
├── tests/                 # 测试文件
│   ├── docs/             # 测试文档
│   ├── scripts/          # 测试脚本
│   ├── reference/        # 参考实现
│   └── logs/             # 测试日志
└── third_party/          # 第三方库
```

## ✨ 主要功能

### 🎯 AI检测与识别
- **YOLOv8 RKNN推理**: 支持80类COCO目标检测
- **动态类别过滤**: 实时选择检测类别，无需重启
- **人员统计**: 基于检测和跟踪的人员计数
- **年龄性别识别**: 基于InsightFace的人脸分析

### 📹 视频处理
- **多路RTSP流**: 支持多个IP摄像头同时接入
- **实时MJPEG流**: 提供HTTP MJPEG可视化流
- **跨摄像头跟踪**: 全局人员跟踪和重识别

### 🌐 Web界面
- **Vue3响应式界面**: 现代化的用户界面
- **实时监控**: 多路视频流同时显示
- **配置管理**: 摄像头、AI参数、检测类别配置
- **统计报表**: 人员统计和系统状态监控

### 🔧 系统特性
- **数据库持久化**: SQLite数据库存储配置和统计
- **RESTful API**: 完整的API接口
- **实时更新**: 配置变更实时生效
- **性能监控**: 系统资源和推理性能监控

## 🎛️ 新功能：动态检测类别过滤

### 功能特点
- **80类COCO检测**: 支持所有YOLOv8预训练类别
- **实时过滤**: 选择后立即生效，无需重启系统
- **分类管理**: 按人员车辆、动物、日用品等分类显示
- **快速选择**: 全选、全不选、常用类别快速操作
- **中文界面**: 所有类别名称本地化显示

### 使用方法
1. 访问前端界面 → 设置页面
2. 点击"配置检测类别"按钮
3. 选择需要检测的类别
4. 点击"保存设置"即可生效

### API接口
- `GET /api/detection/categories/available` - 获取所有可用类别
- `GET /api/detection/categories` - 获取当前启用类别
- `POST /api/detection/categories` - 更新启用类别

## 🔧 技术栈

### 后端
- **C++17**: 现代C++标准
- **OpenCV 4.5.5**: 计算机视觉库
- **RKNN NPU**: RK3588 NPU加速
- **SQLite**: 轻量级数据库
- **httplib**: HTTP服务器
- **InsightFace**: 人脸识别库

### 前端
- **Vue 3**: 渐进式JavaScript框架
- **Element Plus**: Vue 3 UI组件库
- **Vite**: 现代前端构建工具
- **Axios**: HTTP客户端

### AI模型
- **YOLOv8n RKNN**: 目标检测模型
- **InsightFace**: 人脸识别和年龄性别分析
- **ByteTracker**: 多目标跟踪算法

## 📊 性能指标

- **推理速度**: 13-29ms (YOLOv8n RKNN)
- **检测精度**: 支持80类COCO目标
- **并发处理**: 支持8路摄像头同时处理
- **内存占用**: 优化的内存管理
- **NPU利用率**: 充分利用RK3588 NPU性能

## 🛠️ 开发和测试

### 测试文件位置
- **单元测试**: `tests/*.cpp`
- **API测试**: `tests/playwright/`
- **集成测试**: `tests/scripts/`
- **参考实现**: `tests/reference/`

### 开发工具
- **性能监控**: `tests/scripts/monitor_performance.sh`
- **API测试**: `tests/scripts/test_integration.sh`
- **日志分析**: `tests/logs/`

## 📝 许可证

本项目采用MIT许可证 - 详见LICENSE文件

## 🤝 贡献

欢迎提交Issue和Pull Request来改进项目！

---

**AI Security Vision System** - 基于RK3588的智能安防视觉解决方案
