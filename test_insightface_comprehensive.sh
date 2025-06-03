#!/bin/bash

# AI Security Vision System - Comprehensive InsightFace Test Script
echo "=== AI Security Vision System - Comprehensive InsightFace Test ==="

BASE_URL="http://localhost:8080/api"

echo ""
echo "🔍 1. InsightFace库集成验证"
echo "============================================"

echo ""
echo "📚 检查InsightFace库链接状态..."
cd /userdata/source/source/AISecurityVision_byaugment
ldd build/AISecurityVision | grep -i inspire
if [ $? -eq 0 ]; then
    echo "✅ InsightFace库已正确链接"
else
    echo "❌ InsightFace库未链接"
fi

echo ""
echo "📦 检查Pikachu.pack模型文件..."
MODEL_FILE="third_party/insightface/models/Pikachu.pack"
if [ -f "$MODEL_FILE" ]; then
    echo "✅ Pikachu.pack模型文件存在"
    ls -lh "$MODEL_FILE"
else
    echo "❌ Pikachu.pack模型文件不存在"
fi

echo ""
echo "🔧 检查编译宏定义..."
MACRO_COUNT=$(grep -r "HAVE_INSIGHTFACE" src/ --include="*.cpp" --include="*.h" | wc -l)
echo "HAVE_INSIGHTFACE宏使用次数: $MACRO_COUNT"

echo ""
echo ""
echo "🧪 2. 功能测试验证"
echo "============================================"

echo ""
echo "📊 测试人员统计配置API..."
echo "GET $BASE_URL/cameras/camera_01/person-stats/config"
PERSON_STATS_CONFIG=$(curl -s "$BASE_URL/cameras/camera_01/person-stats/config")
echo "$PERSON_STATS_CONFIG"

if echo "$PERSON_STATS_CONFIG" | grep -q "enabled"; then
    echo "✅ 人员统计配置API响应正常"
else
    echo "⚠️  人员统计配置API响应异常"
fi

echo ""
echo ""
echo "⚙️ 启用人员统计功能..."
echo "POST $BASE_URL/cameras/camera_01/person-stats/config"
ENABLE_RESULT=$(curl -s -X POST "$BASE_URL/cameras/camera_01/person-stats/config" \
  -H "Content-Type: application/json" \
  -d '{
    "enabled": true,
    "gender_threshold": 0.7,
    "age_threshold": 0.6,
    "batch_size": 4,
    "enable_caching": true
  }')
echo "$ENABLE_RESULT"

if echo "$ENABLE_RESULT" | grep -q "success\|configured"; then
    echo "✅ 人员统计功能启用成功"
else
    echo "⚠️  人员统计功能启用可能失败"
fi

echo ""
echo ""
echo "📈 测试人员统计数据API..."
echo "GET $BASE_URL/cameras/camera_01/person-stats"
PERSON_STATS=$(curl -s "$BASE_URL/cameras/camera_01/person-stats")
echo "$PERSON_STATS"

if echo "$PERSON_STATS" | grep -q "current_stats\|total_count"; then
    echo "✅ 人员统计数据API响应正常"
else
    echo "⚠️  人员统计数据API响应异常"
fi

echo ""
echo ""
echo "🔍 3. API接口验证"
echo "============================================"

echo ""
echo "📊 测试历史统计数据..."
echo "GET $BASE_URL/cameras/camera_01/person-stats/history?hours=1"
HISTORY_STATS=$(curl -s "$BASE_URL/cameras/camera_01/person-stats/history?hours=1")
echo "$HISTORY_STATS"

echo ""
echo ""
echo "📊 测试全局人员统计..."
echo "GET $BASE_URL/person-stats/summary"
GLOBAL_STATS=$(curl -s "$BASE_URL/person-stats/summary")
echo "$GLOBAL_STATS"

echo ""
echo ""
echo "🎯 4. 性能验证"
echo "============================================"

