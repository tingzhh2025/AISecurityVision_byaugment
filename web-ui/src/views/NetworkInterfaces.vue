<template>
  <div class="network-interfaces">
    <el-card>
      <template #header>
        <div class="card-header">
          <span>网络接口配置</span>
          <el-button type="primary" @click="refreshInterfaces" :loading="loading">
            <el-icon><Refresh /></el-icon>
            刷新
          </el-button>
        </div>
      </template>

      <div v-loading="loading" class="interfaces-container">
        <div v-if="interfaces.length === 0" class="empty-state">
          <el-empty description="未检测到网络接口" />
        </div>

        <div v-else class="interfaces-grid">
          <NetworkInterfaceCard
            v-for="networkInterface in interfaces"
            :key="networkInterface.name"
            :interface="networkInterface"
            @configure="handleConfigure"
            @enable="handleEnable"
            @disable="handleDisable"
            @test="handleTest"
          />
        </div>
      </div>
    </el-card>

    <!-- 网络配置对话框 -->
    <el-dialog
      v-model="showConfigDialog"
      :title="`配置网络接口: ${selectedInterface?.name}`"
      width="600px"
      :close-on-click-modal="false"
    >
      <NetworkConfigForm
        v-if="selectedInterface"
        :network-interface="selectedInterface"
        @save="handleSaveConfig"
        @cancel="showConfigDialog = false"
      />
    </el-dialog>

    <!-- 网络测试对话框 -->
    <el-dialog
      v-model="showTestDialog"
      title="网络连接测试"
      width="500px"
    >
      <el-form :model="testForm" label-width="100px">
        <el-form-item label="测试主机">
          <el-input
            v-model="testForm.host"
            placeholder="请输入IP地址或域名"
            clearable
          />
        </el-form-item>
        <el-form-item label="超时时间">
          <el-input-number
            v-model="testForm.timeout"
            :min="1"
            :max="30"
            :step="1"
            controls-position="right"
          />
          <span class="input-suffix">秒</span>
        </el-form-item>
      </el-form>

      <template #footer>
        <el-button @click="showTestDialog = false">取消</el-button>
        <el-button type="primary" @click="runNetworkTest" :loading="testLoading">
          开始测试
        </el-button>
      </template>
    </el-dialog>

    <!-- 网络统计信息 -->
    <el-card class="stats-card" v-if="networkStats">
      <template #header>
        <span>网络统计信息</span>
      </template>
      
      <el-descriptions :column="2" border>
        <el-descriptions-item
          v-for="(value, key) in networkStats"
          :key="key"
          :label="formatStatLabel(key)"
        >
          {{ formatStatValue(key, value) }}
        </el-descriptions-item>
      </el-descriptions>
    </el-card>
  </div>
</template>

<script setup>
import { ref, reactive, onMounted } from 'vue'
import { ElMessage, ElMessageBox } from 'element-plus'
import { Refresh } from '@element-plus/icons-vue'
import { apiService } from '@/services/api'
import NetworkInterfaceCard from '@/components/NetworkInterfaceCard.vue'
import NetworkConfigForm from '@/components/NetworkConfigForm.vue'

// 响应式数据
const loading = ref(false)
const interfaces = ref([])
const networkStats = ref(null)
const showConfigDialog = ref(false)
const showTestDialog = ref(false)
const selectedInterface = ref(null)
const testLoading = ref(false)

const testForm = reactive({
  host: '8.8.8.8',
  timeout: 5
})

// 方法
const loadInterfaces = async () => {
  try {
    loading.value = true
    const response = await apiService.getNetworkInterfaces()
    interfaces.value = response.data.interfaces || []
    console.log('Network interfaces loaded:', interfaces.value)
  } catch (error) {
    console.error('Failed to load network interfaces:', error)
    ElMessage.error('加载网络接口失败: ' + (error.response?.data?.message || error.message))
  } finally {
    loading.value = false
  }
}

const loadNetworkStats = async () => {
  try {
    const response = await apiService.getNetworkStats()
    networkStats.value = response.data.network_stats || {}
  } catch (error) {
    console.error('Failed to load network stats:', error)
  }
}

