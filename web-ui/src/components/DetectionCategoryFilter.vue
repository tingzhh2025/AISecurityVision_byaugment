<template>
  <div class="detection-category-filter">
    <el-card>
      <template #header>
        <div class="card-header">
          <span>AI检测类别过滤</span>
          <div class="header-actions">
            <el-button 
              link 
              @click="refreshCategories"
              :loading="loading"
            >
              <el-icon><Refresh /></el-icon>
            </el-button>
          </div>
        </div>
      </template>

      <div v-if="loading" class="loading-state">
        <el-skeleton :rows="4" animated />
      </div>

      <div v-else class="category-content">
        <!-- 快速选择 -->
        <div class="quick-select">
          <el-button-group>
            <el-button 
              size="small" 
              @click="selectAll"
              :disabled="saving"
            >
              全选
            </el-button>
            <el-button 
              size="small" 
              @click="selectNone"
              :disabled="saving"
            >
              全不选
            </el-button>
            <el-button 
              size="small" 
              @click="selectCommon"
              :disabled="saving"
            >
              常用类别
            </el-button>
          </el-button-group>
        </div>

        <!-- 类别选择 -->
        <div class="category-grid">
          <el-checkbox-group 
            v-model="selectedCategories" 
            @change="onCategoryChange"
            :disabled="saving"
          >
            <div class="category-section" v-for="section in categorySections" :key="section.name">
              <h4 class="section-title">{{ section.name }}</h4>
              <div class="category-items">
                <el-checkbox 
                  v-for="category in section.categories" 
                  :key="category"
                  :label="category"
                  class="category-item"
                >
                  {{ getCategoryDisplayName(category) }}
                </el-checkbox>
              </div>
            </div>
          </el-checkbox-group>
        </div>

        <!-- 统计信息 -->
        <div class="category-stats">
          <el-descriptions :column="3" size="small" border>
            <el-descriptions-item label="总类别数">
              {{ availableCategories.length }}
            </el-descriptions-item>
            <el-descriptions-item label="已启用">
              {{ selectedCategories.length }}
            </el-descriptions-item>
            <el-descriptions-item label="已禁用">
              {{ availableCategories.length - selectedCategories.length }}
            </el-descriptions-item>
          </el-descriptions>
        </div>

        <!-- 操作按钮 -->
        <div class="action-buttons">
          <el-button 
            type="primary" 
            @click="saveCategories"
            :loading="saving"
            :disabled="!hasChanges"
          >
            保存设置
          </el-button>
          <el-button 
            @click="resetCategories"
            :disabled="saving || !hasChanges"
          >
            重置
          </el-button>
        </div>

        <!-- 实时预览 -->
        <div class="preview-section" v-if="hasChanges">
          <el-alert
            title="设置预览"
            type="info"
            :closable="false"
            show-icon
          >
            <template #default>
              <p>将启用 {{ selectedCategories.length }} 个检测类别：</p>
              <el-tag 
                v-for="category in selectedCategories.slice(0, 10)" 
                :key="category"
                size="small"
                style="margin: 2px;"
              >
                {{ getCategoryDisplayName(category) }}
              </el-tag>
              <el-tag v-if="selectedCategories.length > 10" size="small" type="info">
                +{{ selectedCategories.length - 10 }} 更多...
              </el-tag>
            </template>
          </el-alert>
        </div>
      </div>
    </el-card>
  </div>
</template>

<script setup>
import { ref, reactive, computed, onMounted } from 'vue'
import { apiService } from '@/services/api'
import { ElMessage } from 'element-plus'
import { Refresh } from '@element-plus/icons-vue'

// 响应式数据
const loading = ref(false)
const saving = ref(false)
const availableCategories = ref([])
const selectedCategories = ref([])
const originalCategories = ref([])

