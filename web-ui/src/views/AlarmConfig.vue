<template>
  <div class="alarm-config">
    <!-- 页面标题 -->
    <div class="page-header">
      <h2>报警配置管理</h2>
      <p>配置HTTP、WebSocket、MQTT等多种报警推送方式</p>
    </div>

    <!-- 配置列表 -->
    <el-card class="config-card">
      <template #header>
        <div class="card-header">
          <span>报警配置列表</span>
          <div class="header-actions">
            <el-button type="primary" @click="showAddDialog = true">
              <el-icon><Plus /></el-icon>
              添加配置
            </el-button>
            <el-button @click="refreshConfigs">
              <el-icon><Refresh /></el-icon>
              刷新
            </el-button>
          </div>
        </div>
      </template>

      <el-table
        v-loading="loading"
        :data="configs"
        stripe
        style="width: 100%"
      >
        <el-table-column prop="id" label="配置ID" width="150" />
        
        <el-table-column prop="method" label="推送方式" width="120">
          <template #default="{ row }">
            <el-tag :type="getMethodTagType(row.method)">
              {{ getMethodText(row.method) }}
            </el-tag>
          </template>
        </el-table-column>
        
        <el-table-column label="配置详情" min-width="300">
          <template #default="{ row }">
            <div class="config-details">
              <div v-if="row.method === 'http'">
                <strong>URL:</strong> {{ row.url }}<br>
                <strong>超时:</strong> {{ row.timeout_ms }}ms
              </div>
              <div v-else-if="row.method === 'websocket'">
                <strong>端口:</strong> {{ row.port }}<br>
                <strong>心跳间隔:</strong> {{ row.ping_interval_ms }}ms
              </div>
              <div v-else-if="row.method === 'mqtt'">
                <strong>Broker:</strong> {{ row.broker }}<br>
                <strong>Topic:</strong> {{ row.topic }}<br>
                <strong>QoS:</strong> {{ row.qos }}
              </div>
            </div>
          </template>
        </el-table-column>
        
        <el-table-column prop="priority" label="优先级" width="100">
          <template #default="{ row }">
            <el-tag :type="getPriorityTagType(row.priority)">
              {{ row.priority }}
            </el-tag>
          </template>
        </el-table-column>
        
        <el-table-column prop="enabled" label="状态" width="100">
          <template #default="{ row }">
            <el-switch
              v-model="row.enabled"
              @change="toggleConfig(row)"
              :loading="row.updating"
            />
          </template>
        </el-table-column>
        
        <el-table-column label="操作" width="200" fixed="right">
          <template #default="{ row }">
            <el-button
              type="primary"
              size="small"
              @click="testConfig(row)"
              :loading="row.testing"
            >
              <el-icon><VideoPlay /></el-icon>
              测试
            </el-button>
            
            <el-button
              type="warning"
              size="small"
              @click="editConfig(row)"
            >
              <el-icon><Edit /></el-icon>
              编辑
            </el-button>
            
            <el-button
              type="danger"
              size="small"
              @click="deleteConfig(row)"
            >
              <el-icon><Delete /></el-icon>
              删除
            </el-button>
          </template>
        </el-table-column>
      </el-table>
    </el-card>

    <!-- 系统状态 -->
    <el-row :gutter="20" class="status-row">
      <el-col :span="8">
        <el-card class="status-card">
          <div class="status-content">
            <div class="status-icon">
              <el-icon><Bell /></el-icon>
            </div>
            <div class="status-info">
              <div class="status-value">{{ alarmStatus.pending_alarms || 0 }}</div>
              <div class="status-label">待处理报警</div>
            </div>
          </div>
        </el-card>
      </el-col>
      
      <el-col :span="8">
        <el-card class="status-card">
          <div class="status-content">
            <div class="status-icon success">
              <el-icon><Check /></el-icon>
            </div>
            <div class="status-info">
              <div class="status-value">{{ alarmStatus.delivered_alarms || 0 }}</div>
              <div class="status-label">成功推送</div>
            </div>
          </div>
        </el-card>
      </el-col>
      
      <el-col :span="8">
        <el-card class="status-card">
          <div class="status-content">
            <div class="status-icon error">
              <el-icon><Close /></el-icon>
            </div>
            <div class="status-info">
              <div class="status-value">{{ alarmStatus.failed_alarms || 0 }}</div>
              <div class="status-label">推送失败</div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 添加/编辑配置对话框 -->
    <el-dialog
      v-model="showAddDialog"
      :title="editingConfig ? '编辑报警配置' : '添加报警配置'"
      width="600px"
      @close="resetForm"
    >
      <el-form
        ref="configFormRef"
        :model="configForm"
        :rules="configRules"
        label-width="120px"
      >
        <el-form-item label="配置ID" prop="id">
          <el-input
            v-model="configForm.id"
            placeholder="请输入唯一的配置ID"
            :disabled="editingConfig"
          />
        </el-form-item>
        
        <el-form-item label="推送方式" prop="method">
          <el-select
            v-model="configForm.method"
            placeholder="选择推送方式"
            @change="onMethodChange"
            style="width: 100%"
          >
            <el-option label="HTTP POST" value="http" />
            <el-option label="WebSocket" value="websocket" />
            <el-option label="MQTT" value="mqtt" />
          </el-select>
        </el-form-item>
        
        <el-form-item label="优先级" prop="priority">
          <el-select v-model="configForm.priority" style="width: 100%">
            <el-option label="1 - 最低" :value="1" />
            <el-option label="2 - 低" :value="2" />
            <el-option label="3 - 中等" :value="3" />
            <el-option label="4 - 高" :value="4" />
            <el-option label="5 - 最高" :value="5" />
          </el-select>
        </el-form-item>

        <!-- HTTP 配置 -->
        <template v-if="configForm.method === 'http'">
          <el-form-item label="URL" prop="url">
            <el-input
              v-model="configForm.url"
              placeholder="https://your-server.com/webhook"
            />
          </el-form-item>
          
          <el-form-item label="超时时间(ms)" prop="timeout_ms">
            <el-input-number
              v-model="configForm.timeout_ms"
              :min="1000"
              :max="30000"
              :step="1000"
              style="width: 100%"
            />
          </el-form-item>
          
          <el-form-item label="请求头">
            <el-input
              v-model="configForm.headers"
              type="textarea"
              :rows="3"
              placeholder='{"Authorization": "Bearer token", "Content-Type": "application/json"}'
            />
          </el-form-item>
        </template>

        <!-- WebSocket 配置 -->
        <template v-if="configForm.method === 'websocket'">
          <el-form-item label="端口" prop="port">
            <el-input-number
              v-model="configForm.port"
              :min="1024"
              :max="65535"
              style="width: 100%"
            />
          </el-form-item>
          
          <el-form-item label="心跳间隔(ms)">
            <el-input-number
              v-model="configForm.ping_interval_ms"
              :min="5000"
              :max="60000"
              :step="5000"
              style="width: 100%"
            />
          </el-form-item>
        </template>

        <!-- MQTT 配置 -->
        <template v-if="configForm.method === 'mqtt'">
          <el-form-item label="Broker地址" prop="broker">
            <el-input
              v-model="configForm.broker"
              placeholder="mqtt.example.com"
            />
          </el-form-item>
          
          <el-form-item label="端口" prop="port">
            <el-input-number
              v-model="configForm.port"
              :min="1"
              :max="65535"
              style="width: 100%"
            />
          </el-form-item>
          
          <el-form-item label="Topic" prop="topic">
            <el-input
              v-model="configForm.topic"
              placeholder="aibox/alarms"
            />
          </el-form-item>
          
          <el-form-item label="QoS">
            <el-select v-model="configForm.qos" style="width: 100%">
              <el-option label="0 - 最多一次" :value="0" />
              <el-option label="1 - 至少一次" :value="1" />
              <el-option label="2 - 恰好一次" :value="2" />
            </el-select>
          </el-form-item>
          
          <el-form-item label="用户名">
            <el-input v-model="configForm.username" />
          </el-form-item>
          
          <el-form-item label="密码">
            <el-input
              v-model="configForm.password"
              type="password"
              show-password
            />
          </el-form-item>
          
          <el-form-item label="保活时间(秒)">
            <el-input-number
              v-model="configForm.keep_alive_seconds"
              :min="10"
              :max="300"
              style="width: 100%"
            />
          </el-form-item>
        </template>
      </el-form>
      
      <template #footer>
        <el-button @click="showAddDialog = false">取消</el-button>
        <el-button
          type="primary"
          @click="saveConfig"
          :loading="saving"
        >
          保存
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { apiService } from '@/services/api'
import { ElMessage, ElMessageBox } from 'element-plus'

