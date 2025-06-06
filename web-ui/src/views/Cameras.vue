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
              v-if="camera.status === 'online' && camera.streamUrl"
              :src="camera.streamUrl"
              :alt="camera.name"
              @error="handleImageError"
            />

            <div v-else-if="camera.status === 'online'" class="camera-loading">
              <el-icon><Loading /></el-icon>
              <span>加载中...</span>
            </div>

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
        label-width="120px"
      >
        <el-form-item label="摄像头名称" prop="name">
          <el-input
            v-model="cameraForm.name"
            placeholder="请输入摄像头名称"
          />
        </el-form-item>

        <el-form-item label="协议类型" prop="protocol">
          <el-select v-model="cameraForm.protocol" placeholder="请选择协议">
            <el-option label="RTSP" value="rtsp" />
            <el-option label="RTMP" value="rtmp" />
            <el-option label="HTTP" value="http" />
          </el-select>
        </el-form-item>

        <el-form-item label="RTSP地址" prop="url">
          <el-input
            v-model="cameraForm.url"
            placeholder="rtsp://username:password@ip:port/path"
          />
          <div class="form-tip">
            示例: rtsp://admin:password@192.168.1.100:554/stream1
          </div>
        </el-form-item>

        <el-form-item label="用户名">
          <el-input
            v-model="cameraForm.username"
            placeholder="摄像头用户名（可选）"
          />
        </el-form-item>

        <el-form-item label="密码">
          <el-input
            v-model="cameraForm.password"
            type="password"
            placeholder="摄像头密码（可选）"
            show-password
          />
        </el-form-item>

        <el-form-item label="分辨率">
          <el-select v-model="cameraForm.resolution">
            <el-option label="1920x1080 (Full HD)" value="1920x1080" />
            <el-option label="1280x720 (HD)" value="1280x720" />
            <el-option label="640x480 (VGA)" value="640x480" />
          </el-select>
        </el-form-item>

        <el-form-item label="帧率">
          <el-input-number
            v-model="cameraForm.fps"
            :min="1"
            :max="30"
            controls-position="right"
          />
          <span class="form-tip">fps</span>
        </el-form-item>

        <el-form-item label="启用AI检测">
          <el-switch v-model="cameraForm.aiEnabled" />
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
  protocol: 'rtsp',
  url: '',
  username: '',
  password: '',
  resolution: '1920x1080',
  fps: 25,
  aiEnabled: true
})

// 表单验证规则
const formRules = {
  name: [
    { required: true, message: '请输入摄像头名称', trigger: 'blur' },
    { min: 2, max: 50, message: '长度在 2 到 50 个字符', trigger: 'blur' }
  ],
  protocol: [
    { required: true, message: '请选择协议类型', trigger: 'change' }
  ],
  url: [
    { required: true, message: '请输入RTSP地址', trigger: 'blur' },
    {
      pattern: /^(rtsp|rtmp|http):\/\/.+/,
      message: '请输入有效的URL地址',
      trigger: 'blur'
    }
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
  // 为每个在线摄像头获取流URL
  await loadStreamUrls()
  ElMessage.success('摄像头列表已刷新')
}

const loadStreamUrls = async () => {
  for (const camera of systemStore.cameras) {
    // 添加计算属性
    if (!camera.resolution) {
      camera.resolution = `${camera.width || 1920}x${camera.height || 1080}`
    }
    if (!camera.aiEnabled) {
      camera.aiEnabled = camera.enabled || false
    }

    if (camera.status === 'online') {
      try {
        camera.streamUrl = await apiService.getStreamUrl(camera.id)
        console.log(`[Cameras] Stream URL for ${camera.id}: ${camera.streamUrl}`)
      } catch (error) {
        console.error(`Failed to get stream URL for camera ${camera.id}:`, error)
      }
    }
  }
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

    // 解析分辨率
    const [width, height] = cameraForm.resolution.split('x').map(Number)

    // 构建摄像头数据
    const cameraData = {
      id: cameraForm.id || `camera_${Date.now()}`,
      name: cameraForm.name,
      url: cameraForm.url,
      protocol: cameraForm.protocol,
      username: cameraForm.username,
      password: cameraForm.password,
      width,
      height,
      fps: cameraForm.fps,
      enabled: true
    }

    if (isEditing.value) {
      // 更新摄像头
      await apiService.updateCamera(cameraForm.id, cameraData)
      ElMessage.success('摄像头更新成功')
    } else {
      // 添加摄像头
      await apiService.addCamera(cameraData)
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
  if (!formRef.value) return

  try {
    await formRef.value.validate()
    testing.value = true

    // 调用后端API测试连接
    const response = await apiService.testCameraConnection({
      url: cameraForm.url,
      username: cameraForm.username,
      password: cameraForm.password,
      protocol: cameraForm.protocol
    })

    if (response.success) {
      ElMessage.success('摄像头连接测试成功！')
    } else {
      ElMessage.error(`连接测试失败: ${response.message}`)
    }
  } catch (error) {
    if (error.message) {
      // 表单验证错误
      return
    }
    ElMessage.error('连接测试失败，请检查网络和摄像头设置')
  } finally {
    testing.value = false
  }
}

const viewLive = (camera) => {
  router.push(`/live?camera=${camera.id}`)
}

const handleImageError = (event) => {
  event.target.style.display = 'none'
}

const resetForm = () => {
  Object.keys(cameraForm).forEach(key => {
    if (key === 'protocol') {
      cameraForm[key] = 'rtsp'
    } else if (key === 'resolution') {
      cameraForm[key] = '1920x1080'
    } else if (key === 'fps') {
      cameraForm[key] = 25
    } else if (key === 'aiEnabled') {
      cameraForm[key] = true
    } else {
      cameraForm[key] = ''
    }
  })

  if (formRef.value) {
    formRef.value.clearValidate()
  }
}

onMounted(async () => {
  await systemStore.fetchCameras()
  await loadStreamUrls()
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

.camera-offline,
.camera-loading {
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #909399;
  font-size: 14px;
}

.camera-offline .el-icon,
.camera-loading .el-icon {
  font-size: 48px;
  margin-bottom: 8px;
}

.camera-loading .el-icon {
  animation: rotate 2s linear infinite;
}

@keyframes rotate {
  from {
    transform: rotate(0deg);
  }
  to {
    transform: rotate(360deg);
  }
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

.form-tip {
  font-size: 12px;
  color: #909399;
  margin-top: 4px;
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