// 类别显示名称映射
const categoryDisplayNames = {
  'person': '人员',
  'bicycle': '自行车',
  'car': '汽车',
  'motorcycle': '摩托车',
  'airplane': '飞机',
  'bus': '公交车',
  'train': '火车',
  'truck': '卡车',
  'boat': '船只',
  'traffic light': '交通灯',
  'fire hydrant': '消防栓',
  'stop sign': '停车标志',
  'parking meter': '停车计时器',
  'bench': '长椅',
  'bird': '鸟类',
  'cat': '猫',
  'dog': '狗',
  'horse': '马',
  'sheep': '羊',
  'cow': '牛',
  'elephant': '大象',
  'bear': '熊',
  'zebra': '斑马',
  'giraffe': '长颈鹿',
  'backpack': '背包',
  'umbrella': '雨伞',
  'handbag': '手提包',
  'tie': '领带',
  'suitcase': '行李箱',
  'frisbee': '飞盘',
  'skis': '滑雪板',
  'snowboard': '滑雪板',
  'sports ball': '运动球',
  'kite': '风筝',
  'baseball bat': '棒球棒',
  'baseball glove': '棒球手套',
  'skateboard': '滑板',
  'surfboard': '冲浪板',
  'tennis racket': '网球拍',
  'bottle': '瓶子',
  'wine glass': '酒杯',
  'cup': '杯子',
  'fork': '叉子',
  'knife': '刀',
  'spoon': '勺子',
  'bowl': '碗',
  'banana': '香蕉',
  'apple': '苹果',
  'sandwich': '三明治',
  'orange': '橙子',
  'broccoli': '西兰花',
  'carrot': '胡萝卜',
  'hot dog': '热狗',
  'pizza': '披萨',
  'donut': '甜甜圈',
  'cake': '蛋糕',
  'chair': '椅子',
  'couch': '沙发',
  'potted plant': '盆栽',
  'bed': '床',
  'dining table': '餐桌',
  'toilet': '马桶',
  'tv': '电视',
  'laptop': '笔记本电脑',
  'mouse': '鼠标',
  'remote': '遥控器',
  'keyboard': '键盘',
  'cell phone': '手机',
  'microwave': '微波炉',
  'oven': '烤箱',
  'toaster': '烤面包机',
  'sink': '水槽',
  'refrigerator': '冰箱',
  'book': '书',
  'clock': '时钟',
  'vase': '花瓶',
  'scissors': '剪刀',
  'teddy bear': '泰迪熊',
  'hair drier': '吹风机',
  'toothbrush': '牙刷'
}

// 类别分组
const categorySections = computed(() => [
  {
    name: '人员与车辆',
    categories: availableCategories.value.filter(cat => 
      ['person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat'].includes(cat)
    )
  },
  {
    name: '交通设施',
    categories: availableCategories.value.filter(cat => 
      ['traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench'].includes(cat)
    )
  },
  {
    name: '动物',
    categories: availableCategories.value.filter(cat => 
      ['bird', 'cat', 'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe'].includes(cat)
    )
  },
  {
    name: '日用品',
    categories: availableCategories.value.filter(cat => 
      ['backpack', 'umbrella', 'handbag', 'tie', 'suitcase', 'bottle', 'wine glass', 'cup'].includes(cat)
    )
  },
  {
    name: '运动用品',
    categories: availableCategories.value.filter(cat => 
      ['frisbee', 'skis', 'snowboard', 'sports ball', 'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket'].includes(cat)
    )
  },
  {
    name: '餐具与食物',
    categories: availableCategories.value.filter(cat => 
      ['fork', 'knife', 'spoon', 'bowl', 'banana', 'apple', 'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake'].includes(cat)
    )
  },
  {
    name: '家具',
    categories: availableCategories.value.filter(cat => 
      ['chair', 'couch', 'potted plant', 'bed', 'dining table', 'toilet'].includes(cat)
    )
  },
  {
    name: '电子设备',
    categories: availableCategories.value.filter(cat => 
      ['tv', 'laptop', 'mouse', 'remote', 'keyboard', 'cell phone', 'microwave', 'oven', 'toaster'].includes(cat)
    )
  },
  {
    name: '其他物品',
    categories: availableCategories.value.filter(cat => 
      !['person', 'bicycle', 'car', 'motorcycle', 'airplane', 'bus', 'train', 'truck', 'boat',
        'traffic light', 'fire hydrant', 'stop sign', 'parking meter', 'bench',
        'bird', 'cat', 'dog', 'horse', 'sheep', 'cow', 'elephant', 'bear', 'zebra', 'giraffe',
        'backpack', 'umbrella', 'handbag', 'tie', 'suitcase', 'bottle', 'wine glass', 'cup',
        'frisbee', 'skis', 'snowboard', 'sports ball', 'kite', 'baseball bat', 'baseball glove', 'skateboard', 'surfboard', 'tennis racket',
        'fork', 'knife', 'spoon', 'bowl', 'banana', 'apple', 'sandwich', 'orange', 'broccoli', 'carrot', 'hot dog', 'pizza', 'donut', 'cake',
        'chair', 'couch', 'potted plant', 'bed', 'dining table', 'toilet',
        'tv', 'laptop', 'mouse', 'remote', 'keyboard', 'cell phone', 'microwave', 'oven', 'toaster'].includes(cat)
    )
  }
])