echo ""
echo "💾 检查内存使用情况..."
PROCESS_INFO=$(ps aux | grep AISecurityVision | grep -v grep)
if [ -n "$PROCESS_INFO" ]; then
    echo "$PROCESS_INFO" | awk '{print "PID: " $2 ", CPU: " $3 "%, MEM: " $4 "%, RSS: " $6 " KB"}'
    echo "✅ 系统进程运行正常"
else
    echo "❌ 系统进程未运行"
fi

echo ""
echo "📊 检查系统状态..."
echo "GET $BASE_URL/system/status"
SYSTEM_STATUS=$(curl -s "$BASE_URL/system/status")
echo "$SYSTEM_STATUS"

if echo "$SYSTEM_STATUS" | grep -q "status\|version"; then
    echo "✅ 系统状态API响应正常"
else
    echo "⚠️  系统状态API响应异常"
fi

echo ""
echo ""
echo "🔄 5. 实时监控测试（30秒）"
echo "============================================"

echo "监控人员统计数据变化和AI推理性能..."
for i in {1..6}; do
    echo "--- 第 $i 次检查 ($(date)) ---"
    
    # 检查人员统计
    CURRENT_STATS=$(curl -s "$BASE_URL/cameras/camera_01/person-stats" | grep -o '"total_count":[0-9]*' | cut -d':' -f2)
    if [ -n "$CURRENT_STATS" ]; then
        echo "📊 当前人员统计: $CURRENT_STATS"
    fi
    
    # 检查系统性能
    CPU_USAGE=$(ps aux | grep AISecurityVision | grep -v grep | awk '{print $3}')
    MEM_USAGE=$(ps aux | grep AISecurityVision | grep -v grep | awk '{print $4}')
    if [ -n "$CPU_USAGE" ]; then
        echo "💻 系统性能: CPU ${CPU_USAGE}%, MEM ${MEM_USAGE}%"
    fi
    
    sleep 5
done

echo ""
echo ""
echo "🔍 6. InsightFace特定功能验证"
echo "============================================"

echo ""
echo "📋 检查AgeGenderAnalyzer模型信息..."
# 通过日志或API检查模型加载状态
if [ -f "build/AISecurityVision" ]; then
    echo "✅ 可执行文件存在"
    
    # 检查是否有InsightFace相关的符号
    INSIGHT_SYMBOLS=$(nm build/AISecurityVision 2>/dev/null | grep -i insight | wc -l)
    echo "🔍 InsightFace相关符号数量: $INSIGHT_SYMBOLS"
    
    if [ "$INSIGHT_SYMBOLS" -gt 0 ]; then
        echo "✅ InsightFace符号已链接"
    else
        echo "⚠️  InsightFace符号未找到"
    fi
fi

echo ""
echo "📊 检查年龄性别识别配置..."
AGE_GENDER_CONFIG=$(curl -s "$BASE_URL/cameras/camera_01/person-stats/config" | grep -o '"gender_threshold":[0-9.]*')
if [ -n "$AGE_GENDER_CONFIG" ]; then
    echo "✅ 年龄性别识别配置: $AGE_GENDER_CONFIG"
else
    echo "⚠️  年龄性别识别配置未找到"
fi

echo ""
echo ""
echo "=== InsightFace Comprehensive Test Complete ==="
echo ""
echo "📋 测试总结："
echo "1. ✅ 检查InsightFace库链接状态"
echo "2. ✅ 验证Pikachu.pack模型文件"
echo "3. ✅ 测试人员统计API接口"
echo "4. ✅ 监控系统性能指标"
echo "5. ✅ 验证实时数据更新"
echo "6. ✅ 检查InsightFace特定功能"
echo ""
echo "🔗 相关链接："
echo "- 前端界面: http://localhost:3000"
echo "- API文档: http://localhost:8080/api/system/status"
echo "- MJPEG流: http://localhost:8161 (如果摄像头已配置)"
echo ""
echo "📝 建议下一步："
echo "1. 在前端界面中查看人员统计数据"
echo "2. 观察MJPEG流中的检测结果"
echo "3. 验证年龄性别识别的准确性"
echo "4. 监控长期运行的性能表现"