// 响应式数据
const loading = ref(false)
const saving = ref(false)
const configs = ref([])
const alarmStatus = ref({})
const showAddDialog = ref(false)
const editingConfig = ref(null)
const configFormRef = ref()

// 配置表单
const configForm = reactive({
  id: '',
  method: 'http',
  priority: 1,
  url: '',
  timeout_ms: 5000,
  headers: '',
  port: 8081,
  ping_interval_ms: 30000,
  broker: '',
  topic: 'aibox/alarms',
  qos: 1,
  username: '',
  password: '',
  keep_alive_seconds: 60
})

// 表单验证规则
const configRules = {
  id: [
    { required: true, message: '请输入配置ID', trigger: 'blur' },
    { min: 3, max: 50, message: '长度在 3 到 50 个字符', trigger: 'blur' }
  ],
  method: [
    { required: true, message: '请选择推送方式', trigger: 'change' }
  ],
  url: [
    { required: true, message: '请输入URL', trigger: 'blur' },
    { type: 'url', message: '请输入有效的URL', trigger: 'blur' }
  ],
  broker: [
    { required: true, message: '请输入Broker地址', trigger: 'blur' }
  ],
  topic: [
    { required: true, message: '请输入Topic', trigger: 'blur' }
  ],
  port: [
    { required: true, message: '请输入端口', trigger: 'blur' }
  ]
}