// 计算属性
const hasChanges = computed(() => {
  if (originalCategories.value.length !== selectedCategories.value.length) {
    return true
  }
  return !originalCategories.value.every(cat => selectedCategories.value.includes(cat))
})

// 方法
const getCategoryDisplayName = (category) => {
  return categoryDisplayNames[category] || category
}

const loadCategories = async () => {
  try {
    loading.value = true
    
    // 加载可用类别
    const availableResponse = await apiService.getAvailableCategories()
    availableCategories.value = availableResponse.data.available_categories || []
    
    // 加载已启用类别
    const enabledResponse = await apiService.getDetectionCategories()
    selectedCategories.value = enabledResponse.data.enabled_categories || []
    originalCategories.value = [...selectedCategories.value]
    
    console.log('Categories loaded:', {
      available: availableCategories.value.length,
      enabled: selectedCategories.value.length
    })
    
  } catch (error) {
    console.error('Failed to load categories:', error)
    ElMessage.error('加载检测类别失败: ' + (error.response?.data?.message || error.message))
  } finally {
    loading.value = false
  }
}

const saveCategories = async () => {
  try {
    saving.value = true
    
    await apiService.updateDetectionCategories({
      enabled_categories: selectedCategories.value
    })
    
    originalCategories.value = [...selectedCategories.value]
    ElMessage.success('检测类别设置已保存并生效')
    
  } catch (error) {
    console.error('Failed to save categories:', error)
    ElMessage.error('保存失败: ' + (error.response?.data?.message || error.message))
  } finally {
    saving.value = false
  }
}

const refreshCategories = () => {
  loadCategories()
}

const resetCategories = () => {
  selectedCategories.value = [...originalCategories.value]
}

const selectAll = () => {
  selectedCategories.value = [...availableCategories.value]
}

const selectNone = () => {
  selectedCategories.value = []
}

const selectCommon = () => {
  // 选择常用的检测类别
  const commonCategories = ['person', 'car', 'truck', 'bicycle', 'motorcycle', 'bus', 'train']
  selectedCategories.value = commonCategories.filter(cat => availableCategories.value.includes(cat))
}

const onCategoryChange = (value) => {
  // 类别变化时的处理
  console.log('Categories changed:', value.length, 'selected')
}

onMounted(() => {
  loadCategories()
})
</script>

<style scoped>
.detection-category-filter {
  width: 100%;
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

.loading-state {
  padding: 20px;
}

.category-content {
  display: flex;
  flex-direction: column;
  gap: 20px;
}

.quick-select {
  display: flex;
  justify-content: flex-start;
}

.category-grid {
  max-height: 400px;
  overflow-y: auto;
  border: 1px solid var(--el-border-color);
  border-radius: 4px;
  padding: 16px;
}

.category-section {
  margin-bottom: 20px;
}

.section-title {
  margin: 0 0 12px 0;
  font-size: 14px;
  font-weight: 600;
  color: var(--el-text-color-primary);
  border-bottom: 1px solid var(--el-border-color-light);
  padding-bottom: 8px;
}

.category-items {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(150px, 1fr));
  gap: 8px;
}

.category-item {
  margin: 0;
}

.category-stats {
  background: var(--el-bg-color-page);
  padding: 16px;
  border-radius: 4px;
}

.action-buttons {
  display: flex;
  gap: 12px;
  justify-content: flex-start;
}

.preview-section {
  margin-top: 16px;
}
</style>
