import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { apiService } from '@/services/api'

export const useSystemStore = defineStore('system', () => {
  // 系统状态
  const isOnline = ref(false)
  const systemInfo = ref({
    version: '',
    uptime: 0,
    cpuUsage: 0,
    memoryUsage: 0,
    diskUsage: 0,
    temperature: 0
  })

  // 摄像头状态
  const cameras = ref([])
  const activeCameras = computed(() => cameras.value.filter(camera => camera.status === 'online'))

  // 报警状态
  const alerts = ref([])
  const unreadAlerts = computed(() => alerts.value.filter(alert => !alert.read))
  const alertCount = computed(() => unreadAlerts.value.length)

  // 检查系统状态
  const checkSystemStatus = async () => {
    try {
      const response = await apiService.getSystemStatus()
      isOnline.value = true
      systemInfo.value = response.data
    } catch (error) {
      console.error('Failed to check system status:', error)
      isOnline.value = false
    }
  }

  // 获取摄像头列表
  const fetchCameras = async () => {
    try {
      const response = await apiService.getCameras()
      cameras.value = response.data.cameras || []
    } catch (error) {
      console.error('Failed to fetch cameras:', error)
      cameras.value = []
    }
  }

  // 获取报警列表
  const fetchAlerts = async () => {
    try {
      const response = await apiService.getAlerts()
      alerts.value = response.data.alerts || []
    } catch (error) {
      console.error('Failed to fetch alerts:', error)
      alerts.value = []
    }
  }

  // 标记报警为已读
  const markAlertAsRead = async (alertId) => {
    try {
      await apiService.markAlertAsRead(alertId)
      const alert = alerts.value.find(a => a.id === alertId)
      if (alert) {
        alert.read = true
      }
    } catch (error) {
      console.error('Failed to mark alert as read:', error)
    }
  }

  // 初始化系统
  const initializeSystem = async () => {
    await checkSystemStatus()
    await fetchCameras()
    await fetchAlerts()

    // 定期检查系统状态
    setInterval(checkSystemStatus, 30000) // 30秒检查一次
    setInterval(fetchAlerts, 10000) // 10秒检查一次报警
  }

  return {
    // 状态
    isOnline,
    systemInfo,
    cameras,
    activeCameras,
    alerts,
    unreadAlerts,
    alertCount,

    // 方法
    checkSystemStatus,
    fetchCameras,
    fetchAlerts,
    markAlertAsRead,
    initializeSystem
  }
})
