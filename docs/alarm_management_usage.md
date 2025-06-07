# 报警管理使用指南

## 📖 概述

AISecurityVision的报警管理系统支持多种推送方式，包括HTTP POST、WebSocket广播和MQTT发布。本指南将帮助您配置和使用这些功能。

## 🚀 快速开始

### 1. 访问报警配置页面

在Web界面中，点击左侧导航菜单的"报警配置"进入配置管理页面。

### 2. 添加报警配置

点击"添加配置"按钮，选择推送方式并填写相关参数：

#### HTTP POST配置
- **配置ID**: 唯一标识符，如 `webhook_server`
- **URL**: 接收报警的HTTP端点，如 `https://your-server.com/webhook`
- **超时时间**: 请求超时时间（毫秒），建议5000-10000
- **请求头**: JSON格式的HTTP头，如认证信息

#### WebSocket配置
- **配置ID**: 唯一标识符，如 `websocket_broadcast`
- **端口**: WebSocket服务器端口，默认8081
- **心跳间隔**: 保持连接的心跳间隔（毫秒）

#### MQTT配置
- **配置ID**: 唯一标识符，如 `mqtt_broker`
- **Broker地址**: MQTT服务器地址
- **端口**: MQTT服务器端口，默认1883
- **Topic**: 发布主题，如 `aibox/alarms`
- **QoS**: 服务质量等级（0-2）
- **用户名/密码**: 认证信息（可选）

### 3. 配置优先级

设置报警优先级（1-5），数字越大优先级越高：
- **1-2**: 低优先级（一般信息）
- **3**: 中等优先级（重要事件）
- **4-5**: 高优先级（紧急事件）

## 📊 监控和管理

### 系统状态监控

配置页面显示实时系统状态：
- **待处理报警**: 队列中等待发送的报警数量
- **成功推送**: 已成功发送的报警总数
- **推送失败**: 发送失败的报警总数
- **平均延迟**: 报警处理的平均时间

### 配置管理操作

- **启用/禁用**: 通过开关控制配置的启用状态
- **测试**: 发送测试报警验证配置是否正常
- **编辑**: 修改现有配置参数
- **删除**: 移除不需要的配置

## 🔧 API接口

### 获取所有配置
```http
GET /api/alarms/config
```

### 添加新配置
```http
POST /api/alarms/config
Content-Type: application/json

{
  "id": "webhook_server",
  "method": "http",
  "url": "https://your-server.com/webhook",
  "timeout_ms": 5000,
  "priority": 3,
  "enabled": true
}
```

### 更新配置
```http
PUT /api/alarms/config/{configId}
Content-Type: application/json

{
  "enabled": false,
  "priority": 5
}
```

### 删除配置
```http
DELETE /api/alarms/config/{configId}
```

### 测试报警
```http
POST /api/alarms/test
Content-Type: application/json

{
  "event_type": "intrusion_detection",
  "camera_id": "camera_01"
}
```

### 获取系统状态
```http
GET /api/alarms/status
```

## 📝 报警消息格式

系统发送的报警消息采用JSON格式：

```json
{
  "event_type": "intrusion_detection",
  "camera_id": "camera_01",
  "rule_id": "zone_1_intrusion",
  "object_id": "person_123",
  "confidence": 0.95,
  "timestamp": "2024-01-15T10:30:00Z",
  "bounding_box": {
    "x": 100,
    "y": 150,
    "width": 80,
    "height": 120
  },
  "priority": 4,
  "alarm_id": "alarm_20240115_103000_001",
  "test_mode": false
}
```

## 🛠️ 配置示例

### 1. 企业微信机器人
```json
{
  "id": "wechat_bot",
  "method": "http",
  "url": "https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=YOUR_KEY",
  "timeout_ms": 5000,
  "headers": "{\"Content-Type\": \"application/json\"}",
  "priority": 4,
  "enabled": true
}
```

### 2. 钉钉机器人
```json
{
  "id": "dingtalk_bot",
  "method": "http",
  "url": "https://oapi.dingtalk.com/robot/send?access_token=YOUR_TOKEN",
  "timeout_ms": 5000,
  "priority": 3,
  "enabled": true
}
```

### 3. 本地MQTT服务器
```json
{
  "id": "local_mqtt",
  "method": "mqtt",
  "broker": "localhost",
  "port": 1883,
  "topic": "security/alarms",
  "qos": 1,
  "priority": 2,
  "enabled": true
}
```

## ⚠️ 注意事项

### 安全考虑
- 使用HTTPS URL确保数据传输安全
- 妥善保管认证信息（Token、密码等）
- 定期更新访问凭证

### 性能优化
- 合理设置超时时间，避免阻塞
- 根据网络条件调整重试策略
- 监控推送成功率，及时处理异常

### 故障排除
- 检查网络连通性
- 验证URL和认证信息
- 查看系统日志获取详细错误信息
- 使用测试功能验证配置

## 📞 技术支持

如遇到问题，请：
1. 查看系统日志获取详细错误信息
2. 使用测试功能验证配置是否正确
3. 检查网络连接和防火墙设置
4. 联系技术支持团队

## 🔄 更新日志

### v1.0.0
- 初始版本发布
- 支持HTTP、WebSocket、MQTT三种推送方式
- 提供Web配置界面
- 实现优先级队列和并发推送
