<template>
  <div class="dashboard">
    <!-- 统计卡片 -->
    <el-row :gutter="20" class="stats-row">
      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon camera-icon">
              <el-icon><VideoCamera /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ systemStore.cameras.length }}</div>
              <div class="stat-label">摄像头总数</div>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon online-icon">
              <el-icon><Connection /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ systemStore.activeCameras.length }}</div>
              <div class="stat-label">在线摄像头</div>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon alert-icon">
              <el-icon><Warning /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ systemStore.alertCount }}</div>
              <div class="stat-label">未读报警</div>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon system-icon">
              <el-icon><Monitor /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ systemUptime }}</div>
              <div class="stat-label">系统运行时间</div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 系统监控 -->
    <el-row :gutter="20" class="monitor-row">
      <el-col :span="12">
        <el-card class="monitor-card">
          <template #header>
            <div class="card-header">
              <span>系统性能</span>
              <el-button link @click="refreshSystemInfo">
                <el-icon><Refresh /></el-icon>
              </el-button>
            </div>
          </template>

          <div class="performance-metrics">
            <div class="metric-item">
              <div class="metric-label">CPU使用率</div>
              <el-progress
                :percentage="systemStore.systemInfo.cpuUsage"
                :color="getProgressColor(systemStore.systemInfo.cpuUsage)"
              />
            </div>

            <div class="metric-item">
              <div class="metric-label">内存使用率</div>
              <el-progress
                :percentage="systemStore.systemInfo.memoryUsage"
                :color="getProgressColor(systemStore.systemInfo.memoryUsage)"
              />
            </div>

            <div class="metric-item">
              <div class="metric-label">磁盘使用率</div>
              <el-progress
                :percentage="systemStore.systemInfo.diskUsage"
                :color="getProgressColor(systemStore.systemInfo.diskUsage)"
              />
            </div>

            <div class="metric-item">
              <div class="metric-label">系统温度</div>
              <div class="temperature">
                {{ systemStore.systemInfo.temperature }}°C
                <el-tag
                  :type="getTemperatureType(systemStore.systemInfo.temperature)"
                  size="small"
                >
                  {{ getTemperatureStatus(systemStore.systemInfo.temperature) }}
                </el-tag>
              </div>
            </div>
          </div>
        </el-card>
      </el-col>

      <el-col :span="12">
        <el-card class="monitor-card">
          <template #header>
            <div class="card-header">
              <span>最新报警</span>
              <el-button link @click="$router.push('/alerts')">
                查看全部
              </el-button>
            </div>
          </template>

          <div class="recent-alerts">
            <div
              v-for="alert in recentAlerts"
              :key="alert.id"
              class="alert-item"
              :class="alert.level"
            >
              <div class="alert-icon">
                <el-icon><Warning /></el-icon>
              </div>
              <div class="alert-content">
                <div class="alert-title">{{ alert.title }}</div>
                <div class="alert-time">{{ formatTime(alert.timestamp) }}</div>
              </div>
              <div class="alert-level">
                <el-tag :type="getAlertType(alert.level)" size="small">
                  {{ alert.level }}
                </el-tag>
              </div>
            </div>

            <div v-if="recentAlerts.length === 0" class="no-alerts">
              <el-icon><CircleCheck /></el-icon>
              <span>暂无报警信息</span>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 摄像头状态 -->
    <el-row :gutter="20" class="cameras-row">
      <el-col :span="24">
        <el-card class="cameras-card">
          <template #header>
            <div class="card-header">
              <span>摄像头状态</span>
              <el-button link @click="$router.push('/cameras')">
                管理摄像头
              </el-button>
            </div>
          </template>

          <div class="cameras-grid">
            <div
              v-for="camera in systemStore.cameras.slice(0, 8)"
              :key="camera.id"
              class="camera-item"
              @click="viewCamera(camera)"
            >
              <div class="camera-preview">
                <img
                  v-if="camera.status === 'online'"
                  :src="getStreamUrl(camera.id)"
                  :alt="camera.name"
                  @error="handleImageError"
                />
                <div v-else class="camera-offline">
                  <el-icon><VideoCamera /></el-icon>
                  <span>离线</span>
                </div>
              </div>

              <div class="camera-info">
                <div class="camera-name">{{ camera.name }}</div>
                <div class="camera-status">
                  <div class="status-indicator" :class="`status-${camera.status}`">
                    <div class="status-dot"></div>
                    <span>{{ camera.status === 'online' ? '在线' : '离线' }}</span>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>
  </div>
</template>

<script setup>
import { computed, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useSystemStore } from '@/stores/system'
import { apiService } from '@/services/api'
import dayjs from 'dayjs'

