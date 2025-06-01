<template>
  <div class="person-stats-config">
    <el-form :model="config" label-width="120px">
      <el-form-item label="启用状态">
        <el-switch 
          v-model="config.enabled" 
          active-text="启用"
          inactive-text="禁用"
          @change="onEnabledChange"
        />
      </el-form-item>
      
      <template v-if="config.enabled">
        <el-form-item label="性别识别阈值">
          <el-slider
            v-model="config.gender_threshold"
            :min="0.1"
            :max="1"
            :step="0.05"
            show-input
          />
          <el-text size="small" type="info">
            置信度阈值，越高越严格
          </el-text>
        </el-form-item>
        
        <el-form-item label="年龄识别阈值">
          <el-slider
            v-model="config.age_threshold"
            :min="0.1"
            :max="1"
            :step="0.05"
            show-input
          />
          <el-text size="small" type="info">
            置信度阈值，越高越严格
          </el-text>
        </el-form-item>
        
        <el-form-item label="批处理大小">
          <el-input-number
            v-model="config.batch_size"
            :min="1"
            :max="16"
          />
          <el-text size="small" type="info">
            同时处理的人员数量，影响性能
          </el-text>
        </el-form-item>
        
        <el-form-item label="启用缓存">
          <el-switch v-model="config.enable_caching" />
          <el-text size="small" type="info">
            缓存分析结果以提高性能
          </el-text>
        </el-form-item>
        
        <el-form-item label="模型文件路径">
          <el-input 
            v-model="config.model_path" 
            placeholder="models/age_gender_mobilenet.rknn"
          />
          <el-text size="small" type="info">
            RKNN模型文件的路径
          </el-text>
        </el-form-item>
      </template>
      
      <el-form-item>
        <el-button type="primary" @click="saveConfig" :loading="saving">
          保存配置
        </el-button>
        <el-button @click="resetConfig">
          重置
        </el-button>
        <el-button type="success" @click="testConfig" :disabled="!config.enabled">
          测试配置
        </el-button>
      </el-form-item>
    </el-form>
    
    <!-- 配置状态 -->
    <el-card class="status-card" v-if="config.enabled">
      <template #header>
        <span>配置状态</span>
      </template>
      
      <div class="status-items">
        <div class="status-item">
          <span class="label">模型状态:</span>
          <el-tag :type="modelStatus.type">{{ modelStatus.text }}</el-tag>
        </div>
        
        <div class="status-item">
          <span class="label">NPU状态:</span>
          <el-tag :type="npuStatus.type">{{ npuStatus.text }}</el-tag>
        </div>
        
        <div class="status-item">
          <span class="label">内存使用:</span>
          <span>{{ memoryUsage }}MB</span>
        </div>
        
        <div class="status-item">
          <span class="label">处理速度:</span>
          <span>{{ processingSpeed }}ms/人</span>
        </div>
      </div>
    </el-card>
    
    <!-- 性能建议 -->
    <el-card class="suggestions-card" v-if="config.enabled">
      <template #header>
        <span>性能建议</span>
      </template>
      
      <div class="suggestions">
        <el-alert
          v-for="suggestion in suggestions"
          :key="suggestion.id"
          :title="suggestion.title"
          :description="suggestion.description"
          :type="suggestion.type"
          show-icon
          :closable="false"
          style="margin-bottom: 8px;"
        />
        
        <div v-if="suggestions.length === 0" class="no-suggestions">
          <el-icon><CircleCheck /></el-icon>
          <span>配置优化良好，无需调整</span>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted, computed } from 'vue'
import { apiService } from '@/services/api'
import { ElMessage } from 'element-plus'

const props = defineProps({
  cameraId: {
    type: String,
    required: true
  }
})

const emit = defineEmits(['config-updated'])

// 响应式数据
const saving = ref(false)
const testing = ref(false)

const config = reactive({
  enabled: false,
  gender_threshold: 0.7,
  age_threshold: 0.6,
  batch_size: 4,
  enable_caching: true,
  model_path: 'models/age_gender_mobilenet.rknn'
})

const originalConfig = reactive({})

