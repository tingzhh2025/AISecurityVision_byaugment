<template>
  <div class="playback">
    <!-- 搜索条件 -->
    <el-card class="search-card">
      <el-form :model="searchForm" inline>
        <el-form-item label="摄像头">
          <el-select v-model="searchForm.cameraId" placeholder="选择摄像头" clearable>
            <el-option
              v-for="camera in systemStore.cameras"
              :key="camera.id"
              :label="camera.name"
              :value="camera.id"
            />
          </el-select>
        </el-form-item>
        
        <el-form-item label="日期范围">
          <el-date-picker
            v-model="searchForm.dateRange"
            type="datetimerange"
            range-separator="至"
            start-placeholder="开始时间"
            end-placeholder="结束时间"
            format="YYYY-MM-DD HH:mm:ss"
            value-format="YYYY-MM-DD HH:mm:ss"
          />
        </el-form-item>
        
        <el-form-item label="录像类型">
          <el-select v-model="searchForm.type" placeholder="录像类型" clearable>
            <el-option label="全部" value="" />
            <el-option label="定时录像" value="scheduled" />
            <el-option label="报警录像" value="alert" />
            <el-option label="手动录像" value="manual" />
          </el-select>
        </el-form-item>
        
        <el-form-item>
          <el-button type="primary" @click="searchRecordings">
            <el-icon><Search /></el-icon>
            搜索
          </el-button>
          
          <el-button @click="resetSearch">
            <el-icon><Refresh /></el-icon>
            重置
          </el-button>
        </el-form-item>
      </el-form>
    </el-card>

    <!-- 录像列表 -->
    <el-card class="recordings-card">
      <template #header>
        <div class="card-header">
          <span>录像文件</span>
          <div class="header-actions">
            <el-button 
              type="danger" 
              :disabled="selectedRecordings.length === 0"
              @click="batchDelete"
            >
              <el-icon><Delete /></el-icon>
              批量删除
            </el-button>
          </div>
        </div>
      </template>
      
      <el-table
        v-loading="loading"
        :data="recordings"
        @selection-change="handleSelectionChange"
        stripe
      >
        <el-table-column type="selection" width="55" />
        
        <el-table-column label="缩略图" width="120">
          <template #default="{ row }">
            <div class="thumbnail">
              <img 
                v-if="row.thumbnail"
                :src="row.thumbnail" 
                :alt="row.filename"
                @click="playRecording(row)"
              />
              <div v-else class="no-thumbnail">
                <el-icon><VideoPlay /></el-icon>
              </div>
            </div>
          </template>
        </el-table-column>
        
        <el-table-column prop="filename" label="文件名" min-width="200" />
        
        <el-table-column prop="cameraName" label="摄像头" width="120" />
        
        <el-table-column prop="type" label="类型" width="100">
          <template #default="{ row }">
            <el-tag :type="getRecordingTypeColor(row.type)">
              {{ getRecordingTypeText(row.type) }}
            </el-tag>
          </template>
        </el-table-column>
        
        <el-table-column prop="startTime" label="开始时间" width="180">
          <template #default="{ row }">
            {{ formatTime(row.startTime) }}
          </template>
        </el-table-column>
        
        <el-table-column prop="duration" label="时长" width="100">
          <template #default="{ row }">
            {{ formatDuration(row.duration) }}
          </template>
        </el-table-column>
        
        <el-table-column prop="fileSize" label="文件大小" width="100">
          <template #default="{ row }">
            {{ formatFileSize(row.fileSize) }}
          </template>
        </el-table-column>
        
        <el-table-column label="操作" width="200" fixed="right">
          <template #default="{ row }">
            <el-button 
              type="primary" 
              size="small"
              @click="playRecording(row)"
            >
              <el-icon><VideoPlay /></el-icon>
              播放
            </el-button>
            
            <el-button 
              type="success" 
              size="small"
              @click="downloadRecording(row)"
            >
              <el-icon><Download /></el-icon>
              下载
            </el-button>
            
            <el-button 
              type="danger" 
              size="small"
              @click="deleteRecording(row)"
            >
              <el-icon><Delete /></el-icon>
              删除
            </el-button>
          </template>
        </el-table-column>
      </el-table>
      
      <!-- 分页 -->
      <div class="pagination">
        <el-pagination
          v-model:current-page="pagination.page"
          v-model:page-size="pagination.size"
          :total="pagination.total"
          :page-sizes="[10, 20, 50, 100]"
          layout="total, sizes, prev, pager, next, jumper"
          @size-change="handleSizeChange"
          @current-change="handleCurrentChange"
        />
      </div>
    </el-card>

    <!-- 视频播放对话框 -->
    <el-dialog
      v-model="playbackDialogVisible"
      :title="currentRecording?.filename"
      width="80%"
      :before-close="stopPlayback"
    >
      <div class="playback-container">
        <video
          ref="videoPlayer"
          controls
          preload="metadata"
          class="video-player"
          @loadedmetadata="onVideoLoaded"
          @timeupdate="onTimeUpdate"
        >
          您的浏览器不支持视频播放
        </video>
        
        <!-- 播放控制 -->
        <div class="playback-controls">
          <div class="time-info">
            <span>{{ formatTime(currentRecording?.startTime) }}</span>
            <span class="separator">|</span>
            <span>时长: {{ formatDuration(currentRecording?.duration) }}</span>
          </div>
          
          <div class="playback-actions">
            <el-button @click="changePlaybackSpeed(0.5)">0.5x</el-button>
            <el-button @click="changePlaybackSpeed(1)">1x</el-button>
            <el-button @click="changePlaybackSpeed(1.5)">1.5x</el-button>
            <el-button @click="changePlaybackSpeed(2)">2x</el-button>
            
            <el-divider direction="vertical" />
            
            <el-button @click="takeScreenshot">
              <el-icon><Camera /></el-icon>
              截图
            </el-button>
            
            <el-button @click="downloadRecording(currentRecording)">
              <el-icon><Download /></el-icon>
              下载
            </el-button>
          </div>
        </div>
      </div>
    </el-dialog>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { useSystemStore } from '@/stores/system'
