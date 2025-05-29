<template>
  <div class="alerts">
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
        
        <el-form-item label="报警级别">
          <el-select v-model="searchForm.level" placeholder="报警级别" clearable>
            <el-option label="全部" value="" />
            <el-option label="低" value="low" />
            <el-option label="中" value="medium" />
            <el-option label="高" value="high" />
            <el-option label="紧急" value="critical" />
          </el-select>
        </el-form-item>
        
        <el-form-item label="状态">
          <el-select v-model="searchForm.status" placeholder="处理状态" clearable>
            <el-option label="全部" value="" />
            <el-option label="未读" value="unread" />
            <el-option label="已读" value="read" />
            <el-option label="已处理" value="handled" />
          </el-select>
        </el-form-item>
        
        <el-form-item label="时间范围">
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
        
        <el-form-item>
          <el-button type="primary" @click="searchAlerts">
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

    <!-- 报警统计 -->
    <el-row :gutter="20" class="stats-row">
      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon total-icon">
              <el-icon><Warning /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ alertStats.total }}</div>
              <div class="stat-label">总报警数</div>
            </div>
          </div>
        </el-card>
      </el-col>
      
      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon unread-icon">
              <el-icon><Bell /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ alertStats.unread }}</div>
              <div class="stat-label">未读报警</div>
            </div>
          </div>
        </el-card>
      </el-col>
      
      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon today-icon">
              <el-icon><Calendar /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ alertStats.today }}</div>
              <div class="stat-label">今日报警</div>
            </div>
          </div>
        </el-card>
      </el-col>
      
      <el-col :span="6">
        <el-card class="stat-card">
          <div class="stat-content">
            <div class="stat-icon critical-icon">
              <el-icon><CircleClose /></el-icon>
            </div>
            <div class="stat-info">
              <div class="stat-value">{{ alertStats.critical }}</div>
              <div class="stat-label">紧急报警</div>
            </div>
          </div>
        </el-card>
      </el-col>
    </el-row>

    <!-- 报警列表 -->
    <el-card class="alerts-card">
      <template #header>
        <div class="card-header">
          <span>报警记录</span>
          <div class="header-actions">
            <el-button 
              type="primary"
              :disabled="selectedAlerts.length === 0"
              @click="batchMarkAsRead"
            >
              <el-icon><Check /></el-icon>
              批量标记已读
            </el-button>
            
            <el-button 
              type="danger" 
              :disabled="selectedAlerts.length === 0"
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
        :data="alerts"
        @selection-change="handleSelectionChange"
        stripe
        :row-class-name="getRowClassName"
      >
        <el-table-column type="selection" width="55" />
        
        <el-table-column label="报警图片" width="120">
          <template #default="{ row }">
            <div class="alert-image">
              <img 
                v-if="row.image"
                :src="row.image" 
                :alt="row.title"
                @click="previewImage(row.image)"
              />
              <div v-else class="no-image">
                <el-icon><Picture /></el-icon>
              </div>
            </div>
          </template>
        </el-table-column>
        
        <el-table-column prop="title" label="报警标题" min-width="200" />
        
        <el-table-column prop="cameraName" label="摄像头" width="120" />
        
        <el-table-column prop="level" label="级别" width="100">
          <template #default="{ row }">
            <el-tag :type="getAlertLevelColor(row.level)">
              {{ getAlertLevelText(row.level) }}
            </el-tag>
          </template>
        </el-table-column>
        
        <el-table-column prop="type" label="类型" width="120">
          <template #default="{ row }">
            <el-tag type="info">{{ getAlertTypeText(row.type) }}</el-tag>
          </template>
        </el-table-column>
        
        <el-table-column prop="timestamp" label="发生时间" width="180">
          <template #default="{ row }">
            {{ formatTime(row.timestamp) }}
          </template>
        </el-table-column>
        
        <el-table-column prop="status" label="状态" width="100">
          <template #default="{ row }">
            <el-tag :type="getStatusColor(row.status)">
              {{ getStatusText(row.status) }}
            </el-tag>
          </template>
        </el-table-column>
        
        <el-table-column label="操作" width="200" fixed="right">
          <template #default="{ row }">
            <el-button 
              v-if="!row.read"
              type="primary" 
              size="small"
              @click="markAsRead(row)"
            >
              <el-icon><Check /></el-icon>
              标记已读
            </el-button>
            
            <el-button 
              type="success" 
              size="small"
              @click="viewDetails(row)"
            >
              <el-icon><View /></el-icon>
              详情
            </el-button>
            
            <el-button 
              type="danger" 
              size="small"
              @click="deleteAlert(row)"
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

    <!-- 报警详情对话框 -->
    <el-dialog
      v-model="detailDialogVisible"
      :title="currentAlert?.title"
      width="800px"
    >
      <div v-if="currentAlert" class="alert-detail">
        <el-descriptions :column="2" border>
          <el-descriptions-item label="报警标题">
            {{ currentAlert.title }}
          </el-descriptions-item>
          
          <el-descriptions-item label="摄像头">
            {{ currentAlert.cameraName }}
          </el-descriptions-item>
          
          <el-descriptions-item label="报警级别">
            <el-tag :type="getAlertLevelColor(currentAlert.level)">
              {{ getAlertLevelText(currentAlert.level) }}
            </el-tag>
          </el-descriptions-item>
          
          <el-descriptions-item label="报警类型">
            <el-tag type="info">{{ getAlertTypeText(currentAlert.type) }}</el-tag>
          </el-descriptions-item>
          
          <el-descriptions-item label="发生时间">
            {{ formatTime(currentAlert.timestamp) }}
          </el-descriptions-item>
          
          <el-descriptions-item label="处理状态">
            <el-tag :type="getStatusColor(currentAlert.status)">
              {{ getStatusText(currentAlert.status) }}
            </el-tag>
          </el-descriptions-item>
          
          <el-descriptions-item label="置信度" v-if="currentAlert.confidence">
            {{ Math.round(currentAlert.confidence * 100) }}%
          </el-descriptions-item>
          
          <el-descriptions-item label="检测区域" v-if="currentAlert.region">
            {{ currentAlert.region }}
          </el-descriptions-item>
        </el-descriptions>
        
        <div v-if="currentAlert.description" class="alert-description">
          <h4>详细描述</h4>
          <p>{{ currentAlert.description }}</p>
        </div>
        
        <div v-if="currentAlert.image" class="alert-image-detail">
          <h4>报警图片</h4>
          <img :src="currentAlert.image" :alt="currentAlert.title" />
        </div>
      </div>
      
      <template #footer>
        <el-button @click="detailDialogVisible = false">关闭</el-button>
        <el-button 
          v-if="currentAlert && !currentAlert.read"
          type="primary" 
          @click="markAsRead(currentAlert)"
        >
          标记已读
        </el-button>
      </template>
    </el-dialog>

    <!-- 图片预览 -->
    <el-image-viewer
      v-if="previewVisible"
      :url-list="[previewImageUrl]"
      @close="previewVisible = false"
    />
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { useSystemStore } from '@/stores/system'
import { apiService } from '@/services/api'
import { ElMessage, ElMessageBox } from 'element-plus'
import dayjs from 'dayjs'

