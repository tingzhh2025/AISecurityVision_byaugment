<template>
  <div class="live-monitor">
    <!-- 工具栏 -->
    <div class="toolbar">
      <div class="toolbar-left">
        <el-button-group>
          <el-button
            v-for="layout in layouts"
            :key="layout.value"
            :type="currentLayout === layout.value ? 'primary' : 'default'"
            @click="changeLayout(layout.value)"
          >
            {{ layout.label }}
          </el-button>
        </el-button-group>
      </div>

      <div class="toolbar-right">
        <el-button @click="refreshStreams">
          <el-icon><Refresh /></el-icon>
          刷新
        </el-button>

        <el-button @click="toggleFullscreen">
          <el-icon><FullScreen /></el-icon>
          全屏
        </el-button>
      </div>
    </div>

    <!-- 视频网格 -->
    <div
      ref="videoContainer"
      class="video-grid"
      :class="`layout-${currentLayout}`"
    >
      <div
        v-for="(camera, index) in gridCells"
        :key="camera?.id || `empty-${index}`"
        class="video-cell"
        :class="{
          'active': selectedCamera === camera?.id,
          'empty': !camera
        }"
        @click="selectCamera(camera)"
      >
        <div v-if="camera" class="video-content">
          <!-- 视频流 -->
          <div class="video-stream">
            <img
              v-if="camera.status === 'online'"
              :src="getStreamUrl(camera.id)"
              :alt="camera.name"
              @error="handleStreamError(camera.id)"
              @load="handleStreamLoad(camera.id)"
            />

            <div v-else class="stream-offline">
              <el-icon><VideoCamera /></el-icon>
              <span>摄像头离线</span>
            </div>

            <!-- 加载状态 -->
            <div v-if="loadingStreams.includes(camera.id)" class="stream-loading">
              <el-icon class="is-loading"><Loading /></el-icon>
              <span>加载中...</span>
            </div>
          </div>

          <!-- 摄像头信息覆盖层 -->
          <div class="video-overlay">
            <div class="camera-info">
              <div class="camera-name">{{ camera.name }}</div>
              <div class="camera-status">
                <div class="status-indicator" :class="`status-${camera.status}`">
                  <div class="status-dot"></div>
                  <span>{{ camera.status === 'online' ? '在线' : '离线' }}</span>
                </div>
              </div>
            </div>

            <div class="camera-controls">
              <el-button
                type="primary"
                size="small"
                circle
                @click.stop="openCameraDialog(camera)"
              >
                <el-icon><Setting /></el-icon>
              </el-button>

              <el-button
                type="success"
                size="small"
                circle
                @click.stop="startRecording(camera)"
              >
                <el-icon><VideoPlay /></el-icon>
              </el-button>
            </div>
          </div>

          <!-- AI检测结果覆盖层 -->
          <div v-if="detections[camera.id]" class="detection-overlay">
            <div
              v-for="detection in detections[camera.id]"
              :key="detection.id"
              class="detection-box"
              :style="getDetectionStyle(detection)"
            >
              <div class="detection-label">
                {{ detection.class }} ({{ Math.round(detection.confidence * 100) }}%)
              </div>
            </div>
          </div>
        </div>

        <!-- 空白单元格 -->
        <div v-else class="empty-cell" @click="openAddCameraDialog">
          <el-icon><Plus /></el-icon>
          <span>点击添加摄像头</span>
        </div>
      </div>
    </div>

    <!-- 添加摄像头对话框 -->
    <el-dialog
      v-model="addCameraDialogVisible"
      title="添加摄像头"
      width="600px"
    >
      <el-form
        ref="addCameraForm"
        :model="newCameraData"
        :rules="cameraRules"
        label-width="120px"
      >
        <el-form-item label="摄像头名称" prop="name">
          <el-input
            v-model="newCameraData.name"
            placeholder="请输入摄像头名称"
          />
        </el-form-item>

        <el-form-item label="协议类型" prop="protocol">
          <el-select v-model="newCameraData.protocol" placeholder="请选择协议">
            <el-option label="RTSP" value="rtsp" />
            <el-option label="RTMP" value="rtmp" />
            <el-option label="HTTP" value="http" />
          </el-select>
        </el-form-item>

        <el-form-item label="RTSP地址" prop="url">
          <el-input
            v-model="newCameraData.url"
            placeholder="rtsp://username:password@ip:port/path"
          />
          <div class="form-tip">
            示例: rtsp://admin:password@192.168.1.100:554/stream1
          </div>
        </el-form-item>

        <el-form-item label="用户名">
          <el-input
            v-model="newCameraData.username"
            placeholder="摄像头用户名（可选）"
          />
        </el-form-item>

        <el-form-item label="密码">
          <el-input
            v-model="newCameraData.password"
            type="password"
            placeholder="摄像头密码（可选）"
            show-password
          />
        </el-form-item>

        <el-form-item label="分辨率">
          <el-select v-model="newCameraData.resolution">
            <el-option label="1920x1080 (Full HD)" value="1920x1080" />
            <el-option label="1280x720 (HD)" value="1280x720" />
            <el-option label="640x480 (VGA)" value="640x480" />
          </el-select>
        </el-form-item>

        <el-form-item label="帧率">
          <el-input-number
            v-model="newCameraData.fps"
            :min="1"
            :max="30"
            controls-position="right"
          />
          <span class="form-tip">fps</span>
        </el-form-item>

        <el-form-item label="启用AI检测">
          <el-switch v-model="newCameraData.aiEnabled" />
        </el-form-item>
      </el-form>

      <template #footer>
        <el-button @click="addCameraDialogVisible = false">取消</el-button>
        <el-button @click="testCameraConnection" :loading="testingConnection">
          测试连接
        </el-button>
        <el-button
          type="primary"
          @click="addCamera"
          :loading="addingCamera"
        >
          添加摄像头
        </el-button>
      </template>
    </el-dialog>

    <!-- 摄像头设置对话框 -->
    <el-dialog
      v-model="cameraDialogVisible"
      title="摄像头设置"
      width="600px"
    >
      <div v-if="selectedCameraData" class="camera-dialog-content">
        <el-form :model="selectedCameraData" label-width="100px">
          <el-form-item label="摄像头名称">
            <el-input v-model="selectedCameraData.name" />
          </el-form-item>

          <el-form-item label="RTSP地址">
            <el-input v-model="selectedCameraData.rtspUrl" />
          </el-form-item>

          <el-form-item label="分辨率">
            <el-select v-model="selectedCameraData.resolution">
              <el-option label="1920x1080" value="1920x1080" />
              <el-option label="1280x720" value="1280x720" />
              <el-option label="640x480" value="640x480" />
            </el-select>
          </el-form-item>

          <el-form-item label="帧率">
            <el-input-number
              v-model="selectedCameraData.fps"
              :min="1"
              :max="30"
            />
          </el-form-item>

          <el-form-item label="AI检测">
            <el-switch v-model="selectedCameraData.aiEnabled" />
          </el-form-item>
        </el-form>
      </div>

      <template #footer>
        <el-button @click="cameraDialogVisible = false">取消</el-button>
        <el-button type="primary" @click="saveCameraSettings">保存</el-button>
      </template>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue'
