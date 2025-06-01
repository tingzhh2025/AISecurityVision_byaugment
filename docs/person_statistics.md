# 人员统计功能扩展

## 概述

本文档描述了AI安全视觉系统的人员统计功能扩展。该扩展基于现有的YOLOv8检测系统，添加了年龄性别识别和智能人员统计功能，同时保持与现有代码的完全兼容性。

## 核心特性

### 1. 人员检测过滤
- 从YOLOv8检测结果中自动过滤人员类别（COCO class 0）
- 智能边界框扩展和图像质量检查
- 支持置信度和尺寸过滤

### 2. 年龄性别识别
- 基于RKNN NPU的MobileNetV2多任务模型
- 支持性别识别：男性/女性
- 支持年龄分组：儿童/青年/中年/老年
- 可配置的置信度阈值

### 3. 实时统计
- 实时人员计数和属性统计
- 支持性别分布统计
- 支持年龄分布统计
- 向后兼容的数据结构

## 架构设计

### 最小改动原则
- **零破坏性改动**：现有功能完全不受影响
- **可选启用**：人员统计功能默认关闭
- **插件式设计**：新功能作为独立模块添加
- **向后兼容**：所有现有API保持不变

### 核心组件

```cpp
// 人员过滤器（静态工具类）
class PersonFilter {
    static std::vector<PersonDetection> filterPersons(...);
    static cv::Mat extractPersonCrop(...);
};

// 年龄性别分析器
class AgeGenderAnalyzer {
    bool initialize(const std::string& modelPath);
    std::vector<PersonAttributes> analyze(...);
};

// 扩展的帧结果结构
struct FrameResult {
    // ... 现有字段保持不变 ...
    
    struct PersonStats {
        int total_persons = 0;
        int male_count = 0;
        int female_count = 0;
        int child_count = 0;
        int young_count = 0;
        int middle_count = 0;
        int senior_count = 0;
        std::vector<cv::Rect> person_boxes;
        std::vector<std::string> person_genders;
        std::vector<std::string> person_ages;
    } personStats;  // 默认为空，不影响现有功能
};
```

## 使用方法

### 1. 启用人员统计

```cpp
// 获取视频管道实例
VideoPipeline* pipeline = taskManager.getPipeline("camera_id");

// 启用人员统计功能
pipeline->setPersonStatsEnabled(true);

// 检查状态
bool enabled = pipeline->isPersonStatsEnabled();
```

### 2. 获取统计结果

```cpp
// 在帧处理回调中获取统计数据
void onFrameProcessed(const FrameResult& result) {
    if (result.personStats.total_persons > 0) {
        std::cout << "检测到 " << result.personStats.total_persons << " 人" << std::endl;
        std::cout << "男性: " << result.personStats.male_count << std::endl;
        std::cout << "女性: " << result.personStats.female_count << std::endl;
        std::cout << "儿童: " << result.personStats.child_count << std::endl;
        std::cout << "青年: " << result.personStats.young_count << std::endl;
        std::cout << "中年: " << result.personStats.middle_count << std::endl;
        std::cout << "老年: " << result.personStats.senior_count << std::endl;
    }
}
```

### 3. 独立使用组件

```cpp
// 使用人员过滤器
std::vector<Detection> yoloDetections = detector->detectObjects(frame);
auto persons = PersonFilter::filterPersons(yoloDetections, frame);

// 使用年龄性别分析器
AgeGenderAnalyzer analyzer;
analyzer.initialize("models/age_gender_mobilenet.rknn");
auto attributes = analyzer.analyze(persons);

for (const auto& attr : attributes) {
    std::cout << "性别: " << attr.gender 
              << ", 年龄组: " << attr.age_group << std::endl;
}
```

## 模型要求

### 年龄性别识别模型
- **格式**: RKNN (.rknn)
- **输入**: 224x224x3 RGB图像
- **输出**: 
  - 性别分类：2类（female, male）
  - 年龄分组：4类（child, young, middle, senior）
- **建议模型**: MobileNetV2多任务模型
- **位置**: `models/age_gender_mobilenet.rknn`

### 模型转换示例

```python
from rknn.api import RKNN

rknn = RKNN()
rknn.config(mean_values=[[123.675, 116.28, 103.53]], 
           std_values=[[58.395, 57.12, 57.375]],
           target_platform='rk3588',
           quantized_algorithm='normal')

rknn.load_onnx(model='age_gender_mobilenet.onnx')
rknn.build(do_quantization=True, dataset='calibration.txt')
rknn.export_rknn('age_gender_mobilenet.rknn')
```

## 性能指标

### 预期性能（RK3588）
- **人员过滤**: < 1ms
- **年龄性别分析**: ~15ms/人
- **总体延迟**: < 50ms（5人以内）
- **内存增加**: < 100MB
- **准确率**: 
  - 性别识别: > 94%
  - 年龄分组: > 85%

### 优化建议
- 批量处理多个人员以提高效率
- 对稳定追踪目标减少分析频率
- 使用置信度阈值过滤低质量检测

## 配置参数

```cpp
// 年龄性别分析器配置
analyzer.setGenderThreshold(0.7f);    // 性别置信度阈值
analyzer.setAgeThreshold(0.6f);       // 年龄置信度阈值
analyzer.setBatchSize(4);             // 批处理大小

// 人员过滤器配置
auto persons = PersonFilter::filterByConfidence(allPersons, 0.5f);  // 置信度过滤
auto validPersons = PersonFilter::filterBySize(persons, 50, 100);   // 尺寸过滤
```

## API参考

