<template>
  <div id="app">
    <router-view />
  </div>
</template>

<script setup>
import { onMounted } from 'vue'
import { useSystemStore } from '@/stores/system'

const systemStore = useSystemStore()

onMounted(() => {
  // 初始化系统状态
  systemStore.initializeSystem()
})
</script>

<style scoped>
#app {
  height: 100vh;
  overflow: hidden;
}
</style>