const refreshInterfaces = async () => {
  await Promise.all([
    loadInterfaces(),
    loadNetworkStats()
  ])
}

const handleConfigure = (networkInterface) => {
  selectedInterface.value = networkInterface
  showConfigDialog.value = true
}

const handleEnable = async (networkInterface) => {
  try {
    await apiService.enableNetworkInterface(networkInterface.name)
    ElMessage.success(`网络接口 ${networkInterface.name} 已启用`)
    await refreshInterfaces()
  } catch (error) {
    console.error('Failed to enable interface:', error)
    ElMessage.error('启用失败: ' + (error.response?.data?.message || error.message))
  }
}

const handleDisable = async (networkInterface) => {
  try {
    await ElMessageBox.confirm(
      `确定要禁用网络接口 ${networkInterface.name} 吗？这可能会影响网络连接。`,
      '确认禁用',
      {
        confirmButtonText: '确定',
        cancelButtonText: '取消',
        type: 'warning'
      }
    )

    await apiService.disableNetworkInterface(networkInterface.name)
    ElMessage.success(`网络接口 ${networkInterface.name} 已禁用`)
    await refreshInterfaces()
  } catch (error) {
    if (error !== 'cancel') {
      console.error('Failed to disable interface:', error)
      ElMessage.error('禁用失败: ' + (error.response?.data?.message || error.message))
    }
  }
}

const handleTest = () => {
  showTestDialog.value = true
}

const handleSaveConfig = async (config) => {
  try {
    await apiService.configureNetworkInterface(selectedInterface.value.name, config)
    ElMessage.success('网络配置保存成功')
    showConfigDialog.value = false
    await refreshInterfaces()
  } catch (error) {
    console.error('Failed to save network config:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  }
}

const runNetworkTest = async () => {
  try {
    testLoading.value = true
    const response = await apiService.testNetworkConnection(testForm)
    
    if (response.data.test_result) {
      ElMessage.success(`连接到 ${testForm.host} 成功`)
    } else {
      ElMessage.error(`连接到 ${testForm.host} 失败`)
    }
    
    showTestDialog.value = false
  } catch (error) {
    console.error('Network test failed:', error)
    ElMessage.error('网络测试失败: ' + (error.response?.data?.message || error.message))
  } finally {
    testLoading.value = false
  }
}

const formatStatLabel = (key) => {
  const labelMap = {
    'eth0_rx_bytes': 'eth0 接收字节',
    'eth0_tx_bytes': 'eth0 发送字节',
    'eth0_status': 'eth0 状态',
    'wlan0_rx_bytes': 'wlan0 接收字节',
    'wlan0_tx_bytes': 'wlan0 发送字节',
    'wlan0_status': 'wlan0 状态'
  }
  return labelMap[key] || key
}

const formatStatValue = (key, value) => {
  if (key.includes('_bytes')) {
    const bytes = parseInt(value)
    if (bytes > 1024 * 1024 * 1024) {
      return (bytes / (1024 * 1024 * 1024)).toFixed(2) + ' GB'
    } else if (bytes > 1024 * 1024) {
      return (bytes / (1024 * 1024)).toFixed(2) + ' MB'
    } else if (bytes > 1024) {
      return (bytes / 1024).toFixed(2) + ' KB'
    }
    return bytes + ' B'
  }
  return value
}

onMounted(() => {
  refreshInterfaces()
})
</script>

<style scoped>
.network-interfaces {
  padding: 0;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.interfaces-container {
  min-height: 200px;
}

.interfaces-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(400px, 1fr));
  gap: 20px;
}

.empty-state {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 200px;
}

.stats-card {
  margin-top: 20px;
}

.input-suffix {
  margin-left: 8px;
  color: #909399;
}

/* 响应式 */
@media (max-width: 768px) {
  .interfaces-grid {
    grid-template-columns: 1fr;
  }
  
  .card-header {
    flex-direction: column;
    gap: 10px;
    align-items: stretch;
  }
}
</style>
