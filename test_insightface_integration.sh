#!/bin/bash

# AI Security Vision System - InsightFace Integration Test Script
echo "=== AI Security Vision System - InsightFace Integration Test ==="

BASE_URL="http://localhost:8080/api"

echo ""
echo "🔍 1. InsightFace库集成验证"
echo "============================================"

echo ""
echo "📚 检查InsightFace库链接状态..."
cd /userdata/source/source/AISecurityVision_byaugment/build
ldd ./AISecurityVision | grep -i insight
if [ $? -eq 0 ]; then
    echo "✅ InsightFace库已正确链接"
else
    echo "❌ InsightFace库未链接"
fi

echo ""
echo "📦 检查Pikachu.pack模型文件..."
MODEL_FILE="../third_party/insightface/models/Pikachu.pack"
if [ -f "$MODEL_FILE" ]; then
    echo "✅ Pikachu.pack模型文件存在"
    ls -lh "$MODEL_FILE"
else
    echo "❌ Pikachu.pack模型文件不存在"
fi

echo ""
echo "🔧 检查编译宏定义..."
grep -r "HAVE_INSIGHTFACE" ../src/ --include="*.cpp" --include="*.h" | wc -l
echo "HAVE_INSIGHTFACE宏使用次数: $(grep -r "HAVE_INSIGHTFACE" ../src/ --include="*.cpp" --include="*.h" | wc -l)"

echo ""
echo ""
echo "🧪 2. 功能测试验证"
echo "============================================"

echo ""
echo "📊 测试人员统计配置API..."
echo "GET $BASE_URL/cameras/camera_01/person-stats/config"
curl -s "$BASE_URL/cameras/camera_01/person-stats/config" | jq . 2>/dev/null || curl -s "$BASE_URL/cameras/camera_01/person-stats/config"

echo ""
echo ""
echo "⚙️ 启用人员统计功能..."
echo "POST $BASE_URL/cameras/camera_01/person-stats/config"
curl -s -X POST "$BASE_URL/cameras/camera_01/person-stats/config" \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "gender_threshold": 0.7,
    "age_threshold": 0.6,
    "batch_size": 4,
    "enable_caching": true
  }' | jq . 2>/dev/null || \
curl -s -X POST "$BASE_URL/cameras/camera_01/person-stats/config" \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "gender_threshold": 0.7,
    "age_threshold": 0.6,
    "batch_size": 4,
    "enable_caching": true
  }'

echo ""
echo ""
echo "📈 测试人员统计数据API..."
echo "GET $BASE_URL/cameras/camera_01/person-stats"
curl -s "$BASE_URL/cameras/camera_01/person-stats" | jq . 2>/dev/null || curl -s "$BASE_URL/cameras/camera_01/person-stats"

echo ""
echo ""
echo "🔍 3. API接口验证"
历史统计数据..."
echo "GET $BASE_URL/cameras/camera_01/person-stats/history?hours=1"
curl -s "$BASE_URL/cameras/camera_01/person-stats/history?hours=1" | jq . 2>/dev/null || curl -s "$BASE_URL/cameras/camera_01/person-stats/history?hours=1"

echo ""
echo ""
echo "📊 测试全局人员统计..."
echo "GET $BASE_URL/person-stats/summary"
curl -s "$BASE_URL/person-stats/summary" | jq . 2>/dev/null || curl -s "$BASE_URL/person-stats/summary"

echo ""
echo ""
echo "🎯 4. 性能验证"
echo "============================================"

echo ""
echo "💾 检查内存使用情况..."
ps aux | grep AISecurityVision | grep -v grep | awk '{print "PID: " $2 ", CPU: " $3 "%, MEM: " $4 "%, RSS: " $6 " KB"}'

echo ""
echo "📊 检查系统状态..."
echo "GET $BASE_URL/system/status"
curl -s "$BASE_URL/system/status" | jq . 2>/dev/null || curl -s "$BASE_URL/system/status"

echo ""
echo ""
echo "🔄 实时监控测试（10秒）..."
echo "监控人员统计数据变化..."
for i in {1..5}; do
    echo "--- 第 $i 次检查 ($(date)) ---"
    curl -s "$BASE_URL/cameras/camera_01/person-stats" | jq '.current_stats // .' 2>/dev/null || curl -s "$BASE_URL/cameras/camera_01/person-stats"
    sleep 2
done

echo ""
echo ""
echo "=== InsightFace Integration Test Complete ==="
echo ""
echo "📋 测试总结："
echo "1. ✅ 检查InsightFace库链接状态"
echo "2. ✅ 验证Pikachu.pack模型文件"
echo "3. ✅ 测试人员统计API接口"
echo "4. ✅ 监控系统性能指标"
echo ""
echo "🔗 相关链接："
echo "- 前端界面: http://localhost:3000"
echo "- API文档: http://localhost:8080/api/system/status"
echo "- MJPEG流: http://localhost:8161 (如果摄像头已配置)"
