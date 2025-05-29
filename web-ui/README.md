# AI安防视频监控系统 - Web UI

基于Vue3 + Element Plus的AI安防视频监控系统前端界面，提供实时视频监控、录像回放、报警管理、摄像头管理等功能。

## 功能特性

### 🎯 核心功能
- **实时监控**: 多路视频流同时显示，支持1x1、2x2、3x3、4x4布局
- **录像回放**: 录像文件搜索、播放、下载、截图功能
- **报警管理**: AI检测报警记录查看、处理、统计分析
- **摄像头管理**: 摄像头添加、编辑、删除、连接测试
- **系统设置**: AI检测、录像存储、报警通知等参数配置

### 🚀 技术特性
- **响应式设计**: 支持桌面端和移动端自适应
- **实时数据**: WebSocket实时接收AI检测结果和系统状态
- **模块化架构**: 组件化开发，易于维护和扩展
- **状态管理**: Pinia状态管理，数据流清晰
- **API集成**: 完整的RESTful API接口封装

## 技术栈

- **框架**: Vue 3.3+ (Composition API)
- **UI库**: Element Plus 2.3+
- **状态管理**: Pinia 2.1+
- **路由**: Vue Router 4.2+
- **HTTP客户端**: Axios 1.4+
- **构建工具**: Vite 4.4+
- **图表库**: ECharts 5.4+
- **时间处理**: Day.js 1.11+

## 项目结构

```
web-ui/
├── public/                 # 静态资源
├── src/
│   ├── components/         # 公共组件
│   ├── layout/            # 布局组件
│   ├── views/             # 页面组件
│   │   ├── Dashboard.vue  # 仪表盘
│   │   ├── Live.vue       # 实时监控
│   │   ├── Playback.vue   # 录像回放
│   │   ├── Alerts.vue     # 报警管理
│   │   ├── Cameras.vue    # 摄像头管理
│   │   └── Settings.vue   # 系统设置
│   ├── stores/            # 状态管理
│   │   └── system.js      # 系统状态
│   ├── services/          # API服务
│   │   └── api.js         # API接口封装
│   ├── router/            # 路由配置
│   ├── style.css          # 全局样式
│   ├── App.vue           # 根组件
│   └── main.js           # 入口文件
├── package.json          # 依赖配置
├── vite.config.js        # Vite配置
└── index.html           # HTML模板
```

## 安装和运行

### 环境要求
- Node.js 16.0+
- npm 8.0+ 或 yarn 1.22+

### 安装依赖
```bash
cd web-ui
npm install
```

### 开发模式
```bash
npm run dev
```
访问 http://localhost:3000

### 生产构建
```bash
npm run build
```

### 预览构建结果
```bash
npm run preview
```

## API接口说明

前端通过以下API端点与C++后端通信：

### 系统相关
- `GET /api/system/status` - 获取系统状态
- `GET /api/system/info` - 获取系统信息
- `PUT /api/system/config` - 更新系统配置

### 摄像头相关
- `GET /api/cameras` - 获取摄像头列表
- `POST /api/cameras` - 添加摄像头
- `PUT /api/cameras/:id` - 更新摄像头
- `DELETE /api/cameras/:id` - 删除摄像头
- `POST /api/cameras/test` - 测试摄像头连接

### 视频流
- `GET /stream/camera/:id` - 获取摄像头视频流 (MJPEG)

### 录像相关
- `GET /api/recordings` - 获取录像列表
- `GET /api/recordings/:id/stream` - 播放录像
- `GET /api/recordings/:id/download` - 下载录像
- `DELETE /api/recordings/:id` - 删除录像

### 报警相关
- `GET /api/alerts` - 获取报警列表
- `PUT /api/alerts/:id/read` - 标记报警已读
- `DELETE /api/alerts/:id` - 删除报警

### WebSocket
- `ws://host/ws/detections` - 实时AI检测结果推送

## 配置说明

### 代理配置
在 `vite.config.js` 中配置API代理：

```javascript
server: {
  proxy: {
    '/api': {
      target: 'http://localhost:8080',  // C++后端地址
      changeOrigin: true
    },
    '/stream': {
      target: 'http://localhost:8161', // 视频流地址
      changeOrigin: true
    }
  }
}
```

### 环境变量
创建 `.env.local` 文件配置环境变量：

```bash
# API基础URL
VITE_API_BASE_URL=http://localhost:8080

# 视频流基础URL
VITE_STREAM_BASE_URL=http://localhost:8161

# WebSocket URL
VITE_WS_URL=ws://localhost:8080
```

## 主要功能模块

### 1. 仪表盘 (Dashboard)
- 系统状态概览
- 摄像头在线状态
- 报警统计信息
- 系统性能监控

### 2. 实时监控 (Live)
- 多路视频流显示
- 布局切换 (1x1, 2x2, 3x3, 4x4)
- AI检测结果实时显示
- 摄像头控制和设置

### 3. 录像回放 (Playback)
- 录像文件搜索和筛选
- 视频播放控制
- 录像下载和截图
- 批量管理操作

### 4. 报警管理 (Alerts)
- 报警记录查看和搜索
- 报警级别和类型分类
- 批量标记和删除
- 报警详情查看

### 5. 摄像头管理 (Cameras)
- 摄像头添加和配置
- 连接状态监控
- 参数设置和测试
- 批量管理操作

### 6. 系统设置 (Settings)
- 系统基本配置
- AI检测参数设置
- 录像存储配置
- 报警通知设置
- 网络服务配置

## 开发指南

### 添加新页面
1. 在 `src/views/` 创建Vue组件
2. 在 `src/router/index.js` 添加路由
3. 在布局组件中添加菜单项

### 添加新API
1. 在 `src/services/api.js` 添加API方法
2. 在对应的store中调用API
3. 在组件中使用store方法

### 自定义主题
修改 `src/style.css` 中的CSS变量：

```css
:root {
  --primary-color: #409eff;
  --success-color: #67c23a;
  /* 其他颜色变量 */
}
```

## 部署说明

### Nginx配置示例
```nginx
server {
    listen 80;
    server_name your-domain.com;
    
    # 前端静态文件
    location / {
        root /path/to/web-ui/dist;
        try_files $uri $uri/ /index.html;
    }
    
    # API代理
    location /api/ {
        proxy_pass http://localhost:8080/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
    
    # 视频流代理
    location /stream/ {
        proxy_pass http://localhost:8161/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
    }
    
    # WebSocket代理
    location /ws/ {
        proxy_pass http://localhost:8080/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
    }
}
```

## 浏览器支持

- Chrome 88+
- Firefox 85+
- Safari 14+
- Edge 88+

## 许可证

MIT License

## 贡献

欢迎提交Issue和Pull Request来改进项目。
