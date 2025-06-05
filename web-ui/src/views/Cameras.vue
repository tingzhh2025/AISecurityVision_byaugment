<template>
  <div class="cameras">
    <!-- 工具栏 -->
    <el-card class="toolbar-card">
      <div class="toolbar">
        <div class="toolbar-left">
          <el-button type="primary" @click="addCamera">
            <el-icon><Plus /></el-icon>
            添加摄像头
          </el-button>
          
          <el-button @click="refreshCameras">
            <el-icon><Refresh /></el-icon>
            刷新
          </el-button>
        </div>
        
        <div class="toolbar-right">
          <el-input
            v-model="searchKeyword"
            placeholder="搜索摄像头名称或IP"
            style="width: 200px"
            clearable
            @input="filterCameras"
          >
            <template #prefix>
              <el-icon><Search /></el-icon>
            </template>
          </el-input>
        </div>
      </div>
    </el-card>

    <!-- 摄像头网格 -->
    <div class="cameras-grid">
      <div 
        v-for="camera in filteredCameras" 
        :key="camera.id"
        class="camera-card"
      >
        <el-card>
          <!-- 摄像头预览 -->
          <div class="camera-preview">
            <img 
              v-if="camera.status === 'online'"
              :src="getStreamUrl(camera.id)"
              :alt="camera.name"
              @error="handleImageError"
            />
            
            <div v-else class="camera-offline">
              <el-icon><VideoCamera /></el-icon>
              <span>摄像头离线</span>
            </div>
            
            <!-- 状态覆盖层 -->
            <div class="camera-overlay">
              <div class="status-indicator" :class="`status-${camera.status}`">
                <div class="status-dot"></div>
                <span>{{ camera.status === 'online' ? '在线' : '离线' }}</span>
              </div>
            </div>
          </div>
          
          <!-- 摄像头信息 -->
          <div class="camera-info">
            <h3 class="camera-name">{{ camera.name }}</h3>
            
            <div class="camera-details">
              <div class="detail-item">
                <span class="label">IP地址:</span>
                <span class="value">{{ camera.ip }}</span>
              </div>
              
              <div class="detail-item">
                <span class="label">分辨率:</span>
                <span class="value">{{ camera.resolution }}</span>
              </div>
              
              <div class="detail-item">
                <span class="label">帧率:</span>
                <span class="value">{{ camera.fps }}fps</span>
              </div>
              
              <div class="detail-item">
                <span class="label">AI检测:</span>
                <el-tag :type="camera.aiEnabled ? 'success' : 'info'" size="small">
                  {{ camera.aiEnabled ? '启用' : '禁用' }}
                </el-tag>
              </div>
            </div>
          </div>
          
          <!-- 操作按钮 -->
          <div class="camera-actions">
            <el-button 
              type="primary" 
              size="small"
              @click="viewLive(camera)"
            >
              <el-icon><VideoPlay /></el-icon>
              实时查看
            </el-button>
            
            <el-button 
              type="success" 
              size="small"
              @click="testCamera(camera)"
              :loading="testingCameras.includes(camera.id)"
            >
              <el-icon><Connection /></el-icon>
              测试连接
            </el-button>
            
            <el-button 
              type="warning" 
              size="small"
              @click="editCamera(camera)"
            >
              <el-icon><Edit /></el-icon>
              编辑
            </el-button>
            
            <el-button 
              type="danger" 
              size="small"
              @click="deleteCamera(camera)"
            >
              <el-icon><Delete /></el-icon>
              删除
            </el-button>
          </div>
        </el-card>
      </div>
    </div>

    <!-- 摄像头配置对话框 -->
    <el-dialog
      v-model="dialogVisible"
      :title="isEditing ? '编辑摄像头' : '添加摄像头'"
      width="600px"
      @close="resetForm"
    >
      <el-form
        ref="formRef"
        :model="cameraForm"
        :rules="formRules"
        label-width="100px"
      >
        <el-form-item label="摄像头名称" prop="name">
          <el-input v-model="cameraForm.name" placeholder="请输入摄像头名称" />
        </el-form-item>
        
        <el-form-item label="RTSP地址" prop="rtspUrl">
          <el-input 
            v-model="cameraForm.rtspUrl" 
            placeholder="rtsp://username:password@ip:port/path"
          />
        </el-form-item>
        
        <el-form-item label="IP地址" prop="ip">
          <el-input v-model="cameraForm.ip" placeholder="192.168.1.100" />
        </el-form-item>
        
        <el-form-item label="端口" prop="port">
          <el-input-number 
            v-model="cameraForm.port" 
            :min="1" 
            :max="65535" 
            placeholder="554"
          />
        </el-form-item>
        
        <el-form-item label="用户名">
          <el-input v-model="cameraForm.username" placeholder="用户名" />
        </el-form-item>
        
        <el-form-item label="密码">
          <el-input 
            v-model="cameraForm.password" 
            type="password" 
            placeholder="密码"
            show-password
          />
        </el-form-item>
        
        <el-form-item label="分辨率" prop="resolution">
          <el-select v-model="cameraForm.resolution" placeholder="选择分辨率">
            <el-option label="1920x1080" value="1920x1080" />
            <el-option label="1280x720" value="1280x720" />
            <el-option label="640x480" value="640x480" />
          </el-select>
        </el-form-item>
        
        <el-form-item label="帧率" prop="fps">
          <el-input-number 
            v-model="cameraForm.fps" 
            :min="1" 
            :max="30" 
            placeholder="25"
          />
        </el-form-item>
        
        <el-form-item label="启用AI检测">
          <el-switch v-model="cameraForm.aiEnabled" />
        </el-form-item>
        
        <el-form-item label="检测类型" v-if="cameraForm.aiEnabled">
          <el-checkbox-group v-model="cameraForm.detectionTypes">
            <el-checkbox label="person">人员检测</el-checkbox>
            <el-checkbox label="vehicle">车辆检测</el-checkbox>
            <el-checkbox label="intrusion">入侵检测</el-checkbox>
            <el-checkbox label="loitering">徘徊检测</el-checkbox>
          </el-checkbox-group>
        </el-form-item>
        
        <el-form-item label="检测区域" v-if="cameraForm.aiEnabled">
          <el-input 
            v-model="cameraForm.detectionRegion" 
            type="textarea" 
            placeholder="检测区域坐标 (可选)"
          />
        </el-form-item>
        
        <el-form-item label="描述">
          <el-input 
            v-model="cameraForm.description" 
            type="textarea" 
            placeholder="摄像头描述信息"
          />
        </el-form-item>
      </el-form>
      
      <template #footer>
        <el-button @click="dialogVisible = false">取消</el-button>
        <el-button 
          type="primary" 
          @click="saveCamera"
          :loading="saving"
        >
          {{ isEditing ? '更新' : '添加' }}
        </el-button>
        <el-button 
          type="success" 
          @click="testCameraConnection"
          :loading="testing"
        >
          测试连接
        </el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { useSystemStore } from '@/stores/system'
