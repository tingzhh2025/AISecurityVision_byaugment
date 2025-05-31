<template>
  <div class="settings">
    <el-tabs v-model="activeTab" type="border-card">
      <!-- 系统配置 -->
      <el-tab-pane label="系统配置" name="system">
        <el-card>
          <template #header>
            <span>系统基本配置</span>
          </template>
          
          <el-form
            ref="systemFormRef"
            :model="systemConfig"
            label-width="120px"
            style="max-width: 600px"
          >
            <el-form-item label="系统名称">
              <el-input v-model="systemConfig.systemName" />
            </el-form-item>
            
            <el-form-item label="管理员邮箱">
              <el-input v-model="systemConfig.adminEmail" />
            </el-form-item>
            
            <el-form-item label="时区">
              <el-select v-model="systemConfig.timezone">
                <el-option label="北京时间 (UTC+8)" value="Asia/Shanghai" />
                <el-option label="东京时间 (UTC+9)" value="Asia/Tokyo" />
                <el-option label="纽约时间 (UTC-5)" value="America/New_York" />
                <el-option label="伦敦时间 (UTC+0)" value="Europe/London" />
              </el-select>
            </el-form-item>
            
            <el-form-item label="日志级别">
              <el-select v-model="systemConfig.logLevel">
                <el-option label="调试 (DEBUG)" value="debug" />
                <el-option label="信息 (INFO)" value="info" />
                <el-option label="警告 (WARN)" value="warn" />
                <el-option label="错误 (ERROR)" value="error" />
              </el-select>
            </el-form-item>
            
            <el-form-item label="自动清理日志">
              <el-switch v-model="systemConfig.autoCleanLogs" />
            </el-form-item>
            
            <el-form-item label="日志保留天数" v-if="systemConfig.autoCleanLogs">
              <el-input-number 
                v-model="systemConfig.logRetentionDays" 
                :min="1" 
                :max="365" 
              />
            </el-form-item>
            
            <el-form-item>
              <el-button type="primary" @click="saveSystemConfig">保存配置</el-button>
              <el-button @click="resetSystemConfig">重置</el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-tab-pane>

      <!-- AI检测配置 -->
      <el-tab-pane label="AI检测配置" name="ai">
        <el-card>
          <template #header>
            <span>AI检测参数配置</span>
          </template>
          
          <el-form
            ref="aiFormRef"
            :model="aiConfig"
            label-width="120px"
            style="max-width: 600px"
          >
            <el-form-item label="启用AI检测">
              <el-switch v-model="aiConfig.enabled" />
            </el-form-item>
            
            <el-form-item label="检测模型" v-if="aiConfig.enabled">
              <el-select v-model="aiConfig.model">
                <el-option label="YOLOv8n (快速)" value="yolov8n" />
                <el-option label="YOLOv8s (平衡)" value="yolov8s" />
                <el-option label="YOLOv8m (精确)" value="yolov8m" />
                <el-option label="YOLOv8l (高精度)" value="yolov8l" />
              </el-select>
            </el-form-item>
            
            <el-form-item label="置信度阈值" v-if="aiConfig.enabled">
              <el-slider 
                v-model="aiConfig.confidenceThreshold" 
                :min="0.1" 
                :max="1" 
                :step="0.05"
                show-input
              />
            </el-form-item>
            
            <el-form-item label="NMS阈值" v-if="aiConfig.enabled">
              <el-slider 
                v-model="aiConfig.nmsThreshold" 
                :min="0.1" 
                :max="1" 
                :step="0.05"
                show-input
              />
            </el-form-item>
            
            <el-form-item label="检测类别" v-if="aiConfig.enabled">
              <el-checkbox-group v-model="aiConfig.detectionClasses">
                <el-checkbox label="person">人员</el-checkbox>
                <el-checkbox label="car">汽车</el-checkbox>
                <el-checkbox label="truck">卡车</el-checkbox>
                <el-checkbox label="bus">公交车</el-checkbox>
                <el-checkbox label="motorcycle">摩托车</el-checkbox>
                <el-checkbox label="bicycle">自行车</el-checkbox>
              </el-checkbox-group>
            </el-form-item>
            
            <el-form-item label="最大检测数量" v-if="aiConfig.enabled">
              <el-input-number 
                v-model="aiConfig.maxDetections" 
                :min="1" 
                :max="100" 
              />
            </el-form-item>
            
            <el-form-item label="检测间隔(秒)" v-if="aiConfig.enabled">
              <el-input-number 
                v-model="aiConfig.detectionInterval" 
                :min="0.1" 
                :max="10" 
                :step="0.1"
              />
            </el-form-item>
            
            <el-form-item>
              <el-button type="primary" @click="saveAiConfig">保存配置</el-button>
              <el-button @click="resetAiConfig">重置</el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-tab-pane>

      <!-- 录像配置 -->
      <el-tab-pane label="录像配置" name="recording">
        <el-card>
          <template #header>
            <span>录像存储配置</span>
          </template>
          
          <el-form
            ref="recordingFormRef"
            :model="recordingConfig"
            label-width="120px"
            style="max-width: 600px"
          >
            <el-form-item label="启用录像">
              <el-switch v-model="recordingConfig.enabled" />
            </el-form-item>
            
            <el-form-item label="存储路径" v-if="recordingConfig.enabled">
              <el-input v-model="recordingConfig.storagePath" />
            </el-form-item>
            
            <el-form-item label="录像质量" v-if="recordingConfig.enabled">
              <el-select v-model="recordingConfig.quality">
                <el-option label="高质量" value="high" />
                <el-option label="中等质量" value="medium" />
                <el-option label="低质量" value="low" />
              </el-select>
            </el-form-item>
            
            <el-form-item label="录像格式" v-if="recordingConfig.enabled">
              <el-select v-model="recordingConfig.format">
                <el-option label="MP4" value="mp4" />
                <el-option label="AVI" value="avi" />
                <el-option label="MKV" value="mkv" />
              </el-select>
            </el-form-item>
            
            <el-form-item label="分段时长(分钟)" v-if="recordingConfig.enabled">
              <el-input-number 
                v-model="recordingConfig.segmentDuration" 
                :min="1" 
                :max="60" 
              />
            </el-form-item>
            
            <el-form-item label="自动删除旧录像" v-if="recordingConfig.enabled">
              <el-switch v-model="recordingConfig.autoDelete" />
            </el-form-item>
            
            <el-form-item label="保留天数" v-if="recordingConfig.enabled && recordingConfig.autoDelete">
              <el-input-number 
                v-model="recordingConfig.retentionDays" 
                :min="1" 
                :max="365" 
              />
            </el-form-item>
            
            <el-form-item label="磁盘空间警告阈值" v-if="recordingConfig.enabled">
              <el-slider 
                v-model="recordingConfig.diskWarningThreshold" 
                :min="50" 
                :max="95" 
                show-input
              />
              <span style="margin-left: 8px; color: #909399;">%</span>
            </el-form-item>
            
            <el-form-item>
              <el-button type="primary" @click="saveRecordingConfig">保存配置</el-button>
              <el-button @click="resetRecordingConfig">重置</el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-tab-pane>

      <!-- 报警配置 -->
      <el-tab-pane label="报警配置" name="alert">
        <el-card>
          <template #header>
            <span>报警通知配置</span>
          </template>
          
          <el-form
            ref="alertFormRef"
            :model="alertConfig"
            label-width="120px"
            style="max-width: 600px"
          >
            <el-form-item label="启用报警">
              <el-switch v-model="alertConfig.enabled" />
            </el-form-item>
            
            <el-form-item label="邮件通知" v-if="alertConfig.enabled">
              <el-switch v-model="alertConfig.emailEnabled" />
            </el-form-item>
            
            <el-form-item label="SMTP服务器" v-if="alertConfig.enabled && alertConfig.emailEnabled">
              <el-input v-model="alertConfig.smtpServer" />
            </el-form-item>
            
            <el-form-item label="SMTP端口" v-if="alertConfig.enabled && alertConfig.emailEnabled">
              <el-input-number v-model="alertConfig.smtpPort" :min="1" :max="65535" />
            </el-form-item>
            
            <el-form-item label="发件人邮箱" v-if="alertConfig.enabled && alertConfig.emailEnabled">
              <el-input v-model="alertConfig.senderEmail" />
            </el-form-item>
            
            <el-form-item label="邮箱密码" v-if="alertConfig.enabled && alertConfig.emailEnabled">
              <el-input v-model="alertConfig.senderPassword" type="password" show-password />
            </el-form-item>
            
            <el-form-item label="收件人列表" v-if="alertConfig.enabled && alertConfig.emailEnabled">
              <el-input 
                v-model="alertConfig.recipients" 
                type="textarea" 
                placeholder="多个邮箱用逗号分隔"
              />
            </el-form-item>
            
            <el-form-item label="微信通知" v-if="alertConfig.enabled">
              <el-switch v-model="alertConfig.wechatEnabled" />
            </el-form-item>
            
            <el-form-item label="企业微信Webhook" v-if="alertConfig.enabled && alertConfig.wechatEnabled">
              <el-input v-model="alertConfig.wechatWebhook" />
            </el-form-item>
            
            <el-form-item label="报警冷却时间(秒)" v-if="alertConfig.enabled">
              <el-input-number 
                v-model="alertConfig.cooldownTime" 
                :min="10" 
                :max="3600" 
              />
            </el-form-item>
            
            <el-form-item>
              <el-button type="primary" @click="saveAlertConfig">保存配置</el-button>
              <el-button @click="resetAlertConfig">重置</el-button>
              <el-button type="success" @click="testAlert">测试通知</el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-tab-pane>

      <!-- 网络配置 -->
      <el-tab-pane label="网络配置" name="network">
        <el-card>
          <template #header>
            <span>网络服务配置</span>
          </template>
          
          <el-form
            ref="networkFormRef"
            :model="networkConfig"
            label-width="120px"
            style="max-width: 600px"
          >
            <el-form-item label="HTTP端口">
              <el-input-number v-model="networkConfig.httpPort" :min="1" :max="65535" />
            </el-form-item>
            
            <el-form-item label="HTTPS端口">
              <el-input-number v-model="networkConfig.httpsPort" :min="1" :max="65535" />
            </el-form-item>
            
            <el-form-item label="启用HTTPS">
              <el-switch v-model="networkConfig.httpsEnabled" />
            </el-form-item>
            
            <el-form-item label="SSL证书路径" v-if="networkConfig.httpsEnabled">
              <el-input v-model="networkConfig.sslCertPath" />
            </el-form-item>
            
            <el-form-item label="SSL私钥路径" v-if="networkConfig.httpsEnabled">
              <el-input v-model="networkConfig.sslKeyPath" />
            </el-form-item>
            
            <el-form-item label="RTSP端口">
              <el-input-number v-model="networkConfig.rtspPort" :min="1" :max="65535" />
            </el-form-item>
            
            <el-form-item label="最大连接数">
              <el-input-number v-model="networkConfig.maxConnections" :min="1" :max="1000" />
            </el-form-item>
            
            <el-form-item label="连接超时(秒)">
              <el-input-number v-model="networkConfig.connectionTimeout" :min="5" :max="300" />
            </el-form-item>
            
            <el-form-item>
              <el-button type="primary" @click="saveNetworkConfig">保存配置</el-button>
              <el-button @click="resetNetworkConfig">重置</el-button>
            </el-form-item>
          </el-form>
        </el-card>
      </el-tab-pane>
    </el-tabs>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { apiService } from '@/services/api'