const systemStore = useSystemStore()

// 响应式数据
const loading = ref(false)
const alerts = ref([])
const selectedAlerts = ref([])
const detailDialogVisible = ref(false)
const currentAlert = ref(null)
const previewVisible = ref(false)
const previewImageUrl = ref('')

// 搜索表单
const searchForm = reactive({
  cameraId: '',
  level: '',
  status: '',
  dateRange: []
})

// 分页
const pagination = reactive({
  page: 1,
  size: 20,
  total: 0
})

// 报警统计
const alertStats = computed(() => {
  const stats = {
    total: alerts.value.length,
    unread: alerts.value.filter(alert => !alert.read).length,
    today: alerts.value.filter(alert => 
      dayjs(alert.timestamp).isSame(dayjs(), 'day')
    ).length,
    critical: alerts.value.filter(alert => alert.level === 'critical').length
  }
  return stats
})

// 方法
const searchAlerts = async () => {
  loading.value = true
  try {
    const params = {
      page: pagination.page,
      size: pagination.size,
      cameraId: searchForm.cameraId,
      level: searchForm.level,
      status: searchForm.status
    }
    
    if (searchForm.dateRange && searchForm.dateRange.length === 2) {
      params.startTime = searchForm.dateRange[0]
      params.endTime = searchForm.dateRange[1]
    }
    
    const response = await apiService.getAlerts(params)
    alerts.value = response.data.items
    pagination.total = response.data.total
  } catch (error) {
    ElMessage.error('获取报警列表失败')
  } finally {
    loading.value = false
  }
}

const resetSearch = () => {
  searchForm.cameraId = ''
  searchForm.level = ''
  searchForm.status = ''
  searchForm.dateRange = []
  pagination.page = 1
  searchAlerts()
}

const handleSelectionChange = (selection) => {
  selectedAlerts.value = selection
}

const handleSizeChange = (size) => {
  pagination.size = size
  pagination.page = 1
  searchAlerts()
}

const handleCurrentChange = (page) => {
  pagination.page = page
  searchAlerts()
}

const markAsRead = async (alert) => {
  try {
    await systemStore.markAlertAsRead(alert.id)
    alert.read = true
    ElMessage.success('已标记为已读')
  } catch (error) {
    ElMessage.error('操作失败')
  }
}

const batchMarkAsRead = async () => {
  try {
    const promises = selectedAlerts.value
      .filter(alert => !alert.read)
      .map(alert => systemStore.markAlertAsRead(alert.id))
    
    await Promise.all(promises)
    
    selectedAlerts.value.forEach(alert => {
      alert.read = true
    })
    
    ElMessage.success('批量标记已读成功')
  } catch (error) {
    ElMessage.error('批量操作失败')
  }
}