import { apiService } from '@/services/api'
import { ElMessage, ElMessageBox } from 'element-plus'

const router = useRouter()
const systemStore = useSystemStore()

// 响应式数据
const searchKeyword = ref('')
const dialogVisible = ref(false)
const isEditing = ref(false)
const saving = ref(false)
const testing = ref(false)
const testingCameras = ref([])
const formRef = ref(null)

// 摄像头表单
const cameraForm = reactive({
  id: null,
  name: '',
  rtspUrl: '',
  ip: '',
  port: 554,
  username: '',
  password: '',
  resolution: '1920x1080',
  fps: 25,
  aiEnabled: false,
  detectionTypes: [],
  detectionRegion: '',
  description: ''
})

// 表单验证规则
const formRules = {
  name: [
    { required: true, message: '请输入摄像头名称', trigger: 'blur' }
  ],
  rtspUrl: [
    { required: true, message: '请输入RTSP地址', trigger: 'blur' }
  ],
  ip: [
    { required: true, message: '请输入IP地址', trigger: 'blur' },
    { 
      pattern: /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/,
      message: '请输入有效的IP地址',
      trigger: 'blur'
    }
  ],
  port: [
    { required: true, message: '请输入端口号', trigger: 'blur' }
  ],
  resolution: [
    { required: true, message: '请选择分辨率', trigger: 'change' }
  ],
  fps: [
    { required: true, message: '请输入帧率', trigger: 'blur' }
  ]
}

// 计算属性
const filteredCameras = computed(() => {
  if (!searchKeyword.value) {
    return systemStore.cameras
  }
  
  return systemStore.cameras.filter(camera => 
    camera.name.toLowerCase().includes(searchKeyword.value.toLowerCase()) ||
    camera.ip.includes(searchKeyword.value)
  )
})

// 方法
const refreshCameras = async () => {
  await systemStore.fetchCameras()
  ElMessage.success('摄像头列表已刷新')
}

const filterCameras = () => {
  // 搜索功能通过计算属性实现
}

const addCamera = () => {
  isEditing.value = false
  dialogVisible.value = true
}

const editCamera = (camera) => {
  isEditing.value = true
  
  // 填充表单数据
  Object.keys(cameraForm).forEach(key => {
    if (camera.hasOwnProperty(key)) {
      cameraForm[key] = camera[key]
    }
  })
  
  dialogVisible.value = true
}