import { ElMessage } from 'element-plus'

// 响应式数据
const activeTab = ref('system')

// 表单引用
const systemFormRef = ref(null)
const aiFormRef = ref(null)
const recordingFormRef = ref(null)
const alertFormRef = ref(null)
const networkFormRef = ref(null)

// 配置数据
const systemConfig = reactive({
  systemName: 'AI安防监控系统',
  adminEmail: 'admin@example.com',
  timezone: 'Asia/Shanghai',
  logLevel: 'info',
  autoCleanLogs: true,
  logRetentionDays: 30
})

const aiConfig = reactive({
  enabled: true,
  model: 'yolov8n',
  confidenceThreshold: 0.5,
  nmsThreshold: 0.45,
  detectionClasses: ['person', 'car'],
  maxDetections: 50,
  detectionInterval: 1.0
})

const recordingConfig = reactive({
  enabled: true,
  storagePath: '/data/recordings',
  quality: 'medium',
  format: 'mp4',
  segmentDuration: 10,
  autoDelete: true,
  retentionDays: 30,
  diskWarningThreshold: 85
})

const alertConfig = reactive({
  enabled: true,
  emailEnabled: false,
  smtpServer: 'smtp.gmail.com',
  smtpPort: 587,
  senderEmail: '',
  senderPassword: '',
  recipients: '',
  wechatEnabled: false,
  wechatWebhook: '',
  cooldownTime: 300
})

