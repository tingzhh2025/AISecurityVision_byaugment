<template>
  <div class="network-config-form">
    <el-form
      ref="formRef"
      :model="form"
      :rules="rules"
      label-width="120px"
      @submit.prevent
    >
      <!-- 基本配置 -->
      <el-form-item label="接口状态">
        <el-switch
          v-model="form.enabled"
          active-text="启用"
          inactive-text="禁用"
        />
      </el-form-item>

      <!-- IP配置方式 -->
      <el-form-item label="IP配置方式" prop="is_dhcp">
        <el-radio-group v-model="form.is_dhcp" @change="handleConfigModeChange">
          <el-radio :label="true">DHCP自动获取</el-radio>
          <el-radio :label="false">静态IP配置</el-radio>
        </el-radio-group>
      </el-form-item>

      <!-- 静态IP配置 -->
      <template v-if="!form.is_dhcp">
        <el-form-item label="IP地址" prop="ip_address">
          <el-input
            v-model="form.ip_address"
            placeholder="例如: 192.168.1.100"
            clearable
          />
        </el-form-item>

        <el-form-item label="子网掩码" prop="netmask">
          <el-select
            v-model="form.netmask"
            placeholder="选择子网掩码"
            clearable
            filterable
            allow-create
          >
            <el-option label="255.255.255.0 (/24)" value="255.255.255.0" />
            <el-option label="255.255.0.0 (/16)" value="255.255.0.0" />
            <el-option label="255.0.0.0 (/8)" value="255.0.0.0" />
            <el-option label="255.255.255.128 (/25)" value="255.255.255.128" />
            <el-option label="255.255.255.192 (/26)" value="255.255.255.192" />
            <el-option label="255.255.255.224 (/27)" value="255.255.255.224" />
            <el-option label="255.255.255.240 (/28)" value="255.255.255.240" />
          </el-select>
        </el-form-item>

        <el-form-item label="默认网关" prop="gateway">
          <el-input
            v-model="form.gateway"
            placeholder="例如: 192.168.1.1"
            clearable
          />
        </el-form-item>
      </template>

      <!-- DNS配置 -->
      <el-divider content-position="left">DNS配置</el-divider>
      
      <el-form-item label="首选DNS" prop="dns1">
        <el-select
          v-model="form.dns1"
          placeholder="选择或输入DNS服务器"
          clearable
          filterable
          allow-create
        >
          <el-option label="8.8.8.8 (Google)" value="8.8.8.8" />
          <el-option label="8.8.4.4 (Google)" value="8.8.4.4" />
          <el-option label="1.1.1.1 (Cloudflare)" value="1.1.1.1" />
          <el-option label="1.0.0.1 (Cloudflare)" value="1.0.0.1" />
          <el-option label="114.114.114.114 (114DNS)" value="114.114.114.114" />
          <el-option label="223.5.5.5 (阿里DNS)" value="223.5.5.5" />
          <el-option label="119.29.29.29 (腾讯DNS)" value="119.29.29.29" />
        </el-select>
      </el-form-item>

      <el-form-item label="备用DNS" prop="dns2">
        <el-select
          v-model="form.dns2"
          placeholder="选择或输入备用DNS服务器"
          clearable
          filterable
          allow-create
        >
          <el-option label="8.8.4.4 (Google)" value="8.8.4.4" />
          <el-option label="8.8.8.8 (Google)" value="8.8.8.8" />
          <el-option label="1.0.0.1 (Cloudflare)" value="1.0.0.1" />
          <el-option label="1.1.1.1 (Cloudflare)" value="1.1.1.1" />
          <el-option label="114.114.115.115 (114DNS)" value="114.114.115.115" />
          <el-option label="223.6.6.6 (阿里DNS)" value="223.6.6.6" />
          <el-option label="119.28.28.28 (腾讯DNS)" value="119.28.28.28" />
        </el-select>
      </el-form-item>

      <!-- 配置预览 -->
      <el-divider content-position="left">配置预览</el-divider>
      
      <el-alert
        :title="getConfigSummary()"
        type="info"
        :closable="false"
        show-icon
      />

      <!-- 警告信息 -->
      <el-alert
        v-if="!form.is_dhcp && hasNetworkConflict"
        title="网络配置警告"
        description="检测到可能的网络冲突，请确认IP地址和网关配置正确。"
        type="warning"
        :closable="false"
        show-icon
        style="margin-top: 16px;"
      />
    </el-form>

    <!-- 操作按钮 -->
    <div class="form-actions">
      <el-button @click="$emit('cancel')">取消</el-button>
      <el-button @click="resetForm">重置</el-button>
      <el-button type="primary" @click="handleSave" :loading="saving">
        保存配置
      </el-button>
    </div>
  </div>
</template>

<script setup>
import { ref, reactive, computed, watch } from 'vue'
import { ElMessage } from 'element-plus'

// Props
const props = defineProps({
  networkInterface: {
    type: Object,
    required: true
  }
})

// Emits
const emit = defineEmits(['save', 'cancel'])

// 响应式数据
const formRef = ref(null)
const saving = ref(false)

