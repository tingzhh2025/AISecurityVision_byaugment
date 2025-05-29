import axios from 'axios'
import { ElMessage } from 'element-plus'

// 创建axios实例
const api = axios.create({
  baseURL: '/api',
  timeout: 10000,
  headers: {
    'Content-Type': 'application/json'
  }
})

// 请求拦截器
api.interceptors.request.use(
  config => {
    // 可以在这里添加token等认证信息
    return config
  },
  error => {
    return Promise.reject(error)
  }
)

// 响应拦截器
api.interceptors.response.use(
  response => {
    return response
  },
  error => {
    console.error('API Error:', error)

    if (error.response) {
      const { status, data } = error.response
      let message = data?.message || '请求失败'

      switch (status) {
        case 400:
          message = '请求参数错误'
          break
        case 401:
          message = '未授权访问'
          break
        case 403:
          message = '禁止访问'
          break
        case 404:
          message = '资源不存在'
          break
        case 500:
          message = '服务器内部错误'
          break
        default:
          message = `请求失败 (${status})`
      }

      ElMessage.error(message)
    } else if (error.request) {
      ElMessage.error('网络连接失败')
    } else {
      ElMessage.error('请求配置错误')
    }

    return Promise.reject(error)
  }
)

// API服务
export const apiService = {
  // 系统相关
  getSystemStatus: () => api.get('/system/status'),
  getSystemInfo: () => api.get('/system/info'),
  updateSystemConfig: (config) => api.put('/system/config', config),

  // 摄像头相关
  getCameras: () => api.get('/cameras'),
  getCamera: (id) => api.get(`/cameras/${id}`),
  addCamera: (camera) => api.post('/cameras', camera),
  updateCamera: (id, camera) => api.put(`/cameras/${id}`, camera),
  deleteCamera: (id) => api.delete(`/cameras/${id}`),
  testCamera: (camera) => api.post('/cameras/test', camera),
  testCameraConnection: (connectionData) => api.post('/cameras/test-connection', connectionData),

  // 实时视频流
  getStreamUrl: (cameraId) => {
    // 根据摄像头ID返回正确的MJPEG流地址
    if (cameraId === 'camera_01') {
      return 'http://localhost:8161/stream.mjpg'
    } else if (cameraId === 'camera_02') {
      return 'http://localhost:8162/stream.mjpg'
    }
    // 默认返回第一个摄像头的流
    return 'http://localhost:8161/stream.mjpg'
  },

  // 录像相关
  getRecordings: (params) => api.get('/recordings', { params }),
  getRecording: (id) => api.get(`/recordings/${id}`),
  deleteRecording: (id) => api.delete(`/recordings/${id}`),
  downloadRecording: (id) => api.get(`/recordings/${id}/download`, { responseType: 'blob' }),

  // 报警相关
  getAlerts: (params) => api.get('/alerts', { params }),
  getAlert: (id) => api.get(`/alerts/${id}`),
  markAlertAsRead: (id) => api.put(`/alerts/${id}/read`),
  deleteAlert: (id) => api.delete(`/alerts/${id}`),

  // AI检测相关
  getDetectionConfig: () => api.get('/detection/config'),
  updateDetectionConfig: (config) => api.put('/detection/config', config),
  getDetectionStats: () => api.get('/detection/stats'),

  // 用户相关
  login: (credentials) => api.post('/auth/login', credentials),
  logout: () => api.post('/auth/logout'),
  getCurrentUser: () => api.get('/auth/user'),

  // 日志相关
  getLogs: (params) => api.get('/logs', { params }),

  // 统计相关
  getStatistics: (params) => api.get('/statistics', { params })
}

export default api