const router = useRouter()
const systemStore = useSystemStore()

// 计算属性
const systemUptime = computed(() => {
  const uptime = systemStore.systemInfo.uptime
  const days = Math.floor(uptime / 86400)
  const hours = Math.floor((uptime % 86400) / 3600)
  return `${days}天${hours}小时`
})

const recentAlerts = computed(() => {
  return systemStore.alerts
    .filter(alert => !alert.read)
    .slice(0, 5)
    .sort((a, b) => new Date(b.timestamp) - new Date(a.timestamp))
})

// 方法
const refreshSystemInfo = async () => {
  await systemStore.checkSystemStatus()
}

const getProgressColor = (percentage) => {
  if (percentage < 60) return '#67c23a'
  if (percentage < 80) return '#e6a23c'
  return '#f56c6c'
}

const getTemperatureType = (temp) => {
  if (temp < 60) return 'success'
  if (temp < 80) return 'warning'
  return 'danger'
}

const getTemperatureStatus = (temp) => {
  if (temp < 60) return '正常'
  if (temp < 80) return '偏高'
  return '过热'
}

const getAlertType = (level) => {
  const types = {
    'low': 'info',
    'medium': 'warning',
    'high': 'danger',
    'critical': 'danger'
  }
  return types[level] || 'info'
}

const formatTime = (timestamp) => {
  return dayjs(timestamp).format('MM-DD HH:mm')
}

const getStreamUrl = (cameraId) => {
  return apiService.getStreamUrl(cameraId)
}

const handleImageError = (event) => {
  event.target.style.display = 'none'
}

const viewCamera = (camera) => {
  router.push(`/live?camera=${camera.id}`)
}

onMounted(() => {
  // 组件挂载时刷新数据
  refreshSystemInfo()
})
</script>

<style scoped>
.dashboard {
  padding: 0;
}

.stats-row {
  margin-bottom: 20px;
}

.stat-card {
  height: 120px;
}

.stat-content {
  display: flex;
  align-items: center;
  height: 100%;
}

.stat-icon {
  width: 60px;
  height: 60px;
  border-radius: 12px;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 16px;
  font-size: 24px;
  color: #fff;
}

.camera-icon {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

.online-icon {
  background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
}

.alert-icon {
  background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
}

.system-icon {
  background: linear-gradient(135deg, #43e97b 0%, #38f9d7 100%);
}

.stat-info {
  flex: 1;
}

.stat-value {
  font-size: 32px;
  font-weight: bold;
  color: #303133;
  line-height: 1;
}

.stat-label {
  font-size: 14px;
  color: #909399;
  margin-top: 8px;
}

.monitor-row {
  margin-bottom: 20px;
}

.monitor-card {
  height: 300px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.performance-metrics {
  padding: 20px 0;
}

.metric-item {
  margin-bottom: 20px;
}

.metric-label {
  font-size: 14px;
  color: #606266;
  margin-bottom: 8px;
}

.temperature {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 18px;
  font-weight: bold;
}

.recent-alerts {
  max-height: 240px;
  overflow-y: auto;
}

.alert-item {
  display: flex;
  align-items: center;
  padding: 12px 0;
  border-bottom: 1px solid #f0f0f0;
}

.alert-item:last-child {
  border-bottom: none;
}

.alert-icon {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: #f56c6c;
  color: #fff;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 12px;
  font-size: 16px;
}

.alert-content {
  flex: 1;
}

.alert-title {
  font-size: 14px;
  color: #303133;
  margin-bottom: 4px;
}

.alert-time {
  font-size: 12px;
  color: #909399;
}

.no-alerts {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  height: 200px;
  color: #909399;
  font-size: 14px;
}

.no-alerts .el-icon {
  font-size: 48px;
  margin-bottom: 12px;
  color: #67c23a;
}

.cameras-card {
  min-height: 400px;
}

.cameras-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 16px;
  padding: 16px 0;
}

.camera-item {
  border: 1px solid #e4e7ed;
  border-radius: 8px;
  overflow: hidden;
  cursor: pointer;
  transition: all 0.3s;
}

.camera-item:hover {
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.1);
  transform: translateY(-2px);
}

.camera-preview {
  height: 120px;
  background: #000;
  position: relative;
  overflow: hidden;
}

.camera-preview img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.camera-offline {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #909399;
  font-size: 14px;
}

.camera-offline .el-icon {
  font-size: 32px;
  margin-bottom: 8px;
}

.camera-info {
  padding: 12px;
}

.camera-name {
  font-size: 14px;
  font-weight: 500;
  color: #303133;
  margin-bottom: 8px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.camera-status {
  display: flex;
  align-items: center;
  justify-content: space-between;
}
</style>