import { apiService } from '@/services/api'
import { ElMessage, ElMessageBox } from 'element-plus'
import dayjs from 'dayjs'

const systemStore = useSystemStore()

// 响应式数据
const loading = ref(false)
const recordings = ref([])
const selectedRecordings = ref([])
const playbackDialogVisible = ref(false)
const currentRecording = ref(null)
const videoPlayer = ref(null)

// 搜索表单
const searchForm = reactive({
  cameraId: '',
  dateRange: [],
  type: ''
})

// 分页
const pagination = reactive({
  page: 1,
  size: 20,
  total: 0
})

// 方法
const searchRecordings = async () => {
  loading.value = true
  try {
    const params = {
      page: pagination.page,
      size: pagination.size,
      cameraId: searchForm.cameraId,
      type: searchForm.type
    }
    
    if (searchForm.dateRange && searchForm.dateRange.length === 2) {
      params.startTime = searchForm.dateRange[0]
      params.endTime = searchForm.dateRange[1]
    }
    
    const response = await apiService.getRecordings(params)
    recordings.value = response.data.items
    pagination.total = response.data.total
  } catch (error) {
    ElMessage.error('获取录像列表失败')
  } finally {
    loading.value = false
  }
}

const resetSearch = () => {
  searchForm.cameraId = ''
  searchForm.dateRange = []
  searchForm.type = ''
  pagination.page = 1
  searchRecordings()
}

const handleSelectionChange = (selection) => {
  selectedRecordings.value = selection
}

const handleSizeChange = (size) => {
  pagination.size = size
  pagination.page = 1
  searchRecordings()
}

const handleCurrentChange = (page) => {
  pagination.page = page
  searchRecordings()
}

const playRecording = (recording) => {
  currentRecording.value = recording
  playbackDialogVisible.value = true
  
  // 设置视频源
  setTimeout(() => {
    if (videoPlayer.value) {
      videoPlayer.value.src = `/api/recordings/${recording.id}/stream`
    }
  }, 100)
}

const stopPlayback = () => {
  if (videoPlayer.value) {
    videoPlayer.value.pause()
    videoPlayer.value.src = ''
  }
  currentRecording.value = null
}

const downloadRecording = async (recording) => {
  try {
    const response = await apiService.downloadRecording(recording.id)
    
    // 创建下载链接
    const url = window.URL.createObjectURL(new Blob([response.data]))
    const link = document.createElement('a')
    link.href = url
    link.download = recording.filename
    document.body.appendChild(link)
    link.click()
    document.body.removeChild(link)
    window.URL.revokeObjectURL(url)
    
    ElMessage.success('下载开始')
  } catch (error) {
    ElMessage.error('下载失败')
  }
}

