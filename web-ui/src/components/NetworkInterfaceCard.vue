<template>
  <el-card class="interface-card" :class="{ 'interface-disabled': !interface.is_up }">
    <template #header>
      <div class="card-header">
        <div class="interface-info">
          <h3 class="interface-name">{{ interface.display_name || interface.name }}</h3>
          <el-tag :type="getStatusType(interface.status)" size="small">
            {{ interface.status }}
          </el-tag>
        </div>
        <div class="interface-actions">
          <el-switch
            v-model="interface.is_up"
            @change="handleToggle"
            :disabled="loading"
            active-text="启用"
            inactive-text="禁用"
          />
        </div>
      </div>
    </template>

    <div class="interface-details">
      <!-- 基本信息 -->
      <el-descriptions :column="1" size="small" border>
        <el-descriptions-item label="接口类型">
          <el-tag :type="getTypeColor(interface.type)" size="small">
            {{ getTypeLabel(interface.type) }}
          </el-tag>
        </el-descriptions-item>
        
        <el-descriptions-item label="MAC地址">
          <code>{{ interface.mac_address || 'N/A' }}</code>
        </el-descriptions-item>
        
        <el-descriptions-item label="IP配置">
          <el-tag :type="interface.is_dhcp ? 'success' : 'info'" size="small">
            {{ interface.is_dhcp ? 'DHCP' : '静态IP' }}
          </el-tag>
        </el-descriptions-item>
        
        <el-descriptions-item label="IP地址">
          <span class="ip-address">{{ interface.ip_address || '未分配' }}</span>
        </el-descriptions-item>
        
        <el-descriptions-item label="子网掩码">
          {{ interface.netmask || 'N/A' }}
        </el-descriptions-item>
        
        <el-descriptions-item label="网关">
          {{ interface.gateway || 'N/A' }}
        </el-descriptions-item>
      </el-descriptions>

      <!-- 无线网络特有信息 -->
      <div v-if="interface.type === 'wireless'" class="wireless-info">
        <el-divider content-position="left">无线网络信息</el-divider>
        <el-descriptions :column="1" size="small" border>
          <el-descriptions-item label="SSID">
            {{ interface.ssid || '未连接' }}
          </el-descriptions-item>
          
          <el-descriptions-item label="信号强度">
            <el-progress
              :percentage="interface.signal_strength || 0"
              :color="getSignalColor(interface.signal_strength)"
              :show-text="true"
              :format="() => `${interface.signal_strength || 0}%`"
            />
          </el-descriptions-item>
          
          <el-descriptions-item label="安全类型">
            {{ interface.security || 'N/A' }}
          </el-descriptions-item>
        </el-descriptions>
      </div>

      <!-- 网络统计 -->
      <div class="network-stats">
        <el-divider content-position="left">网络统计</el-divider>
        <el-row :gutter="16">
          <el-col :span="12">
            <el-statistic
              title="接收数据"
              :value="interface.bytes_received || 0"
              :formatter="formatBytes"
            >
              <template #suffix>
                <el-icon color="#67C23A"><Download /></el-icon>
              </template>
            </el-statistic>
          </el-col>
          <el-col :span="12">
            <el-statistic
              title="发送数据"
              :value="interface.bytes_sent || 0"
              :formatter="formatBytes"
            >
              <template #suffix>
                <el-icon color="#E6A23C"><Upload /></el-icon>
              </template>
            </el-statistic>
          </el-col>
        </el-row>
      </div>

      <!-- 操作按钮 -->
      <div class="interface-actions-bottom">
        <el-button
          type="primary"
          size="small"
          @click="$emit('configure', interface)"
          :disabled="!interface.is_up"
        >
          <el-icon><Setting /></el-icon>
          配置
        </el-button>
        
        <el-button
          type="success"
          size="small"
          @click="$emit('test')"
          :disabled="!interface.is_up || !interface.is_connected"
        >
          <el-icon><Connection /></el-icon>
          测试连接
        </el-button>
      </div>
    </div>
  </el-card>
</template>

<script setup>
import { ref } from 'vue'
import { ElMessage } from 'element-plus'
import { Download, Upload, Setting, Connection } from '@element-plus/icons-vue'

// Props
const props = defineProps({
  interface: {
    type: Object,
    required: true
  }
})

// Emits
const emit = defineEmits(['configure', 'enable', 'disable', 'test'])

// 响应式数据
const loading = ref(false)

// 方法
const handleToggle = async (enabled) => {
  try {
    loading.value = true
    if (enabled) {
      emit('enable', props.interface)
    } else {
      emit('disable', props.interface)
    }
  } finally {
    loading.value = false
  }
}

const getStatusType = (status) => {
  const statusMap = {
    '已连接': 'success',
    '已启用': 'warning',
    '已禁用': 'info',
    '错误': 'danger'
  }
  return statusMap[status] || 'info'
}

const getTypeColor = (type) => {
  const typeMap = {
    'ethernet': 'primary',
    'wireless': 'success',
    'loopback': 'info',
    'unknown': 'warning'
  }
  return typeMap[type] || 'info'
}

const getTypeLabel = (type) => {
  const labelMap = {
    'ethernet': '以太网',
    'wireless': '无线网络',
    'loopback': '回环接口',
    'unknown': '未知类型'
  }
  return labelMap[type] || type
}

const getSignalColor = (strength) => {
  if (strength >= 80) return '#67C23A'
  if (strength >= 60) return '#E6A23C'
  if (strength >= 40) return '#F56C6C'
  return '#909399'
}

const formatBytes = (value) => {
  const bytes = parseInt(value)
  if (bytes === 0) return '0 B'
  
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}
</script>

<style scoped>
.interface-card {
  height: 100%;
  transition: all 0.3s ease;
}

.interface-card:hover {
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
}

.interface-disabled {
  opacity: 0.7;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.interface-info {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.interface-name {
  margin: 0;
  font-size: 16px;
  font-weight: 600;
  color: #303133;
}

.interface-details {
  padding: 0;
}

.ip-address {
  font-family: 'Courier New', monospace;
  font-weight: 600;
  color: #409EFF;
}

.wireless-info,
.network-stats {
  margin-top: 16px;
}

.interface-actions-bottom {
  margin-top: 20px;
  display: flex;
  gap: 8px;
  justify-content: flex-end;
}

.el-descriptions {
  margin-bottom: 0;
}

.el-statistic {
  text-align: center;
}

/* 响应式 */
@media (max-width: 768px) {
  .card-header {
    flex-direction: column;
    gap: 12px;
    align-items: stretch;
  }
  
  .interface-actions-bottom {
    justify-content: center;
  }
  
  .el-row {
    margin: 0 !important;
  }
  
  .el-col {
    padding: 0 8px !important;
  }
}
</style>
