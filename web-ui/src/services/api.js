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
    // 开发环境下记录请求
    if (process.env.NODE_ENV === 'development') {
      console.log(`[API Request] ${config.method?.toUpperCase()} ${config.url}`, config.data || '')
    }
    return config
  },
  error => {
    return Promise.reject(error)
  }
)

// 响应拦截器
api.interceptors.response.use(
  response => {
    // 开发环境下记录响应
    if (process.env.NODE_ENV === 'development') {
      console.log(`[API Response] ${response.config.method?.toUpperCase()} ${response.config.url}`, response.data)
    }
    return response
  },
  error => {
    console.error('API Error:', error)

    if (error.response) {
      const { status, data } = error.response
      let message = data?.message || '请求失败'
      let showMessage = true

      switch (status) {
        case 400:
          message = data?.message || '请求参数错误'
          break
        case 401:
          message = '未授权访问'
          break
        case 403:
          message = '禁止访问'
          break
        case 404:
          message = '请求的资源不存在'
          break
        case 500:
          message = '服务器内部错误'
          break
        case 501:
          // Not Implemented - 功能尚未实现
          message = '该功能尚未实现'
          showMessage = process.env.NODE_ENV === 'development' // 仅在开发环境显示
          break
        default:
          message = `请求失败 (${status})`
      }

      if (showMessage) {
        ElMessage.error(message)
      }
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
  getSystemConfig: () => api.get('/system/config'),
  updateSystemConfig: (config) => api.post('/system/config', config),
  saveSystemConfig: (config) => api.post('/system/config', config),
  getSystemMetrics: () => api.get('/system/metrics'),
  getSystemStats: () => api.get('/system/stats'),
  getPipelineStats: () => api.get('/system/pipeline-stats'),

  // 网络接口管理
  getNetworkInterfaces: () => api.get('/network/interfaces'),
  getNetworkInterface: (interfaceName) => api.get(`/network/interfaces/${interfaceName}`),
  configureNetworkInterface: (interfaceName, config) => api.post(`/network/interfaces/${interfaceName}`, config),
  enableNetworkInterface: (interfaceName) => api.post(`/network/interfaces/${interfaceName}/enable`),
  disableNetworkInterface: (interfaceName) => api.post(`/network/interfaces/${interfaceName}/disable`),
  getNetworkStats: () => api.get('/network/stats'),
  testNetworkConnection: (testConfig) => api.post('/network/test', testConfig),

  // 配置管理相关
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
        // 使用当前页面的hostname，确保与前端访问地址一致
        const hostname = window.location.hostname
        return `http://${hostname}:${camera.mjpeg_port}/stream.mjpg`
      }

      // 如果API调用失败，回退到静态映射
      const hostname = window.location.hostname
      const streamMapping = {
        'camera_01': `http://${hostname}:8161/stream.mjpg`,
        'camera_02': `http://${hostname}:8162/stream.mjpg`,
        'camera_03': `http://${hostname}:8163/stream.mjpg`,
        'camera_04': `http://${hostname}:8164/stream.mjpg`,
        'camera_05': `http://${hostname}:8165/stream.mjpg`,
        'camera_06': `http://${hostname}:8166/stream.mjpg`,
        'camera_07': `http://${hostname}:8167/stream.mjpg`,
        'camera_08': `http://${hostname}:8168/stream.mjpg`,
        'test_camera': `http://${hostname}:8161/stream.mjpg`
      }

      return streamMapping[cameraId] || `http://${hostname}:8161/stream.mjpg`
    } catch (error) {
      console.error('Failed to get camera info for stream URL:', error)
      const hostname = window.location.hostname
      return `http://${hostname}:8161/stream.mjpg`
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
  getAlarmConfigs: () => api.get('/alarms/config'),
  saveAlarmConfig: (config) => api.post('/alarms/config', config),
  updateAlarmConfig: (id, config) => api.put(`/alarms/config/${id}`, config),
  deleteAlarmConfig: (id) => api.delete(`/alarms/config/${id}`),
  testAlarm: (config) => api.post('/alarms/test', config),
  getAlarmStatus: () => api.get('/alarms/status'),

  // AI检测相关
  getDetectionConfig: () => api.get('/detection/config'),
  updateDetectionConfig: (config) => api.put('/detection/config', config),
  getDetectionStats: () => api.get('/detection/stats'),

  // 检测类别过滤相关
  getDetectionCategories: () => api.get('/detection/categories'),
  updateDetectionCategories: (categories) => api.post('/detection/categories', { enabled_categories: categories }),
  getAvailableCategories: () => api.get('/detection/categories/available'),

  // 用户相关 (占位符，返回模拟数据)
  login: async (credentials) => {
    try {
      return await api.post('/auth/login', credentials)
    } catch (error) {
      if (error.response?.status === 501) {
        // 模拟登录成功
        return {
          data: {
            token: 'mock-token-' + Date.now(),
            user: {
              id: 1,
              username: credentials.username,
              role: 'admin'
            }
          }
        }
      }
      throw error
    }
  },
  logout: () => api.post('/auth/logout'),
  getCurrentUser: async () => {
    try {
      return await api.get('/auth/user')
    } catch (error) {
      if (error.response?.status === 501) {
        // 返回模拟用户
        return {
          data: {
            user: {
              id: 1,
              username: 'admin',
              role: 'admin'
            }
          }
        }
      }
      throw error
    }
  },

  // 日志相关
  getLogs: async (params) => {
    try {
      return await api.get('/logs', { params })
    } catch (error) {
      if (error.response?.status === 501) {
        // 返回模拟日志
        return {
          data: {
            logs: [],
            total: 0,
            message: '日志功能尚未实现'
          }
        }
      }
      throw error
    }
  },

  // 统计相关
  getStatistics: async (params) => {
    try {
      return await api.get('/statistics', { params })
    } catch (error) {
      if (error.response?.status === 501) {
        // 返回模拟统计
        return {
          data: {
            total_detections: 0,
            total_alarms: 0,
            active_cameras: 0,
            message: '统计功能尚未实现'
          }
        }
      }
      throw error
    }
  },

  // 人员统计相关 (Person Statistics Extension)
  getPersonStats: (cameraId) => api.get(`/cameras/${cameraId}/person-stats`),
  enablePersonStats: (cameraId) => api.post(`/cameras/${cameraId}/person-stats/enable`),
  disablePersonStats: (cameraId) => api.post(`/cameras/${cameraId}/person-stats/disable`),
  getPersonStatsConfig: (cameraId) => api.get(`/cameras/${cameraId}/person-stats/config`),
  updatePersonStatsConfig: (cameraId, config) => api.post(`/cameras/${cameraId}/person-stats/config`, config),

  // ONVIF设备发现
  discoverDevices: () => api.get('/source/discover'),
  addDiscoveredDevice: (deviceInfo) => api.post('/source/add-discovered', deviceInfo),

  // 视频源管理（兼容旧API）
  addVideoSource: (source) => api.post('/source/add', source),
  getVideoSources: () => api.get('/source/list')
}

// 导出API健康检查工具
export const checkApiHealth = async () => {
  const endpoints = [
    { name: '系统状态', path: '/system/status', method: 'GET' },
    { name: '摄像头列表', path: '/cameras', method: 'GET' },
    { name: '检测类别', path: '/detection/categories', method: 'GET' },
    { name: '网络接口', path: '/network/interfaces', method: 'GET' }
  ]

  const results = []
  
  for (const endpoint of endpoints) {
    try {
      await api.request({
        method: endpoint.method,
        url: endpoint.path,
        validateStatus: (status) => status < 500
      })
      results.push({ ...endpoint, status: 'ok' })
    } catch (error) {
      results.push({ 
        ...endpoint, 
        status: 'error',
        error: error.message 
      })
    }
  }

  return results
}

// 开发环境下添加到window对象以便调试
if (process.env.NODE_ENV === 'development') {
  window.__apiService = apiService
  window.__checkApiHealth = checkApiHealth
}

export default api