// 方法
const refreshConfigs = async () => {
  loading.value = true
  try {
    const response = await apiService.getAlarmConfigs()
    configs.value = response.data.configs || []

    // 获取系统状态
    const statusResponse = await apiService.getAlarmStatus()
    alarmStatus.value = statusResponse.data
  } catch (error) {
    ElMessage.error('获取配置失败: ' + (error.response?.data?.message || error.message))
  } finally {
    loading.value = false
  }
}

const getMethodTagType = (method) => {
  const types = {
    'http': 'primary',
    'websocket': 'success',
    'mqtt': 'warning'
  }
  return types[method] || 'info'
}

const getMethodText = (method) => {
  const texts = {
    'http': 'HTTP',
    'websocket': 'WebSocket',
    'mqtt': 'MQTT'
  }
  return texts[method] || method
}

const getPriorityTagType = (priority) => {
  if (priority >= 4) return 'danger'
  if (priority >= 3) return 'warning'
  if (priority >= 2) return 'primary'
  return 'info'
}

const onMethodChange = () => {
  // 重置相关字段
  if (configForm.method === 'http') {
    configForm.port = undefined
    configForm.broker = ''
    configForm.topic = 'aibox/alarms'
  } else if (configForm.method === 'websocket') {
    configForm.url = ''
    configForm.broker = ''
    configForm.topic = 'aibox/alarms'
    configForm.port = 8081
  } else if (configForm.method === 'mqtt') {
    configForm.url = ''
    configForm.port = 1883
    configForm.topic = 'aibox/alarms'
  }
}

const resetForm = () => {
  Object.assign(configForm, {
    id: '',
    method: 'http',
    priority: 1,
    url: '',
    timeout_ms: 5000,
    headers: '',
    port: 8081,
    ping_interval_ms: 30000,
    broker: '',
    topic: 'aibox/alarms',
    qos: 1,
    username: '',
    password: '',
    keep_alive_seconds: 60
  })
  editingConfig.value = null
  configFormRef.value?.clearValidate()
}