const deleteAlert = async (alert) => {
  try {
    await ElMessageBox.confirm(
      `确定要删除报警 "${alert.title}" 吗？`,
      '确认删除',
      {
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        type: 'warning'
      }
    )
    
    await apiService.deleteAlert(alert.id)
    ElMessage.success('删除成功')
    searchAlerts()
  } catch (error) {
    if (error !== 'cancel') {
      ElMessage.error('删除失败')
    }
  }
}

const batchDelete = async () => {
  try {
    await ElMessageBox.confirm(
      `确定要删除选中的 ${selectedAlerts.value.length} 条报警记录吗？`,
      '批量删除',
      {
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        type: 'warning'
      }
    )
    
    const promises = selectedAlerts.value.map(alert => 
      apiService.deleteAlert(alert.id)
    )
    
    await Promise.all(promises)
    ElMessage.success('批量删除成功')
    searchAlerts()
  } catch (error) {
    if (error !== 'cancel') {
      ElMessage.error('批量删除失败')
    }
  }
}

const viewDetails = (alert) => {
  currentAlert.value = alert
  detailDialogVisible.value = true
}

const previewImage = (imageUrl) => {
  previewImageUrl.value = imageUrl
  previewVisible.value = true
}

// 工具函数
const getRowClassName = ({ row }) => {
  if (!row.read) return 'unread-row'
  return ''
}

const getAlertLevelColor = (level) => {
  const colors = {
    'low': 'info',
    'medium': 'warning',
    'high': 'danger',
    'critical': 'danger'
  }
  return colors[level] || 'info'
}

const getAlertLevelText = (level) => {
  const texts = {
    'low': '低',
    'medium': '中',
    'high': '高',
    'critical': '紧急'
  }
  return texts[level] || level
}

const getAlertTypeText = (type) => {
  const texts = {
    'person_detection': '人员检测',
    'vehicle_detection': '车辆检测',
    'intrusion': '入侵检测',
    'loitering': '徘徊检测',
    'abandoned_object': '遗留物检测',
    'crowd_gathering': '聚集检测'
  }
  return texts[type] || type
}

const getStatusColor = (status) => {
  const colors = {
    'unread': 'danger',
    'read': 'warning',
    'handled': 'success'
  }
  return colors[status] || 'info'
}

const getStatusText = (status) => {
  const texts = {
    'unread': '未读',
    'read': '已读',
    'handled': '已处理'
  }
  return texts[status] || status
}

const formatTime = (timestamp) => {
  return dayjs(timestamp).format('YYYY-MM-DD HH:mm:ss')
}

onMounted(() => {
  // 设置默认搜索时间为今天
  const today = dayjs()
  searchForm.dateRange = [
    today.startOf('day').format('YYYY-MM-DD HH:mm:ss'),
    today.endOf('day').format('YYYY-MM-DD HH:mm:ss')
  ]
  
  searchAlerts()
})
</script>

<style scoped>
.alerts {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.search-card {
  margin-bottom: 16px;
}

.stats-row {
  margin-bottom: 16px;
}

.stat-card {
  height: 100px;
}

.stat-content {
  display: flex;
  align-items: center;
  height: 100%;
}

.stat-icon {
  width: 50px;
  height: 50px;
  border-radius: 8px;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 12px;
  font-size: 20px;
  color: #fff;
}

.total-icon {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

.unread-icon {
  background: linear-gradient(135deg, #f093fb 0%, #f5576c 100%);
}

.today-icon {
  background: linear-gradient(135deg, #4facfe 0%, #00f2fe 100%);
}

.critical-icon {
  background: linear-gradient(135deg, #fa709a 0%, #fee140 100%);
}

.stat-info {
  flex: 1;
}

.stat-value {
  font-size: 24px;
  font-weight: bold;
  color: #303133;
  line-height: 1;
}

.stat-label {
  font-size: 12px;
  color: #909399;
  margin-top: 4px;
}

.alerts-card {
  flex: 1;
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

.alert-image {
  width: 80px;
  height: 60px;
  border-radius: 4px;
  overflow: hidden;
  cursor: pointer;
}

.alert-image img {
  width: 100%;
  height: 100%;
  object-fit: cover;
}

.no-image {
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

.alert-detail {
  padding: 20px 0;
}

.alert-description {
  margin-top: 20px;
}

.alert-description h4 {
  margin-bottom: 8px;
  color: #303133;
}

.alert-description p {
  color: #606266;
  line-height: 1.6;
}

.alert-image-detail {
  margin-top: 20px;
}

.alert-image-detail h4 {
  margin-bottom: 12px;
  color: #303133;
}

.alert-image-detail img {
  max-width: 100%;
  border-radius: 4px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

/* 未读行样式 */
:deep(.unread-row) {
  background-color: #fef0f0;
}

:deep(.unread-row:hover) {
  background-color: #fde2e2 !important;
}
</style>