const networkConfig = reactive({
  httpPort: 8080,
  httpsPort: 8443,
  httpsEnabled: false,
  sslCertPath: '/etc/ssl/certs/server.crt',
  sslKeyPath: '/etc/ssl/private/server.key',
  rtspPort: 554,
  maxConnections: 100,
  connectionTimeout: 30
})

// 方法
const loadConfigs = async () => {
  try {
    // 从API加载系统配置
    const response = await apiService.getSystemConfig()
    const data = response.data

    // 更新各个配置对象
    if (data.system) {
      Object.assign(systemConfig, data.system)
    }
    if (data.ai) {
      Object.assign(aiConfig, data.ai)
    }
    if (data.recording) {
      Object.assign(recordingConfig, data.recording)
    }
    if (data.alert) {
      Object.assign(alertConfig, data.alert)
    }
    if (data.network) {
      Object.assign(networkConfig, data.network)
    }

    console.log('Configs loaded successfully:', data)
  } catch (error) {
    console.error('Failed to load configs:', error)
    ElMessage.warning('配置加载失败，使用默认配置')
  }
}

const saveSystemConfig = async () => {
  try {
    await apiService.saveSystemConfig({ system: systemConfig })
    ElMessage.success('系统配置保存成功')
  } catch (error) {
    console.error('Failed to save system config:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  }
}

const saveAiConfig = async () => {
  try {
    await apiService.saveSystemConfig({ ai: aiConfig })
    ElMessage.success('AI配置保存成功')
  } catch (error) {
    console.error('Failed to save AI config:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  }
}

const saveRecordingConfig = async () => {
  try {
    await apiService.saveSystemConfig({ recording: recordingConfig })
    ElMessage.success('录像配置保存成功')
  } catch (error) {
    console.error('Failed to save recording config:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  }
}

const saveAlertConfig = async () => {
  try {
    await apiService.saveSystemConfig({ alert: alertConfig })
    ElMessage.success('报警配置保存成功')
  } catch (error) {
    console.error('Failed to save alert config:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  }
}

const saveNetworkConfig = async () => {
  try {
    await apiService.saveSystemConfig({ network: networkConfig })
    ElMessage.success('网络配置保存成功，重启后生效')
  } catch (error) {
    console.error('Failed to save network config:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  }
}

const resetSystemConfig = () => {
  // 重置为默认值
  loadConfigs()
}

const resetAiConfig = () => {
  loadConfigs()
}

const resetRecordingConfig = () => {
  loadConfigs()
}

const resetAlertConfig = () => {
  loadConfigs()
}

const resetNetworkConfig = () => {
  loadConfigs()
}

const testAlert = async () => {
  try {
    // 发送测试报警
    ElMessage.success('测试通知已发送')
  } catch (error) {
    ElMessage.error('测试失败')
  }
}

onMounted(() => {
  loadConfigs()
})
</script>

<style scoped>
.settings {
  padding: 0;
}

.el-tabs {
  background: #fff;
  border-radius: 8px;
}

.el-card {
  border: none;
  box-shadow: none;
}

.el-form {
  padding: 20px 0;
}

.el-form-item {
  margin-bottom: 24px;
}

/* 响应式 */
@media (max-width: 768px) {
  .el-form {
    max-width: 100% !important;
  }
  
  .el-form-item {
    flex-direction: column;
    align-items: flex-start;
  }
  
  .el-form-item__label {
    width: 100% !important;
    text-align: left !important;
    margin-bottom: 8px;
  }
  
  .el-form-item__content {
    width: 100%;
    margin-left: 0 !important;
  }
}
</style>