const saveConfig = async () => {
  try {
    await configFormRef.value.validate()

    saving.value = true

    const configData = { ...configForm }

    // 处理headers字段
    if (configData.method === 'http' && configData.headers) {
      try {
        JSON.parse(configData.headers)
      } catch (e) {
        ElMessage.error('请求头格式错误，请输入有效的JSON')
        return
      }
    }

    await apiService.saveAlarmConfig(configData)

    ElMessage.success('配置保存成功')
    showAddDialog.value = false
    resetForm()
    refreshConfigs()
  } catch (error) {
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  } finally {
    saving.value = false
  }
}

const editConfig = (config) => {
  editingConfig.value = config
  Object.assign(configForm, config)
  showAddDialog.value = true
}

const deleteConfig = async (config) => {
  try {
    await ElMessageBox.confirm(
      `确定要删除配置 "${config.id}" 吗？`,
      '确认删除',
      {
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        type: 'warning'
      }
    )

    await apiService.deleteAlarmConfig(config.id)
    ElMessage.success('删除成功')
    refreshConfigs()
  } catch (error) {
    if (error !== 'cancel') {
      ElMessage.error('删除失败: ' + (error.response?.data?.message || error.message))
    }
  }
}

const toggleConfig = async (config) => {
  config.updating = true
  try {
    await apiService.updateAlarmConfig(config.id, { enabled: config.enabled })
    ElMessage.success(config.enabled ? '配置已启用' : '配置已禁用')
  } catch (error) {
    config.enabled = !config.enabled // 回滚状态
    ElMessage.error('操作失败: ' + (error.response?.data?.message || error.message))
  } finally {
    config.updating = false
  }
}

const testConfig = async (config) => {
  config.testing = true
  try {
    await apiService.testAlarm({
      event_type: 'test_alarm',
      camera_id: 'test_camera',
      config_id: config.id
    })
    ElMessage.success('测试报警已发送')
  } catch (error) {
    ElMessage.error('测试失败: ' + (error.response?.data?.message || error.message))
  } finally {
    config.testing = false
  }
}

// 生命周期
onMounted(() => {
  refreshConfigs()

  // 定时刷新状态
  setInterval(async () => {
    try {
      const response = await apiService.getAlarmStatus()
      alarmStatus.value = response.data
    } catch (error) {
      // 静默失败
    }
  }, 5000)
})
</script>

<style scoped>
.alarm-config {
  padding: 20px;
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

.config-card {
  margin-bottom: 20px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-actions {
  display: flex;
  gap: 8px;
}

.config-details {
  font-size: 13px;
  line-height: 1.6;
}

.config-details strong {
  color: #303133;
}

.status-row {
  margin-top: 20px;
}

.status-card {
  height: 100px;
}

.status-content {
  display: flex;
  align-items: center;
  height: 100%;
}

.status-icon {
  width: 48px;
  height: 48px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  background: #409eff;
  color: white;
  font-size: 24px;
  margin-right: 16px;
}

.status-icon.success {
  background: #67c23a;
}

.status-icon.error {
  background: #f56c6c;
}

.status-icon.info {
  background: #909399;
}

.status-info {
  flex: 1;
}

.status-value {
  font-size: 24px;
  font-weight: bold;
  color: #303133;
  line-height: 1;
  margin-bottom: 4px;
}

.status-label {
  font-size: 14px;
  color: #909399;
}

.el-dialog .el-form {
  padding: 0 20px;
}

.el-form-item {
  margin-bottom: 20px;
}

.el-input-number {
  width: 100%;
}

@media (max-width: 768px) {
  .alarm-config {
    padding: 10px;
  }

  .status-row .el-col {
    margin-bottom: 10px;
  }

  .card-header {
    flex-direction: column;
    gap: 10px;
    align-items: flex-start;
  }

  .header-actions {
    width: 100%;
    justify-content: flex-end;
  }
}
</style>
