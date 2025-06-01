<template>
  <div class="person-statistics">
    <!-- 页面标题 -->
    <div class="page-header">
      <h2>人员统计分析</h2>
      <p>基于AI视觉识别的实时人员统计和分析</p>
    </div>

    <!-- 摄像头选择 -->
    <el-card class="camera-selector">
      <el-row :gutter="20" align="middle">
        <el-col :span="6">
          <el-select 
            v-model="selectedCamera" 
            placeholder="选择摄像头"
            @change="onCameraChange"
            style="width: 100%"
          >
            <el-option
              v-for="camera in systemStore.cameras"
              :key="camera.id"
              :label="camera.name"
              :value="camera.id"
            />
          </el-select>
        </el-col>
        
        <el-col :span="6">
          <el-switch
            v-model="autoRefresh"
            active-text="自动刷新"
            inactive-text="手动刷新"
          />
        </el-col>
        
        <el-col :span="6">
          <el-button @click="refreshData" :loading="loading">
            <el-icon><Refresh /></el-icon>
            刷新数据
          </el-button>
        </el-col>
        
        <el-col :span="6">
          <el-button type="primary" @click="showConfig = true" :disabled="!selectedCamera">
            <el-icon><Setting /></el-icon>
            配置
          </el-button>
        </el-col>
      </el-row>
    </el-card>

    <!-- 统计内容 -->
    <div v-if="!selectedCamera" class="no-camera">
      <el-empty description="请选择摄像头查看人员统计" />
    </div>

    <div v-else class="statistics-content">
      <!-- 实时统计卡片 -->
      <el-row :gutter="20" class="stats-cards">
        <el-col :span="8">
          <PersonStats 
            :camera-id="selectedCamera"
            :auto-refresh="autoRefresh"
            :refresh-interval="5000"
          />
        </el-col>
        
        <el-col :span="16">
          <!-- 实时视频流 -->
          <el-card class="video-card">
            <template #header>
              <div class="card-header">
                <span>实时视频</span>
                <el-tag :type="cameraStatus === 'online' ? 'success' : 'danger'">
                  {{ cameraStatus === 'online' ? '在线' : '离线' }}
                </el-tag>
              </div>
            </template>
            
            <div class="video-container">
              <img
                v-if="cameraStatus === 'online'"
                :src="getStreamUrl(selectedCamera)"
                :alt="selectedCameraName"
                @error="handleVideoError"
                class="video-stream"
              />
              <div v-else class="video-offline">
                <el-icon><VideoCamera /></el-icon>
                <span>摄像头离线</span>
              </div>
            </div>
          </el-card>
        </el-col>
      </el-row>

      <!-- 历史统计图表 -->
      <el-row :gutter="20" class="charts-row">
        <el-col :span="12">
          <el-card class="chart-card">
            <template #header>
              <span>性别分布趋势</span>
            </template>
            <div id="genderChart" class="chart-container"></div>
          </el-card>
        </el-col>
        
        <el-col :span="12">
          <el-card class="chart-card">
            <template #header>
              <span>年龄分布趋势</span>
            </template>
            <div id="ageChart" class="chart-container"></div>
          </el-card>
        </el-col>
      </el-row>

      <!-- 详细数据表格 -->
      <el-card class="data-table">
        <template #header>
          <div class="card-header">
            <span>历史统计数据</span>
            <el-button link @click="exportData">
              <el-icon><Download /></el-icon>
              导出数据
            </el-button>
          </div>
        </template>
        
        <el-table :data="historyData" stripe>
          <el-table-column prop="timestamp" label="时间" width="180">
            <template #default="{ row }">
              {{ formatTime(row.timestamp) }}
            </template>
          </el-table-column>
          <el-table-column prop="total_persons" label="总人数" width="100" />
          <el-table-column prop="male_count" label="男性" width="80" />
          <el-table-column prop="female_count" label="女性" width="80" />
          <el-table-column prop="child_count" label="儿童" width="80" />
          <el-table-column prop="young_count" label="青年" width="80" />
          <el-table-column prop="middle_count" label="中年" width="80" />
          <el-table-column prop="senior_count" label="老年" width="80" />
        </el-table>
        
        <div class="pagination">
          <el-pagination
            v-model:current-page="currentPage"
            v-model:page-size="pageSize"
            :page-sizes="[10, 20, 50, 100]"
            :total="totalRecords"
            layout="total, sizes, prev, pager, next, jumper"
            @size-change="handleSizeChange"
            @current-change="handleCurrentChange"
          />
        </div>
      </el-card>
    </div>

    <!-- 配置对话框 -->
    <el-dialog
      v-model="showConfig"
      title="人员统计配置"
      width="600px"
    >
      <PersonStatsConfig 
        v-if="selectedCamera"
        :camera-id="selectedCamera"
        @config-updated="onConfigUpdated"
      />
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted, onUnmounted, watch } from 'vue'
import { useSystemStore } from '@/stores/system'
import { apiService } from '@/services/api'
import PersonStats from '@/components/PersonStats.vue'
import { ElMessage } from 'element-plus'
import dayjs from 'dayjs'
import * as echarts from 'echarts'

const systemStore = useSystemStore()

// 响应式数据
const selectedCamera = ref('')
const autoRefresh = ref(true)
const loading = ref(false)
const showConfig = ref(false)
const currentPage = ref(1)
const pageSize = ref(20)
const totalRecords = ref(0)