const form = reactive({
  enabled: props.networkInterface.is_up || false,
  is_dhcp: props.networkInterface.is_dhcp !== false,
  ip_address: props.networkInterface.ip_address || '',
  netmask: props.networkInterface.netmask || '255.255.255.0',
  gateway: props.networkInterface.gateway || '',
  dns1: props.networkInterface.dns1 || '8.8.8.8',
  dns2: props.networkInterface.dns2 || '8.8.4.4'
})

// 表单验证规则
const rules = {
  ip_address: [
    {
      validator: (rule, value, callback) => {
        if (!form.is_dhcp && !value) {
          callback(new Error('请输入IP地址'))
        } else if (!form.is_dhcp && !isValidIP(value)) {
          callback(new Error('请输入有效的IP地址'))
        } else {
          callback()
        }
      },
      trigger: 'blur'
    }
  ],
  netmask: [
    {
      validator: (rule, value, callback) => {
        if (!form.is_dhcp && !value) {
          callback(new Error('请选择子网掩码'))
        } else if (!form.is_dhcp && !isValidIP(value)) {
          callback(new Error('请输入有效的子网掩码'))
        } else {
          callback()
        }
      },
      trigger: 'blur'
    }
  ],
  gateway: [
    {
      validator: (rule, value, callback) => {
        if (!form.is_dhcp && value && !isValidIP(value)) {
          callback(new Error('请输入有效的网关地址'))
        } else {
          callback()
        }
      },
      trigger: 'blur'
    }
  ],
  dns1: [
    {
      validator: (rule, value, callback) => {
        if (value && !isValidIP(value)) {
          callback(new Error('请输入有效的DNS服务器地址'))
        } else {
          callback()
        }
      },
      trigger: 'blur'
    }
  ],
  dns2: [
    {
      validator: (rule, value, callback) => {
        if (value && !isValidIP(value)) {
          callback(new Error('请输入有效的DNS服务器地址'))
        } else {
          callback()
        }
      },
      trigger: 'blur'
    }
  ]
}

// 计算属性
const hasNetworkConflict = computed(() => {
  if (form.is_dhcp) return false
  
  // 简单的网络冲突检测
  if (form.ip_address && form.gateway) {
    const ipParts = form.ip_address.split('.')
    const gatewayParts = form.gateway.split('.')
    
    // 检查IP和网关是否在同一网段
    if (ipParts.length === 4 && gatewayParts.length === 4) {
      const ipNetwork = `${ipParts[0]}.${ipParts[1]}.${ipParts[2]}`
      const gatewayNetwork = `${gatewayParts[0]}.${gatewayParts[1]}.${gatewayParts[2]}`
      
      return ipNetwork !== gatewayNetwork
    }
  }
  
  return false
})

// 方法
const isValidIP = (ip) => {
  const ipRegex = /^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$/
  return ipRegex.test(ip)
}

const handleConfigModeChange = (isDhcp) => {
  if (isDhcp) {
    // 切换到DHCP时清空静态IP配置
    form.ip_address = ''
    form.netmask = '255.255.255.0'
    form.gateway = ''
  }
}

const getConfigSummary = () => {
  if (form.is_dhcp) {
    return `接口将使用DHCP自动获取IP配置，DNS: ${form.dns1}${form.dns2 ? ', ' + form.dns2 : ''}`
  } else {
    return `静态IP: ${form.ip_address || '未设置'}/${form.netmask || '未设置'}，网关: ${form.gateway || '未设置'}，DNS: ${form.dns1}${form.dns2 ? ', ' + form.dns2 : ''}`
  }
}

const resetForm = () => {
  form.enabled = props.networkInterface.is_up || false
  form.is_dhcp = props.networkInterface.is_dhcp !== false
  form.ip_address = props.networkInterface.ip_address || ''
  form.netmask = props.networkInterface.netmask || '255.255.255.0'
  form.gateway = props.networkInterface.gateway || ''
  form.dns1 = props.networkInterface.dns1 || '8.8.8.8'
  form.dns2 = props.networkInterface.dns2 || '8.8.4.4'

  // 清除验证错误
  if (formRef.value) {
    formRef.value.clearValidate()
  }
}

const handleSave = async () => {
  try {
    // 表单验证
    const valid = await formRef.value.validate()
    if (!valid) {
      return
    }

    saving.value = true
    
    // 发送配置数据
    emit('save', {
      enabled: form.enabled,
      is_dhcp: form.is_dhcp,
      ip_address: form.ip_address,
      netmask: form.netmask,
      gateway: form.gateway,
      dns1: form.dns1,
      dns2: form.dns2
    })
    
  } catch (error) {
    console.error('Form validation failed:', error)
  } finally {
    saving.value = false
  }
}

// 监听接口变化，更新表单数据
watch(() => props.networkInterface, (newInterface) => {
  if (newInterface) {
    resetForm()
  }
}, { immediate: true })
</script>

<style scoped>
.network-config-form {
  padding: 0;
}

.form-actions {
  margin-top: 24px;
  text-align: right;
  border-top: 1px solid #EBEEF5;
  padding-top: 16px;
}

.form-actions .el-button {
  margin-left: 8px;
}

.el-alert {
  margin-bottom: 16px;
}

.el-divider {
  margin: 24px 0 16px 0;
}

/* 响应式 */
@media (max-width: 768px) {
  .form-actions {
    text-align: center;
  }
  
  .form-actions .el-button {
    margin: 4px;
  }
}
</style>