const saveCamera = async () => {
  if (!formRef.value) return

  try {
    await formRef.value.validate()
    saving.value = true

    // 构建摄像头配置对象
    const cameraConfig = {
      camera_id: cameraForm.id || `camera_${Date.now()}`,
      name: cameraForm.name,
      url: cameraForm.rtspUrl,
      protocol: 'rtsp',
      username: cameraForm.username,
      password: cameraForm.password,
      width: parseInt(cameraForm.resolution.split('x')[0]),
      height: parseInt(cameraForm.resolution.split('x')[1]),
      fps: cameraForm.fps,
      mjpeg_port: 8161 + (systemStore.cameras.length), // 自动分配端口
      enabled: true,
      detection_enabled: cameraForm.aiEnabled,
      recording_enabled: false,
      detection_config: {
        confidence_threshold: 0.5,
        nms_threshold: 0.4,
        backend: 'RKNN',
        model_path: 'models/yolov8n.rknn'
      },
      stream_config: {
        fps: cameraForm.fps,
        quality: 80,
        max_width: parseInt(cameraForm.resolution.split('x')[0]),
        max_height: parseInt(cameraForm.resolution.split('x')[1])
      }
    }

    if (isEditing.value) {
      // 更新摄像头
      await apiService.updateCamera(cameraForm.id, cameraForm)
      // 同时保存到配置数据库
      await apiService.saveCameraConfig(cameraConfig)
      ElMessage.success('摄像头更新成功')
    } else {
      // 添加摄像头 - 只调用一个API，后端会处理所有逻辑
      await apiService.addCamera(cameraForm)
      ElMessage.success('摄像头添加成功')
    }

    dialogVisible.value = false
    await systemStore.fetchCameras()
  } catch (error) {
    if (error !== false) { // 不是表单验证错误
      console.error('Failed to save camera:', error)
      ElMessage.error(isEditing.value ? '更新失败' : '添加失败')
    }
  } finally {
    saving.value = false
  }
}

const deleteCamera = async (camera) => {
  try {
    await ElMessageBox.confirm(
      `确定要删除摄像头 "${camera.name}" 吗？`,
      '确认删除',
      {
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        type: 'warning'
      }
    )

    // 从API删除摄像头
    await apiService.deleteCamera(camera.id)
    // 同时从配置数据库删除
    await apiService.deleteCameraConfig(camera.id)

    ElMessage.success('删除成功')
    await systemStore.fetchCameras()
  } catch (error) {
    if (error !== 'cancel') {
      console.error('Failed to delete camera:', error)
      ElMessage.error('删除失败')
    }
  }
}

const testCamera = async (camera) => {
  testingCameras.value.push(camera.id)
  
  try {
    await apiService.testCamera(camera)
    ElMessage.success(`摄像头 "${camera.name}" 连接正常`)
  } catch (error) {
    ElMessage.error(`摄像头 "${camera.name}" 连接失败`)
  } finally {
    const index = testingCameras.value.indexOf(camera.id)
    if (index > -1) {
      testingCameras.value.splice(index, 1)
    }
  }
}

const testCameraConnection = async () => {
  testing.value = true
  
  try {
    await apiService.testCamera(cameraForm)
    ElMessage.success('连接测试成功')
  } catch (error) {
    ElMessage.error('连接测试失败')
  } finally {
    testing.value = false
  }
}

const viewLive = (camera) => {
  router.push(`/live?camera=${camera.id}`)
}

const getStreamUrl = (cameraId) => {
  return apiService.getStreamUrl(cameraId)
}

const handleImageError = (event) => {
  event.target.style.display = 'none'
}

const resetForm = () => {
  Object.keys(cameraForm).forEach(key => {
    if (key === 'port') {
      cameraForm[key] = 554
    } else if (key === 'resolution') {
      cameraForm[key] = '1920x1080'
    } else if (key === 'fps') {
      cameraForm[key] = 25
    } else if (key === 'aiEnabled') {
      cameraForm[key] = false
    } else if (key === 'detectionTypes') {
      cameraForm[key] = []
    } else {
      cameraForm[key] = ''
    }
  })
  
  if (formRef.value) {
    formRef.value.clearValidate()
  }
}

onMounted(() => {
  refreshCameras()
})
</script>

<style scoped>
.cameras {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.toolbar-card {
  margin-bottom: 16px;
}

.toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.toolbar-left {
  display: flex;
  gap: 12px;
}

.cameras-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(350px, 1fr));
  gap: 20px;
}

.camera-card {
  transition: transform 0.3s, box-shadow 0.3s;
}

.camera-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 24px rgba(0, 0, 0, 0.12);
}

.camera-preview {
  height: 200px;
  background: #000;
  border-radius: 8px;
  overflow: hidden;
  position: relative;
  margin-bottom: 16px;
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
  font-size: 48px;
  margin-bottom: 8px;
}

.camera-overlay {
  position: absolute;
  top: 12px;
  right: 12px;
}

.camera-info {
  margin-bottom: 16px;
}

.camera-name {
  font-size: 18px;
  font-weight: 600;
  color: #303133;
  margin: 0 0 12px 0;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.camera-details {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.detail-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 14px;
}

.label {
  color: #909399;
  font-weight: 500;
}

.value {
  color: #606266;
}

.camera-actions {
  display: flex;
  flex-wrap: wrap;
  gap: 8px;
}

.camera-actions .el-button {
  flex: 1;
  min-width: 0;
}

/* 响应式 */
@media (max-width: 768px) {
  .cameras-grid {
    grid-template-columns: 1fr;
  }
  
  .toolbar {
    flex-direction: column;
    gap: 12px;
  }
  
  .toolbar-left,
  .toolbar-right {
    width: 100%;
  }
  
  .toolbar-right .el-input {
    width: 100% !important;
  }
}
</style>