const historyData = ref([])
let genderChart = null
let ageChart = null
let refreshTimer = null

// 计算属性
const selectedCameraName = computed(() => {
  const camera = systemStore.cameras.find(c => c.id === selectedCamera.value)
  return camera?.name || ''
})

const cameraStatus = computed(() => {
  const camera = systemStore.cameras.find(c => c.id === selectedCamera.value)
  return camera?.status || 'offline'
})

// 方法
const onCameraChange = (cameraId) => {
  selectedCamera.value = cameraId
  refreshData()
  initCharts()
}

const refreshData = async () => {
  if (!selectedCamera.value) return
  
  try {
    loading.value = true
    
    // 获取历史统计数据
    const response = await apiService.getStatistics({
      camera_id: selectedCamera.value,
      type: 'person_stats',
      page: currentPage.value,
      page_size: pageSize.value
    })
    
    if (response.data.success) {
      historyData.value = response.data.data.records || []
      totalRecords.value = response.data.data.total || 0
      updateCharts()
    }
  } catch (error) {
    console.error('Failed to refresh data:', error)
    ElMessage.error('数据刷新失败')
  } finally {
    loading.value = false
  }
}

const getStreamUrl = (cameraId) => {
  return apiService.getStreamUrl(cameraId)
}

const handleVideoError = (event) => {
  event.target.style.display = 'none'
}

const formatTime = (timestamp) => {
  return dayjs(timestamp).format('YYYY-MM-DD HH:mm:ss')
}

const handleSizeChange = (size) => {
  pageSize.value = size
  refreshData()
}

const handleCurrentChange = (page) => {
  currentPage.value = page
  refreshData()
}

const onConfigUpdated = () => {
  showConfig.value = false
  refreshData()
}

const exportData = () => {
  // 导出数据功能
  ElMessage.info('导出功能开发中...')
}

const initCharts = () => {
  // 初始化图表
  setTimeout(() => {
    const genderEl = document.getElementById('genderChart')
    const ageEl = document.getElementById('ageChart')
    
    if (genderEl && !genderChart) {
      genderChart = echarts.init(genderEl)
    }
    
    if (ageEl && !ageChart) {
      ageChart = echarts.init(ageEl)
    }
    
    updateCharts()
  }, 100)
}

const updateCharts = () => {
  if (!historyData.value.length) return
  
  // 性别分布图表
  if (genderChart) {
    const genderOption = {
      title: { text: '性别分布', left: 'center' },
      tooltip: { trigger: 'item' },
      series: [{
        type: 'pie',
        radius: '60%',
        data: [
          { value: historyData.value[0]?.male_count || 0, name: '男性' },
          { value: historyData.value[0]?.female_count || 0, name: '女性' }
        ]
      }]
    }
    genderChart.setOption(genderOption)
  }
  
  // 年龄分布图表
  if (ageChart) {
    const ageOption = {
      title: { text: '年龄分布', left: 'center' },
      tooltip: { trigger: 'item' },
      series: [{
        type: 'pie',
        radius: '60%',
        data: [
          { value: historyData.value[0]?.child_count || 0, name: '儿童' },
          { value: historyData.value[0]?.young_count || 0, name: '青年' },
          { value: historyData.value[0]?.middle_count || 0, name: '中年' },
          { value: historyData.value[0]?.senior_count || 0, name: '老年' }
        ]
      }]
    }
    ageChart.setOption(ageOption)
  }
}

const startAutoRefresh = () => {
  if (autoRefresh.value) {
    refreshTimer = setInterval(refreshData, 10000) // 10秒刷新
  }
}

const stopAutoRefresh = () => {
  if (refreshTimer) {
    clearInterval(refreshTimer)
    refreshTimer = null
  }
}

// 监听自动刷新状态
watch(autoRefresh, (newVal) => {
  if (newVal) {
    startAutoRefresh()
  } else {
    stopAutoRefresh()
  }
})

// 生命周期
onMounted(() => {
  if (systemStore.cameras.length > 0) {
    selectedCamera.value = systemStore.cameras[0].id
    refreshData()
  }
  
  setTimeout(initCharts, 500)
  startAutoRefresh()
})

onUnmounted(() => {
  stopAutoRefresh()
  
  if (genderChart) {
    genderChart.dispose()
  }
  if (ageChart) {
    ageChart.dispose()
  }
})
</script>

<style scoped>
.person-statistics {
  padding: 0;
}

.page-header {
  margin-bottom: 20px;
}

.page-header h2 {
  margin: 0 0 8px 0;
  color: #303133;
}

.page-header p {
  margin: 0;
  color: #909399;
  font-size: 14px;
}

.camera-selector {
  margin-bottom: 20px;
}

.no-camera {
  padding: 60px 20px;
  text-align: center;
}

.stats-cards {
  margin-bottom: 20px;
}

.video-card {
  height: 350px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.video-container {
  height: 280px;
  background: #000;
  border-radius: 4px;
  overflow: hidden;
  position: relative;
}

.video-stream {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.video-offline {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #909399;
  font-size: 16px;
}

.video-offline .el-icon {
  font-size: 48px;
  margin-bottom: 12px;
}

.charts-row {
  margin-bottom: 20px;
}

.chart-card {
  height: 300px;
}

.chart-container {
  height: 240px;
}

.data-table .pagination {
  margin-top: 20px;
  text-align: right;
}
</style>