import { useRoute } from 'vue-router'
import { useSystemStore } from '@/stores/system'
import { apiService } from '@/services/api'
import { ElMessage } from 'element-plus'

const route = useRoute()
const systemStore = useSystemStore()

// 响应式数据
const currentLayout = ref('2x2')
const selectedCamera = ref(null)
const loadingStreams = ref([])
const detections = ref({})
const cameraDialogVisible = ref(false)
const selectedCameraData = ref(null)
const videoContainer = ref(null)

// 添加摄像头相关
const addCameraDialogVisible = ref(false)
const addCameraForm = ref(null)
const testingConnection = ref(false)
const addingCamera = ref(false)
const newCameraData = ref({
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
const cameraRules = {
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

// 布局选项
const layouts = [
  { label: '1x1', value: '1x1' },
  { label: '2x2', value: '2x2' },
  { label: '3x3', value: '3x3' },
  { label: '4x4', value: '4x4' }
]

// 计算属性
const cameras = computed(() => systemStore.cameras)

const gridCells = computed(() => {
  const layoutSizes = {
    '1x1': 1,
    '2x2': 4,
    '3x3': 9,
    '4x4': 16
  }

  const maxCameras = layoutSizes[currentLayout.value]
  const cameraList = [...systemStore.cameras]

  // 填充空白单元格
  while (cameraList.length < maxCameras) {
    cameraList.push(null)
  }

  return cameraList.slice(0, maxCameras)
})

// 方法
const changeLayout = (layout) => {
  currentLayout.value = layout
}

const selectCamera = (camera) => {
  if (camera) {
    selectedCamera.value = camera.id
  }
}

const getStreamUrl = (cameraId) => {
  return apiService.getStreamUrl(cameraId)
}

const handleStreamError = (cameraId) => {
  console.error(`Stream error for camera ${cameraId}`)
  const index = loadingStreams.value.indexOf(cameraId)
  if (index > -1) {
    loadingStreams.value.splice(index, 1)
  }
}

const handleStreamLoad = (cameraId) => {
  const index = loadingStreams.value.indexOf(cameraId)
  if (index > -1) {
    loadingStreams.value.splice(index, 1)
  }
}

const refreshStreams = () => {
  loadingStreams.value = systemStore.cameras
    .filter(camera => camera.status === 'online')
    .map(camera => camera.id)

  // 重新加载所有图片
  nextTick(() => {
    const images = videoContainer.value?.querySelectorAll('img')
    images?.forEach(img => {
      const src = img.src
      img.src = ''
      img.src = src
    })
  })
}

const toggleFullscreen = () => {
  if (!document.fullscreenElement) {
    videoContainer.value?.requestFullscreen()
  } else {
    document.exitFullscreen()
  }
}

const openCameraDialog = (camera) => {
  selectedCameraData.value = { ...camera }
  cameraDialogVisible.value = true
}

const saveCameraSettings = async () => {
  try {
    await apiService.updateCamera(selectedCameraData.value.id, selectedCameraData.value)
    ElMessage.success('摄像头设置已保存')
    cameraDialogVisible.value = false
    await systemStore.fetchCameras()
  } catch (error) {
    ElMessage.error('保存失败')
  }
}

const startRecording = async (camera) => {
  try {
    // 这里应该调用录制API
    ElMessage.success(`开始录制 ${camera.name}`)
  } catch (error) {
    ElMessage.error('录制失败')
  }
}

const getDetectionStyle = (detection) => {
  return {
    left: `${detection.x}%`,
    top: `${detection.y}%`,
    width: `${detection.width}%`,
    height: `${detection.height}%`
  }
}

// 添加摄像头相关方法
const openAddCameraDialog = () => {
  // 重置表单数据
  newCameraData.value = {
    name: '',
    protocol: 'rtsp',
    url: '',
    username: '',
    password: '',
    resolution: '1920x1080',
    fps: 25,
    aiEnabled: true
  }
  addCameraDialogVisible.value = true
}

const testCameraConnection = async () => {
  if (!addCameraForm.value) return

  try {
    await addCameraForm.value.validate()
    testingConnection.value = true

    // 调用后端API测试连接
    const response = await apiService.testCameraConnection({
      url: newCameraData.value.url,
      username: newCameraData.value.username,
      password: newCameraData.value.password,
      protocol: newCameraData.value.protocol
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
    testingConnection.value = false
  }
}

const addCamera = async () => {
  if (!addCameraForm.value) return

  try {
    await addCameraForm.value.validate()
    addingCamera.value = true

    // 解析分辨率
    const [width, height] = newCameraData.value.resolution.split('x').map(Number)

    // 构建摄像头数据
    const cameraData = {
      id: `camera_${Date.now()}`,
      name: newCameraData.value.name,
      url: newCameraData.value.url,
      protocol: newCameraData.value.protocol,
      username: newCameraData.value.username,
      password: newCameraData.value.password,
      width,
      height,
      fps: newCameraData.value.fps,
      enabled: true
    }

    // 调用后端API添加摄像头
    await apiService.addCamera(cameraData)

    ElMessage.success('摄像头添加成功！')
    addCameraDialogVisible.value = false

    // 刷新摄像头列表
    await systemStore.fetchCameras()

    // 刷新视频流
    refreshStreams()

  } catch (error) {
    if (error.message) {
      // 表单验证错误
      return
    }
    ElMessage.error('添加摄像头失败，请检查设置')
  } finally {
    addingCamera.value = false
  }
}

// WebSocket连接用于接收AI检测结果
let detectionSocket = null

const connectDetectionSocket = () => {
  const wsUrl = `ws://localhost:8080/ws/detections`
  detectionSocket = new WebSocket(wsUrl)

  detectionSocket.onmessage = (event) => {
    const data = JSON.parse(event.data)
    detections.value[data.cameraId] = data.detections
  }

  detectionSocket.onerror = (error) => {
    console.error('WebSocket error:', error)
  }

  detectionSocket.onclose = () => {
    // 重连逻辑
    setTimeout(connectDetectionSocket, 5000)
  }
}

onMounted(() => {
  // 如果URL中有摄像头参数，选中该摄像头
  if (route.query.camera) {
    selectedCamera.value = route.query.camera
  }

  // 连接WebSocket接收检测结果 (暂时禁用)
  // connectDetectionSocket()

  // 初始加载流
  refreshStreams()
})

onUnmounted(() => {
  if (detectionSocket) {
    detectionSocket.close()
  }
})

// 在 <script setup> 中，所有顶层变量和函数都会自动暴露给模板
</script>

<style scoped>
.live-monitor {
  height: 100%;
  display: flex;
  flex-direction: column;
}

.toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 16px;
  background: #fff;
  border-radius: 8px;
  margin-bottom: 16px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.video-grid {
  flex: 1;
  display: grid;
  gap: 8px;
  padding: 8px;
  background: #f5f5f5;
  border-radius: 8px;
  min-height: 0;
}

.layout-1x1 {
  grid-template-columns: 1fr;
  grid-template-rows: 1fr;
}

.layout-2x2 {
  grid-template-columns: 1fr 1fr;
  grid-template-rows: 1fr 1fr;
}

.layout-3x3 {
  grid-template-columns: repeat(3, 1fr);
  grid-template-rows: repeat(3, 1fr);
}

.layout-4x4 {
  grid-template-columns: repeat(4, 1fr);
  grid-template-rows: repeat(4, 1fr);
}

.video-cell {
  background: #000;
  border-radius: 4px;
  overflow: hidden;
  position: relative;
  cursor: pointer;
  transition: all 0.3s;
  border: 2px solid transparent;
}

.video-cell.active {
  border-color: #409eff;
  box-shadow: 0 0 12px rgba(64, 158, 255, 0.3);
}

.video-cell.empty {
  background: #f0f0f0;
  border: 2px dashed #d0d0d0;
}

.video-content {
  width: 100%;
  height: 100%;
  position: relative;
}

.video-stream {
  width: 100%;
  height: 100%;
  position: relative;
}

.video-stream img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.stream-offline,
.stream-loading {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  background: rgba(0, 0, 0, 0.8);
  color: #fff;
  font-size: 14px;
}

.stream-offline .el-icon,
.stream-loading .el-icon {
  font-size: 32px;
  margin-bottom: 8px;
}

.video-overlay {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  background: linear-gradient(
    to bottom,
    rgba(0, 0, 0, 0.6) 0%,
    transparent 30%,
    transparent 70%,
    rgba(0, 0, 0, 0.6) 100%
  );
  opacity: 0;
  transition: opacity 0.3s;
  display: flex;
  flex-direction: column;
  justify-content: space-between;
  padding: 12px;
}

.video-cell:hover .video-overlay {
  opacity: 1;
}

.camera-info {
  color: #fff;
}

.camera-name {
  font-size: 14px;
  font-weight: 500;
  margin-bottom: 4px;
}

.camera-controls {
  display: flex;
  gap: 8px;
  align-self: flex-end;
}

.detection-overlay {
  position: absolute;
  top: 0;
  left: 0;
  width: 100%;
  height: 100%;
  pointer-events: none;
}

.detection-box {
  position: absolute;
  border: 2px solid #ff4444;
  background: rgba(255, 68, 68, 0.1);
}

.detection-label {
  position: absolute;
  top: -24px;
  left: 0;
  background: #ff4444;
  color: #fff;
  padding: 2px 6px;
  font-size: 12px;
  border-radius: 2px;
  white-space: nowrap;
}

.empty-cell {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  color: #909399;
  font-size: 14px;
}

.empty-cell .el-icon {
  font-size: 32px;
  margin-bottom: 8px;
}

.camera-dialog-content {
  padding: 20px 0;
}

.form-tip {
  font-size: 12px;
  color: #909399;
  margin-top: 4px;
  line-height: 1.4;
}

.el-form-item .form-tip {
  margin-left: 4px;
}

/* 响应式 */
@media (max-width: 768px) {
  .toolbar {
    flex-direction: column;
    gap: 12px;
  }

  .layout-3x3,
  .layout-4x4 {
    grid-template-columns: repeat(2, 1fr);
  }
}
</style>