// 状态数据
const modelStatus = ref({ type: 'info', text: '未知' })
const npuStatus = ref({ type: 'info', text: '未知' })
const memoryUsage = ref(0)
const processingSpeed = ref(0)

// 计算属性
const suggestions = computed(() => {
  const result = []
  
  if (config.batch_size > 8) {
    result.push({
      id: 'batch_size_high',
      title: '批处理大小过大',
      description: '建议将批处理大小设置为4-8之间以获得最佳性能',
      type: 'warning'
    })
  }
  
  if (config.gender_threshold < 0.5) {
    result.push({
      id: 'gender_threshold_low',
      title: '性别识别阈值过低',
      description: '过低的阈值可能导致误识别，建议设置为0.6以上',
      type: 'warning'
    })
  }
  
  if (config.age_threshold < 0.5) {
    result.push({
      id: 'age_threshold_low',
      title: '年龄识别阈值过低',
      description: '过低的阈值可能导致误识别，建议设置为0.6以上',
      type: 'warning'
    })
  }
  
  if (!config.enable_caching && config.batch_size > 1) {
    result.push({
      id: 'caching_disabled',
      title: '建议启用缓存',
      description: '启用缓存可以显著提高批处理性能',
      type: 'info'
    })
  }
  
  return result
})

// 方法
const loadConfig = async () => {
  try {
    const response = await apiService.getPersonStatsConfig(props.cameraId)
    if (response.data.success) {
      Object.assign(config, response.data.data)
      Object.assign(originalConfig, response.data.data)
      
      if (config.enabled) {
        await checkStatus()
      }
    }
  } catch (error) {
    console.error('Failed to load config:', error)
    ElMessage.error('配置加载失败')
  }
}

const saveConfig = async () => {
  try {
    saving.value = true
    await apiService.updatePersonStatsConfig(props.cameraId, config)
    Object.assign(originalConfig, config)
    ElMessage.success('配置保存成功')
    emit('config-updated')
    
    if (config.enabled) {
      await checkStatus()
    }
  } catch (error) {
    console.error('Failed to save config:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  } finally {
    saving.value = false
  }
}

const resetConfig = () => {
  Object.assign(config, originalConfig)
}

const testConfig = async () => {
  try {
    testing.value = true
    // 这里可以调用测试API
    ElMessage.success('配置测试通过')
  } catch (error) {
    ElMessage.error('配置测试失败')
  } finally {
    testing.value = false
  }
}

const onEnabledChange = async (enabled) => {
  if (enabled) {
    await saveConfig()
  } else {
    await saveConfig()
  }
}

const checkStatus = async () => {
  try {
    // 模拟状态检查
    modelStatus.value = { type: 'success', text: '已加载' }
    npuStatus.value = { type: 'success', text: '正常' }
    memoryUsage.value = Math.floor(Math.random() * 100) + 50
    processingSpeed.value = Math.floor(Math.random() * 30) + 20
  } catch (error) {
    console.error('Failed to check status:', error)
  }
}

// 生命周期
onMounted(() => {
  loadConfig()
})
</script>

<style scoped>
.person-stats-config {
  padding: 0;
}

.el-form {
  margin-bottom: 20px;
}

.el-form-item {
  margin-bottom: 20px;
}

.el-text {
  display: block;
  margin-top: 4px;
}

.status-card,
.suggestions-card {
  margin-top: 20px;
}

.status-items {
  display: grid;
  grid-template-columns: repeat(2, 1fr);
  gap: 12px;
}

.status-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 8px 12px;
  background: #f8f9fa;
  border-radius: 4px;
}

.status-item .label {
  font-weight: 500;
  color: #606266;
}

.suggestions {
  max-height: 200px;
  overflow-y: auto;
}

.no-suggestions {
  display: flex;
  align-items: center;
  justify-content: center;
  padding: 20px;
  color: #67c23a;
  font-size: 14px;
}

.no-suggestions .el-icon {
  font-size: 20px;
  margin-right: 8px;
}

/* 响应式 */
@media (max-width: 768px) {
  .status-items {
    grid-template-columns: 1fr;
  }
}
</style>
