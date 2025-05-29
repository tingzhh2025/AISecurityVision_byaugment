<template>
  <el-container class="layout-container">
    <!-- 侧边栏 -->
    <el-aside :width="sidebarWidth" class="sidebar">
      <div class="logo">
        <el-icon class="logo-icon"><Monitor /></el-icon>
        <span v-show="!isCollapse" class="logo-text">AI安防监控</span>
      </div>

      <el-menu
        :default-active="$route.path"
        :collapse="isCollapse"
        :unique-opened="true"
        router
        class="sidebar-menu"
      >
        <el-menu-item
          v-for="item in menuItems"
          :key="item.path"
          :index="item.path"
        >
          <el-icon><component :is="item.meta.icon" /></el-icon>
          <template #title>{{ item.meta.title }}</template>
        </el-menu-item>
      </el-menu>
    </el-aside>

    <!-- 主内容区 -->
    <el-container class="main-container">
      <!-- 顶部导航 -->
      <el-header class="header">
        <div class="header-left">
          <el-button
            link
            @click="toggleSidebar"
            class="collapse-btn"
          >
            <el-icon><Expand v-if="isCollapse" /><Fold v-else /></el-icon>
          </el-button>

          <el-breadcrumb separator="/">
            <el-breadcrumb-item>{{ currentRoute?.meta?.title || '首页' }}</el-breadcrumb-item>
          </el-breadcrumb>
        </div>

        <div class="header-right">
          <!-- 系统状态 -->
          <div class="system-status">
            <el-tooltip content="系统状态" placement="bottom">
              <div class="status-indicator" :class="systemStatus">
                <div class="status-dot"></div>
                <span>{{ systemStatusText }}</span>
              </div>
            </el-tooltip>
          </div>

          <!-- 时间显示 -->
          <div class="current-time">
            {{ currentTime }}
          </div>

          <!-- 用户菜单 -->
          <el-dropdown>
            <div class="user-avatar">
              <el-icon><User /></el-icon>
            </div>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item>个人设置</el-dropdown-item>
                <el-dropdown-item divided>退出登录</el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
        </div>
      </el-header>

      <!-- 主内容 -->
      <el-main class="main-content">
        <router-view />
      </el-main>
    </el-container>
  </el-container>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useRoute } from 'vue-router'
import { useSystemStore } from '@/stores/system'
import dayjs from 'dayjs'

const route = useRoute()
const systemStore = useSystemStore()

// 侧边栏状态
const isCollapse = ref(false)
const sidebarWidth = computed(() => isCollapse.value ? '64px' : '240px')

// 当前时间
const currentTime = ref('')
let timeInterval = null

// 菜单项
const menuItems = [
  { path: '/dashboard', meta: { title: '仪表盘', icon: 'Monitor' } },
  { path: '/live', meta: { title: '实时监控', icon: 'VideoCamera' } },
  { path: '/playback', meta: { title: '录像回放', icon: 'VideoPlay' } },
  { path: '/alerts', meta: { title: '报警管理', icon: 'Warning' } },
  { path: '/cameras', meta: { title: '摄像头管理', icon: 'Camera' } },
  { path: '/settings', meta: { title: '系统设置', icon: 'Setting' } }
]

// 当前路由
const currentRoute = computed(() => {
  return menuItems.find(item => item.path === route.path)
})

// 系统状态
const systemStatus = computed(() => {
  return systemStore.isOnline ? 'status-online' : 'status-offline'
})

const systemStatusText = computed(() => {
  return systemStore.isOnline ? '在线' : '离线'
})

// 切换侧边栏
const toggleSidebar = () => {
  isCollapse.value = !isCollapse.value
}

// 更新时间
const updateTime = () => {
  currentTime.value = dayjs().format('YYYY-MM-DD HH:mm:ss')
}

onMounted(() => {
  updateTime()
  timeInterval = setInterval(updateTime, 1000)
})

onUnmounted(() => {
  if (timeInterval) {
    clearInterval(timeInterval)
  }
})
</script>

<style scoped>
.layout-container {
  height: 100vh;
}

.sidebar {
  background: #304156;
  transition: width 0.3s;
  overflow: hidden;
}

.logo {
  height: 60px;
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 8px;
  color: #fff;
  font-size: 18px;
  font-weight: bold;
  border-bottom: 1px solid #434a50;
}

.logo-icon {
  font-size: 24px;
  color: #409eff;
}

.logo-text {
  white-space: nowrap;
}

.sidebar-menu {
  border: none;
  background: transparent;
}

.sidebar-menu :deep(.el-menu-item) {
  color: #bfcbd9;
  border-bottom: 1px solid #434a50;
}

.sidebar-menu :deep(.el-menu-item:hover),
.sidebar-menu :deep(.el-menu-item.is-active) {
  background-color: #409eff !important;
  color: #fff;
}

.main-container {
  background: #f0f2f5;
}

.header {
  background: #fff;
  border-bottom: 1px solid #e4e7ed;
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 0 20px;
  box-shadow: 0 1px 4px rgba(0, 21, 41, 0.08);
}

.header-left {
  display: flex;
  align-items: center;
  gap: 16px;
}

.collapse-btn {
  font-size: 18px;
  color: #606266;
}

.header-right {
  display: flex;
  align-items: center;
  gap: 20px;
}

.system-status {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 14px;
}

.current-time {
  font-size: 14px;
  color: #606266;
  font-family: 'Courier New', monospace;
}

.user-avatar {
  width: 32px;
  height: 32px;
  border-radius: 50%;
  background: #409eff;
  display: flex;
  align-items: center;
  justify-content: center;
  color: #fff;
  cursor: pointer;
}

.main-content {
  padding: 20px;
  overflow-y: auto;
}
</style>