const deleteRecording = async (recording) => {
  try {
    await ElMessageBox.confirm(
      `确定要删除录像文件 "${recording.filename}" 吗？`,
      '确认删除',
      {
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        type: 'warning'
      }
    )
    
    await apiService.deleteRecording(recording.id)
    ElMessage.success('删除成功')
    searchRecordings()
  } catch (error) {
    if (error !== 'cancel') {
      ElMessage.error('删除失败')
    }
  }
}

const batchDelete = async () => {
  try {
    await ElMessageBox.confirm(
      `确定要删除选中的 ${selectedRecordings.value.length} 个录像文件吗？`,
      '批量删除',
      {
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        type: 'warning'
      }
    )
    
    const promises = selectedRecordings.value.map(recording => 
      apiService.deleteRecording(recording.id)
    )
    
    await Promise.all(promises)
    ElMessage.success('批量删除成功')
    searchRecordings()
  } catch (error) {
    if (error !== 'cancel') {
      ElMessage.error('批量删除失败')
    }
  }
}

const changePlaybackSpeed = (speed) => {
  if (videoPlayer.value) {
    videoPlayer.value.playbackRate = speed
  }
}

const takeScreenshot = () => {
  if (videoPlayer.value) {
    const canvas = document.createElement('canvas')
    const ctx = canvas.getContext('2d')
    
    canvas.width = videoPlayer.value.videoWidth
    canvas.height = videoPlayer.value.videoHeight
    
    ctx.drawImage(videoPlayer.value, 0, 0, canvas.width, canvas.height)
    
    canvas.toBlob(blob => {
      const url = URL.createObjectURL(blob)
      const link = document.createElement('a')
      link.href = url
      link.download = `screenshot_${dayjs().format('YYYY-MM-DD_HH-mm-ss')}.png`
      link.click()
      URL.revokeObjectURL(url)
    })
    
    ElMessage.success('截图已保存')
  }
}

const onVideoLoaded = () => {
  console.log('Video loaded')
}

const onTimeUpdate = () => {
  // 可以在这里更新播放进度
}

// 工具函数
const getRecordingTypeColor = (type) => {
  const colors = {
    'scheduled': '',
    'alert': 'warning',
    'manual': 'success'
  }
  return colors[type] || ''
}

const getRecordingTypeText = (type) => {
  const texts = {
    'scheduled': '定时录像',
    'alert': '报警录像',
    'manual': '手动录像'
  }
  return texts[type] || type
}

const formatTime = (timestamp) => {
  return dayjs(timestamp).format('YYYY-MM-DD HH:mm:ss')
}

const formatDuration = (seconds) => {
  const hours = Math.floor(seconds / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)
  const secs = seconds % 60
  
  if (hours > 0) {
    return `${hours}:${minutes.toString().padStart(2, '0')}:${secs.toString().padStart(2, '0')}`
  } else {
    return `${minutes}:${secs.toString().padStart(2, '0')}`
  }
}

const formatFileSize = (bytes) => {
  if (bytes === 0) return '0 B'
  
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

onMounted(() => {
  // 设置默认搜索时间为今天
  const today = dayjs()
  searchForm.dateRange = [
    today.startOf('day').format('YYYY-MM-DD HH:mm:ss'),
    today.endOf('day').format('YYYY-MM-DD HH:mm:ss')
  ]
  
  searchRecordings()
})
</script>

<style scoped>
.playback {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.search-card {
  margin-bottom: 16px;
}

.recordings-card {
  flex: 1;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.thumbnail {
  width: 80px;
  height: 60px;
  border-radius: 4px;
  overflow: hidden;
  cursor: pointer;
}

.thumbnail img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.no-thumbnail {
  width: 100%;
  height: 100%;
  background: #f0f0f0;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #909399;
}

.pagination {
  margin-top: 20px;
  text-align: right;
}

.playback-container {
  width: 100%;
}

.video-player {
  width: 100%;
  max-height: 60vh;
  background: #000;
}

.playback-controls {
  margin-top: 16px;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px;
  background: #f5f5f5;
  border-radius: 4px;
}

.time-info {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 14px;
  color: #606266;
}

.separator {
  color: #dcdfe6;
}

.playback-actions {
  display: flex;
  align-items: center;
  gap: 8px;
}

/* 响应式 */
@media (max-width: 768px) {
  .playback-controls {
    flex-direction: column;
    gap: 12px;
  }
  
  .playback-actions {
    flex-wrap: wrap;
    justify-content: center;
  }
}
</style>