### PersonFilter类
```cpp
static std::vector<PersonDetection> filterPersons(
    const std::vector<Detection>& detections,
    const cv::Mat& frame,
    const std::vector<int>& trackIds = {},
    int64_t timestamp = 0
);

static cv::Mat extractPersonCrop(
    const cv::Mat& frame,
    const cv::Rect& bbox,
    float padding = 0.1f
);

static PersonStats getBasicStats(
    const std::vector<PersonDetection>& persons
);
```

### AgeGenderAnalyzer类
```cpp
bool initialize(const std::string& modelPath);
std::vector<PersonAttributes> analyze(
    const std::vector<PersonDetection>& persons
);
PersonAttributes analyzeSingle(const cv::Mat& personCrop);
```

### VideoPipeline扩展
```cpp
void setPersonStatsEnabled(bool enabled);
bool isPersonStatsEnabled() const;
```

## 测试和验证

### 编译测试程序
```bash
cd tests
g++ -std=c++17 person_stats_test.cpp -o person_stats_test \
    -I../src -lopencv_core -lopencv_imgproc
./person_stats_test
```

### 集成测试
```bash
# 启用人员统计功能测试
curl -X POST http://localhost:8080/api/cameras/camera1/person_stats/enable

# 获取实时统计数据
curl http://localhost:8080/api/cameras/camera1/person_stats
```

## 故障排除

### 常见问题

1. **模型加载失败**
   - 检查模型文件路径和权限
   - 确认RKNN运行时环境正确安装

2. **性能问题**
   - 调整批处理大小
   - 增加置信度阈值减少处理量
   - 检查NPU资源使用情况

3. **准确率问题**
   - 确认模型量化质量
   - 检查输入图像预处理
   - 验证校准数据集质量

### 日志调试
```cpp
// 启用调试日志
LOG_DEBUG() << "[PersonStats] Processing " << persons.size() << " persons";
```

## 未来扩展

### 计划功能
- 人员轨迹分析
- 停留时间统计
- 人流密度分析
- 历史数据聚合
- 跨摄像头人员追踪

### 模型升级
- 更精细的年龄预测
- 表情识别
- 行为分析
- 服装属性识别

## 实施状态

### ✅ 已完成功能

1. **核心模块实现**
   - ✅ PersonFilter - 人员检测过滤器
   - ✅ AgeGenderAnalyzer - 年龄性别识别器
   - ✅ VideoPipeline扩展 - 可选处理逻辑
   - ✅ FrameResult扩展 - 向后兼容数据结构

2. **API接口实现**
   - ✅ GET `/api/cameras/{id}/person-stats` - 获取实时统计
   - ✅ POST `/api/cameras/{id}/person-stats/enable` - 启用功能
   - ✅ POST `/api/cameras/{id}/person-stats/disable` - 禁用功能
   - ✅ GET `/api/cameras/{id}/person-stats/config` - 获取配置
   - ✅ POST `/api/cameras/{id}/person-stats/config` - 更新配置

3. **工具和文档**
   - ✅ 测试程序 - `tests/person_stats_test.cpp`
   - ✅ 模型转换脚本 - `scripts/convert_age_gender_to_rknn.py`
   - ✅ API测试脚本 - `scripts/test_person_stats_api.sh`
   - ✅ 完整文档 - `docs/person_statistics.md`

### 🔧 快速开始

#### 1. 编译系统
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

#### 2. 测试功能
```bash
# 编译并运行测试程序
./person_stats_test

# 测试API接口
chmod +x scripts/test_person_stats_api.sh
./scripts/test_person_stats_api.sh camera1
```

#### 3. 启用人员统计
```bash
# 通过API启用
curl -X POST http://localhost:8080/api/cameras/camera1/person-stats/enable

# 获取实时统计
curl http://localhost:8080/api/cameras/camera1/person-stats
```

### 📋 下一步工作

1. **模型准备**
   - 获取或训练年龄性别识别ONNX模型
   - 使用提供的脚本转换为RKNN格式
   - 放置到 `models/age_gender_mobilenet.rknn`

2. **性能优化**
   - 根据实际使用情况调整批处理大小
   - 优化置信度阈值
   - 监控NPU资源使用

3. **前端集成**
   - 在Web界面中添加人员统计显示
   - 实现实时数据更新
   - 添加配置管理界面

### 🎯 使用示例

#### C++ 代码示例
```cpp
// 启用人员统计
VideoPipeline* pipeline = taskManager.getPipeline("camera1");
pipeline->setPersonStatsEnabled(true);

// 获取统计结果
void onFrameProcessed(const FrameResult& result) {
    if (result.personStats.total_persons > 0) {
        std::cout << "检测到 " << result.personStats.total_persons << " 人" << std::endl;
        std::cout << "男性: " << result.personStats.male_count << std::endl;
        std::cout << "女性: " << result.personStats.female_count << std::endl;
    }
}
```

#### API 使用示例
```bash
# 启用功能
curl -X POST http://localhost:8080/api/cameras/camera1/person-stats/enable

# 获取统计数据
curl http://localhost:8080/api/cameras/camera1/person-stats

# 更新配置
curl -X POST -H "Content-Type: application/json" \
  -d '{"enabled": true, "gender_threshold": 0.8}' \
  http://localhost:8080/api/cameras/camera1/person-stats/config
```

## 总结

人员统计功能扩展采用最小改动原则，在不影响现有功能的前提下，为AI安全视觉系统添加了强大的人员分析能力。该扩展具有良好的性能表现和易用性，为智能监控应用提供了重要的数据支持。

### 关键优势
- **零风险部署** - 不修改任何现有逻辑
- **渐进式启用** - 可以逐步测试和部署
- **高性能设计** - 基于RKNN NPU优化
- **完整API支持** - RESTful接口便于集成
- **详细文档** - 完整的使用指南和示例
