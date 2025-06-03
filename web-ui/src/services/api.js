import axios from 'axios'
import { ElMessage } from 'element-plus'

// 创建axios实例
const api = axios.create({
  baseURL: '/api',
  timeout: 30000, // 增加到30秒
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
      // 检查是否是超时错误
      if (error.code === 'ECONNABORTED') {
        ElMessage.error('请求超时，请检查网络连接或稍后重试')
      } else {
        ElMessage.error('网络连接失败')
      }
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

  // 网络接口管理
  getNetworkInterfaces: () => api.get('/network/interfaces'),
  getNetworkInterface: (interfaceName) => api.get(`/network/interfaces/${interfaceName}`),
  configureNetworkInterface: (interfaceName, config) => api.post(`/network/interfaces/${interfaceName}`, config),
  enableNetworkInterface: (interfaceName) => api.post(`/network/interfaces/${interfaceName}/enable`),
  disableNetworkInterface: (interfaceName) => api.post(`/network/interfaces/${interfaceName}/disable`),
  getNetworkStats: () => api.get('/network/stats'),
  testNetworkConnection: (testConfig) => api.post('/network/test', testConfig),
  updateSystemConfig: (config) => api.put('/system/config', config),

  // 配置管理相关
  getSystemConfig: () => api.get('/system/config'),
  saveSystemConfig: (config) => api.post('/system/config', config),
  getCameraConfigs: () => api.get('/cameras/config'),
  saveCameraConfig: (config) => api.post('/cameras/config', config),
  deleteCameraConfig: (cameraId) => api.delete(`/cameras/config/${cameraId}`),
  getConfigCategory: (category) => api.get(`/config/${category}`),

  // 摄像头相关
  getCameras: () => api.get('/cameras'),
  getCamera: (id) => api.get(`/cameras/${id}`),
  addCamera: (camera) => api.post('/cameras', camera),
  updateCamera: (id, camera) => api.put(`/cameras/${id}`, camera),
  deleteCamera: (id) => api.delete(`/cameras/${id}`),
  testCamera: (camera) => api.post('/cameras/test', camera),
  testCameraConnection: (connectionData) => api.post('/cameras/test-connection', connectionData),

  // 实时视频流
  getStreamUrl: async (cameraId) => {
    try {
      // 首先尝试从API获取摄像头信息
      const response = await api.get('/cameras')
      const cameras = response.data.cameras || []
      const camera = cameras.find(cam => cam.id === cameraId)

      if (camera && camera.mjpeg_port) {
        // 直接访问MJPEG流，不通过Vite代理
        return `http://127.0.0.1:${camera.mjpeg_port}/stream.mjpg`
      }

      // 如果API调用失败，回退到静态映射
      const streamMapping = {
        'camera_01': 'http://127.0.0.1:8161/stream.mjpg',
        'camera_02': 'http://127.0.0.1:8162/stream.mjpg',
        'camera_03': 'http://127.0.0.1:8163/stream.mjpg',
        'camera_04': 'http://127.0.0.1:8164/stream.mjpg',
        'camera_05': 'http://127.0.0.1:8165/stream.mjpg',
        'camera_06': 'http://127.0.0.1:8166/stream.mjpg',
        'camera_07': 'http://127.0.0.1:8167/stream.mjpg',
        'camera_08': 'http://127.0.0.1:8168/stream.mjpg',
        'test_camera': 'http://127.0.0.1:8161/stream.mjpg'
      }

      return streamMapping[cameraId] || 'http://127.0.0.1:8161/stream.mjpg'
    } catch (error) {
      console.error('Failed to get camera info for stream URL:', error)
      return 'http://127.0.0.1:8161/stream.mjpg'
    }
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

  // 检测类别过滤相关
  getDetectionCategories: () => api.get('/detection/categories'),
  updateDetectionCategories: (categories) => api.post('/detection/categories', categories),
  getAvailableCategories: () => api.get('/detection/categories/available'),

  // 用户相关
  login: (credentials) => api.post('/auth/login', credentials),
  logout: () => api.post('/auth/logout'),
  getCurrentUser: () => api.get('/auth/user'),

  // 日志相关
  getLogs: (params) => api.get('/logs', { params }),

  // 统计相关
  getStatistics: (params) => api.get('/statistics', { params }),

  // 人员统计相关 (Person Statistics Extension)
  getPersonStats: (cameraId) => api.get(`/cameras/${cameraId}/person-stats`),
  enablePersonStats: (cameraId) => api.post(`/cameras/${cameraId}/person-stats/enable`),
  disablePersonStats: (cameraId) => api.post(`/cameras/${cameraId}/person-stats/disable`),
  getPersonStatsConfig: (cameraId) => api.get(`/cameras/${cameraId}/person-stats/config`),
  updatePersonStatsConfig: (cameraId, config) => api.post(`/cameras/${cameraId}/person-stats/config`, config)
}

export default api
