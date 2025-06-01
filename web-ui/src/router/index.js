import { createRouter, createWebHistory } from 'vue-router'

const routes = [
  {
    path: '/',
    name: 'Layout',
    component: () => import('@/layout/index.vue'),
    redirect: '/dashboard',
    children: [
      {
        path: '/dashboard',
        name: 'Dashboard',
        component: () => import('@/views/Dashboard.vue'),
        meta: { title: '仪表盘', icon: 'Monitor' }
      },
      {
        path: '/live',
        name: 'Live',
        component: () => import('@/views/Live.vue'),
        meta: { title: '实时监控', icon: 'VideoCamera' }
      },
      {
        path: '/playback',
        name: 'Playback',
        component: () => import('@/views/Playback.vue'),
        meta: { title: '录像回放', icon: 'VideoPlay' }
      },
      {
        path: '/alerts',
        name: 'Alerts',
        component: () => import('@/views/Alerts.vue'),
        meta: { title: '报警管理', icon: 'Warning' }
      },
      {
        path: '/cameras',
        name: 'Cameras',
        component: () => import('@/views/Cameras.vue'),
        meta: { title: '摄像头管理', icon: 'Camera' }
      },
      {
        path: '/person-statistics',
        name: 'PersonStatistics',
        component: () => import('@/views/PersonStatistics.vue'),
        meta: { title: '人员统计', icon: 'User' }
      },
      {
        path: '/settings',
        name: 'Settings',
        component: () => import('@/views/Settings.vue'),
        meta: { title: '系统设置', icon: 'Setting' }
      }
    ]
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

export default router
