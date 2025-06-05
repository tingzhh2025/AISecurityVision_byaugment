# AI Security Vision API Testing Suite

这个目录包含了用于测试AI Security Vision系统API端点的全面测试工具。

## 📁 文件说明

### 测试脚本

1. **`api_endpoint_tester.py`** - 主要的Python测试脚本
   - 自动发现所有API端点（从后端和前端代码）
   - 执行全面的端点测试
   - 生成详细的HTML和Markdown报告
   - 提供彩色控制台输出

2. **`run_api_tests.sh`** - 测试运行器脚本
   - 检查系统依赖
   - 验证后端服务状态
   - 运行Python测试脚本
   - 自动打开测试报告

3. **`quick_api_test.sh`** - 快速curl测试脚本
   - 使用curl进行基本API测试
   - 不需要Python依赖
   - 提供快速的端点状态检查
   - 适合CI/CD环境

## 🚀 使用方法

### 前提条件

1. **确保后端服务正在运行**：
   ```bash
   cd /userdata/source/source/AISecurityVision_byaugment/build
   ./AISecurityVision
   ```

2. **安装Python依赖**（仅用于完整测试）：
   ```bash
   pip3 install requests --user
   ```

### 运行测试

#### 方法1：完整测试（推荐）
```bash
cd /userdata/source/source/AISecurityVision_byaugment/scripts
chmod +x run_api_tests.sh
./run_api_tests.sh
```

#### 方法2：直接运行Python脚本
```bash
cd /userdata/source/source/AISecurityVision_byaugment/scripts
python3 api_endpoint_tester.py
```

#### 方法3：快速curl测试
```bash
cd /userdata/source/source/AISecurityVision_byaugment/scripts
chmod +x quick_api_test.sh
./quick_api_test.sh
```

## 📊 测试报告

### 生成的报告文件

1. **`api_test_report.html`** - 详细的HTML报告
   - 美观的可视化界面
   - 按类别分组的端点
   - 响应时间统计
   - 实现状态概览

2. **`api_test_report.md`** - Markdown格式报告
   - 适合文档和版本控制
   - 包含实现建议
   - 技术说明和下一步计划

### 报告内容

- **总体统计**：总端点数、已实现数量、未实现数量
- **按类别分析**：系统管理、摄像头管理、AI检测等
- **详细结果**：每个端点的测试状态、响应时间、错误信息
- **实现建议**：优先级排序的开发建议

## 🎯 测试覆盖范围

### 已测试的API类别

1. **系统管理** (System Management)
   - 系统状态、信息、配置
   - 系统指标和统计

2. **摄像头管理** (Camera Management)
   - CRUD操作（创建、读取、更新、删除）
   - 连接测试和配置管理

3. **人员统计** (Person Statistics)
   - 统计数据获取和配置
   - 启用/禁用功能

4. **AI检测** (AI Detection)
   - 检测类别管理
   - 检测配置和统计

5. **网络管理** (Network Management)
   - 网络接口信息
   - 网络测试和统计

6. **ONVIF发现** (ONVIF Discovery)
   - 设备发现和添加

7. **认证系统** (Authentication)
   - 登录/登出（占位符）

8. **录像管理** (Recording Management)
   - 录像列表和操作（占位符）

9. **日志系统** (Logging)
   - 系统日志查询（占位符）

### 测试数据

测试脚本使用真实的测试数据：
- 使用现有摄像头ID（camera_ch2, camera_ch3）
- 真实的RTSP URL格式
- 有效的网络配置参数
- 合理的检测类别设置

## 🔧 配置选项

### 环境变量

可以通过环境变量自定义测试行为：

```bash
# 设置后端服务地址
export API_BASE_URL="http://localhost:8080"

# 设置请求超时时间
export API_TIMEOUT=10

# 设置测试摄像头ID
export TEST_CAMERA_ID="camera_ch2"
```

### 自定义测试数据

编辑`api_endpoint_tester.py`中的`test_data`字典来自定义测试数据：

```python
self.test_data = {
    'camera_config': {
        "id": "your_test_camera",
        "name": "Your Test Camera",
        "rtsp_url": "rtsp://user:pass@ip:port/stream",
        # ... 其他配置
    }
}
```

## 📈 性能基准

### 预期响应时间

- **GET请求**：< 100ms
- **POST请求**：< 500ms
- **系统状态**：< 50ms
- **摄像头列表**：< 200ms

### 实现状态目标

- **生产环境**：> 90% 端点已实现
- **测试环境**：> 70% 端点已实现
- **开发环境**：> 50% 端点已实现

## 🐛 故障排除

### 常见问题

1. **"Backend service is not accessible"**
   - 确保AISecurityVision服务正在运行
   - 检查端口8080是否被占用
   - 验证防火墙设置

2. **"Python3 is not installed"**
   - 安装Python 3.6+
   - 确保python3命令可用

3. **"requests module not found"**
   - 运行：`pip3 install requests --user`

4. **权限错误**
   - 给脚本添加执行权限：`chmod +x *.sh`

### 调试模式

启用详细输出：
```bash
# 对于bash脚本
bash -x quick_api_test.sh

# 对于Python脚本
python3 -v api_endpoint_tester.py
```

## 🔄 持续集成

### 在CI/CD中使用

```yaml
# GitHub Actions 示例
- name: Test API Endpoints
  run: |
    cd scripts
    ./quick_api_test.sh
    
- name: Generate API Report
  run: |
    cd scripts
    python3 api_endpoint_tester.py
    
- name: Upload Reports
  uses: actions/upload-artifact@v2
  with:
    name: api-test-reports
    path: scripts/api_test_report.*
```

## 📝 贡献指南

### 添加新的测试端点

1. 在`api_endpoint_tester.py`的`_discover_backend_endpoints()`中添加新端点
2. 如果需要特殊测试数据，更新`_get_test_data()`方法
3. 在`quick_api_test.sh`中添加对应的curl测试

### 改进测试覆盖

1. 添加更多边界条件测试
2. 增加错误处理测试
3. 添加性能基准测试
4. 实现并发测试

---

**作者**: Augment Agent  
**日期**: 2024-12-19  
**版本**: 1.0.0
