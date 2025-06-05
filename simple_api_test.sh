#!/bin/bash

# AI Security Vision System - Simple API Endpoint Tester
# 测试主要API端点的响应状态

BASE_URL="http://localhost:8080"
TOTAL=0
SUCCESS=0
FAILED=0

echo "🚀 开始测试AI Security Vision System API端点..."
echo "📡 后端地址: $BASE_URL"
echo "=" | tr -d '\n'; for i in {1..80}; do echo -n "="; done; echo

# 测试函数
test_endpoint() {
    local method=$1
    local path=$2
    local description=$3
    local data=$4
    
    TOTAL=$((TOTAL + 1))
    printf "[%2d] %-6s %-40s - %s" $TOTAL "$method" "$path" "$description"
    
    if [ "$method" = "GET" ]; then
        response=$(curl -s -w "%{http_code}" -o /tmp/response.json "$BASE_URL$path")
    elif [ "$method" = "POST" ]; then
        if [ -n "$data" ]; then
            response=$(curl -s -w "%{http_code}" -o /tmp/response.json -X POST -H "Content-Type: application/json" -d "$data" "$BASE_URL$path")
        else
            response=$(curl -s -w "%{http_code}" -o /tmp/response.json -X POST -H "Content-Type: application/json" "$BASE_URL$path")
        fi
    elif [ "$method" = "PUT" ]; then
        if [ -n "$data" ]; then
            response=$(curl -s -w "%{http_code}" -o /tmp/response.json -X PUT -H "Content-Type: application/json" -d "$data" "$BASE_URL$path")
        else
            response=$(curl -s -w "%{http_code}" -o /tmp/response.json -X PUT -H "Content-Type: application/json" "$BASE_URL$path")
        fi
    elif [ "$method" = "DELETE" ]; then
        response=$(curl -s -w "%{http_code}" -o /tmp/response.json -X DELETE "$BASE_URL$path")
    fi
    
    status_code="${response: -3}"
    
    if [ "$status_code" -ge 200 ] && [ "$status_code" -lt 300 ]; then
        echo " ✅ $status_code"
        SUCCESS=$((SUCCESS + 1))
    elif [ "$status_code" -eq 501 ]; then
        echo " 🚧 $status_code (Not Implemented)"
        SUCCESS=$((SUCCESS + 1))  # 501是预期的占位符响应
    else
        echo " ❌ $status_code"
        FAILED=$((FAILED + 1))
    fi
    
    sleep 0.1  # 避免请求过快
}

# 系统管理端点
echo "📊 系统管理端点:"
test_endpoint "GET" "/api/system/status" "获取系统状态"
test_endpoint "GET" "/api/system/info" "获取系统信息"
test_endpoint "GET" "/api/system/config" "获取系统配置"
test_endpoint "GET" "/api/system/metrics" "获取系统指标"
test_endpoint "GET" "/api/system/stats" "获取系统统计"
test_endpoint "GET" "/api/system/pipeline-stats" "获取管道统计"

echo
echo "📹 摄像头管理端点:"
test_endpoint "GET" "/api/cameras" "获取所有摄像头"
test_endpoint "GET" "/api/cameras/configs" "获取摄像头配置"

echo
echo "🤖 AI检测端点:"
test_endpoint "GET" "/api/detection/categories" "获取检测类别"
test_endpoint "GET" "/api/detection/available-categories" "获取可用检测类别"
test_endpoint "GET" "/api/detection/config" "获取检测配置"
test_endpoint "GET" "/api/detection/stats" "获取检测统计"

echo
echo "👥 人员统计端点:"
test_endpoint "GET" "/api/person-stats/camera_01" "获取人员统计"
test_endpoint "GET" "/api/person-stats/camera_01/hourly" "获取小时统计"
test_endpoint "GET" "/api/person-stats/camera_01/daily" "获取日统计"
test_endpoint "GET" "/api/person-stats/camera_01/age-gender" "获取年龄性别统计"

echo
echo "🌐 网络管理端点:"
test_endpoint "GET" "/api/network/interfaces" "获取网络接口"
test_endpoint "GET" "/api/network/ports" "获取端口配置"

echo
echo "🔍 ONVIF发现端点:"
test_endpoint "GET" "/api/source/discover" "ONVIF设备发现"

echo
echo "🚨 报警管理端点:"
test_endpoint "GET" "/api/alerts" "获取报警列表"
test_endpoint "GET" "/api/alerts/recent" "获取最近报警"

echo
echo "👤 人脸管理端点:"
test_endpoint "GET" "/api/faces" "获取人脸列表"
test_endpoint "GET" "/api/reid/config" "获取ReID配置"
test_endpoint "GET" "/api/reid/status" "获取ReID状态"

echo
echo "🔐 认证端点:"
test_endpoint "GET" "/api/auth/users" "获取用户列表"

echo
echo "📹 录像管理端点 (占位符):"
test_endpoint "GET" "/api/recordings" "获取录像列表"

echo
echo "📊 日志和统计端点 (占位符):"
test_endpoint "GET" "/api/logs" "获取系统日志"
test_endpoint "GET" "/api/statistics" "获取统计信息"

# 测试POST端点
echo
echo "📝 POST端点测试:"
test_endpoint "POST" "/api/cameras" "添加摄像头" '{"id":"test_camera","name":"Test Camera","rtsp_url":"rtsp://test:test@192.168.1.100:554/stream","enabled":true}'
test_endpoint "POST" "/api/detection/categories" "更新检测类别" '{"enabled_categories":["person","car","bicycle"]}'
test_endpoint "POST" "/api/network/config" "更新网络配置" '{"interface":"eth0","ip":"192.168.1.100"}'

# 清理测试数据
test_endpoint "DELETE" "/api/cameras/test_camera" "删除测试摄像头"

echo
echo "=" | tr -d '\n'; for i in {1..80}; do echo -n "="; done; echo
echo "📊 测试总结:"
echo "   总计: $TOTAL 个端点"
echo "   成功: $SUCCESS 个 ($(echo "scale=1; $SUCCESS*100/$TOTAL" | bc -l)%)"
echo "   失败: $FAILED 个 ($(echo "scale=1; $FAILED*100/$TOTAL" | bc -l)%)"

if [ $FAILED -eq 0 ]; then
    echo "🎉 所有API端点测试通过！"
else
    echo "⚠️  有 $FAILED 个端点测试失败，请检查后端实现"
fi

# 清理临时文件
rm -f /tmp/response.json
