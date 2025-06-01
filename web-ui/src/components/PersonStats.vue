<template>
  <div class="person-stats">
    <el-card class="stats-card">
      <template #header>
        <div class="card-header">
          <span>人员统计</span>
          <div class="header-actions">
            <el-switch
              v-model="enabled"
              @change="togglePersonStats"
              active-text="启用"
              inactive-text="禁用"
              :loading="loading"
            />
            <el-button 
              link 
              @click="showConfig = true"
              :disabled="!enabled"
            >
              <el-icon><Setting /></el-icon>
            </el-button>
            <el-button link @click="refreshStats">
              <el-icon><Refresh /></el-icon>
            </el-button>
          </div>
        </div>
      </template>

      <div v-if="!enabled" class="disabled-state">
        <el-empty description="人员统计功能已禁用">
          <el-button type="primary" @click="enableStats">启用人员统计</el-button>
        </el-empty>
      </div>

      <div v-else-if="loading" class="loading-state">
        <el-skeleton :rows="4" animated />
      </div>

      <div v-else class="stats-content">
        <!-- 总体统计 -->
        <div class="total-stats">
          <div class="stat-item total">
            <div class="stat-icon">
              <el-icon><User /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ stats.total_persons || 0 }}</div>
              <div class="stat-label">总人数</div>
            </div>
          </div>
        </div>

        <!-- 性别统计 -->
        <div class="gender-stats">
          <h4>性别分布</h4>
          <div class="stats-row">
            <div class="stat-item male">
              <div class="stat-icon">
                <el-icon><Male /></el-icon>
              </div>
              <div class="stat-info">
                <div class="stat-value">{{ stats.male_count || 0 }}</div>
                <div class="stat-label">男性</div>
              </div>
            </div>
            <div class="stat-item female">
              <div class="stat-icon">
                <el-icon><Female /></el-icon>
              </div>
              <div class="stat-info">
                <div class="stat-value">{{ stats.female_count || 0 }}</div>
                <div class="stat-label">女性</div>
              </div>
            </div>
          </div>
        </div>

        <!-- 年龄统计 -->
        <div class="age-stats">
          <h4>年龄分布</h4>
          <div class="stats-grid">
            <div class="stat-item child">
              <div class="stat-value">{{ stats.child_count || 0 }}</div>
              <div class="stat-label">儿童</div>
            </div>
            <div class="stat-item young">
              <div class="stat-value">{{ stats.young_count || 0 }}</div>
              <div class="stat-label">青年</div>
            </div>
            <div class="stat-item middle">
              <div class="stat-value">{{ stats.middle_count || 0 }}</div>
              <div class="stat-label">中年</div>
            </div>
            <div class="stat-item senior">
              <div class="stat-value">{{ stats.senior_count || 0 }}</div>
              <div class="stat-label">老年</div>
            </div>
          </div>
        </div>

        <!-- 最后更新时间 -->
        <div class="update-time">
          <el-text size="small" type="info">
            最后更新: {{ lastUpdateTime }}
          </el-text>
        </div>
      </div>
    </el-card>

    <!-- 配置对话框 -->
    <el-dialog
      v-model="showConfig"
      title="人员统计配置"
      width="500px"
    >
      <el-form :model="config" label-width="120px">
        <el-form-item label="性别阈值">
          <el-slider
            v-model="config.gender_threshold"
            :min="0.1"
            :max="1"
            :step="0.05"
            show-input
          />
        </el-form-item>
        
        <el-form-item label="年龄阈值">
          <el-slider
            v-model="config.age_threshold"
            :min="0.1"
            :max="1"
            :step="0.05"
            show-input
          />
        </el-form-item>
        
        <el-form-item label="批处理大小">
          <el-input-number
            v-model="config.batch_size"
            :min="1"
            :max="16"
          />
        </el-form-item>
        
        <el-form-item label="启用缓存">
          <el-switch v-model="config.enable_caching" />
        </el-form-item>
      </el-form>
      
      <template #footer>
        <el-button @click="showConfig = false">取消</el-button>
        <el-button type="primary" @click="saveConfig" :loading="configLoading">
          保存
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, onUnmounted, computed } from 'vue'
import { apiService } from '@/services/api'
import { ElMessage } from 'element-plus'
import dayjs from 'dayjs'

const props = defineProps({
  cameraId: {
    type: String,
    required: true
  },
  autoRefresh: {
    type: Boolean,
    default: true
  },
  refreshInterval: {
    type: Number,
    default: 5000 // 5秒
  }
})

// 响应式数据
const enabled = ref(false)
const loading = ref(false)
const configLoading = ref(false)
const showConfig = ref(false)
const lastUpdate = ref(null)

const stats = reactive({
  total_persons: 0,
  male_count: 0,
  female_count: 0,
  child_count: 0,
  young_count: 0,
  middle_count: 0,
  senior_count: 0
})

const config = reactive({
  enabled: false,
  gender_threshold: 0.7,
  age_threshold: 0.6,
  batch_size: 4,
  enable_caching: true
})

let refreshTimer = null

// 计算属性
const lastUpdateTime = computed(() => {
  return lastUpdate.value ? dayjs(lastUpdate.value).format('HH:mm:ss') : '--'
})

// 方法
const refreshStats = async () => {
  if (!enabled.value) return
  
  try {
    loading.value = true
    const response = await apiService.getPersonStats(props.cameraId)
    
    if (response.data.success) {
      Object.assign(stats, response.data.data)
      lastUpdate.value = new Date()
    }
  } catch (error) {
    console.error('Failed to refresh person stats:', error)
  } finally {
    loading.value = false
  }
}

const togglePersonStats = async (value) => {
  try {
    loading.value = true
    
    if (value) {
      await apiService.enablePersonStats(props.cameraId)
      ElMessage.success('人员统计已启用')
      await refreshStats()
    } else {
      await apiService.disablePersonStats(props.cameraId)
      ElMessage.success('人员统计已禁用')
      Object.assign(stats, {
        total_persons: 0,
        male_count: 0,
        female_count: 0,
        child_count: 0,
        young_count: 0,
        middle_count: 0,
        senior_count: 0
      })
    }
  } catch (error) {
    enabled.value = !value // 回滚状态
    ElMessage.error('操作失败: ' + (error.response?.data?.message || error.message))
  } finally {
    loading.value = false
  }
}

const enableStats = () => {
  enabled.value = true
  togglePersonStats(true)
}

const loadConfig = async () => {
  try {
    const response = await apiService.getPersonStatsConfig(props.cameraId)
    if (response.data.success) {
      Object.assign(config, response.data.data)
      enabled.value = config.enabled
    }
  } catch (error) {
    console.error('Failed to load person stats config:', error)
  }
}

const saveConfig = async () => {
  try {
    configLoading.value = true
    await apiService.updatePersonStatsConfig(props.cameraId, config)
    ElMessage.success('配置已保存')
    showConfig.value = false
    await refreshStats()
  } catch (error) {
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  } finally {
    configLoading.value = false
  }
}

const startAutoRefresh = () => {
  if (props.autoRefresh && props.refreshInterval > 0) {
    refreshTimer = setInterval(refreshStats, props.refreshInterval)
  }
}

const stopAutoRefresh = () => {
  if (refreshTimer) {
    clearInterval(refreshTimer)
    refreshTimer = null
  }
}

// 生命周期
onMounted(async () => {
  await loadConfig()
  if (enabled.value) {
    await refreshStats()
  }
  startAutoRefresh()
})

onUnmounted(() => {
  stopAutoRefresh()
})
</script>

<style scoped>
.person-stats {
  height: 100%;
}

.stats-card {
  height: 100%;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 8px;
}

.disabled-state,
.loading-state {
  padding: 40px 20px;
  text-align: center;
}

.stats-content {
  padding: 16px 0;
}

.total-stats {
  margin-bottom: 24px;
}

.stat-item {
  display: flex;
  align-items: center;
  padding: 12px;
  border-radius: 8px;
  margin-bottom: 8px;
}

.stat-item.total {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
}

.stat-item.male {
  background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
  color: white;
}

.stat-item.female {
  background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
  color: white;
}

.stat-icon {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.2);
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 12px;
  font-size: 20px;
}

.stat-info {
  flex: 1;
}

.stat-value {
  font-size: 24px;
  font-weight: bold;
  line-height: 1;
}

.stat-label {
  font-size: 14px;
  opacity: 0.9;
  margin-top: 4px;
}

.gender-stats,
.age-stats {
  margin-bottom: 20px;
}

.gender-stats h4,
.age-stats h4 {
  margin: 0 0 12px 0;
  font-size: 16px;
  color: #303133;
}

.stats-row {
  display: flex;
  gap: 12px;
}

.stats-row .stat-item {
  flex: 1;
}

.stats-grid {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 8px;
}

.stats-grid .stat-item {
  background: #f8f9fa;
  border: 1px solid #e9ecef;
  text-align: center;
  padding: 12px 8px;
}

.stats-grid .stat-value {
  font-size: 20px;
  font-weight: bold;
  color: #495057;
}

.stats-grid .stat-label {
  font-size: 12px;
  color: #6c757d;
  margin-top: 4px;
}

.update-time {
  text-align: center;
  margin-top: 16px;
  padding-top: 16px;
  border-top: 1px solid #f0f0f0;
}
</style>
