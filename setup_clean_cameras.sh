#!/bin/bash

echo "=== AI安全视觉系统 - 清理并重新配置摄像头 ==="
echo ""

API_BASE="http://localhost:8080"

# 函数：添加摄像头
add_camera() {
    local camera_id=$1
    local camera_name=$2
    local rtsp_url=$3
    local mjpeg_port=$4
    
    echo "📹 添加摄像头: $camera_name"
    echo "   ID: $camera_id"
    echo "   RTSP: $rtsp_url"
    echo "   MJPEG端口: $mjpeg_port"
    
    response=$(curl -s -w "\n%{http_code}" -X POST "$API_BASE/api/cameras" \
        -H "Content-Type: application/json" \
        -d "{
            \"id\": \"$camera_id\",
            \"name\": \"$camera_name\",
            \"url\": \"$rtsp_url\",
            \"protocol\": \"rtsp\",
            \"username\": \"admin\",
            \"password\": \"sharpi1688\",
            \"width\": 1920,
            \"height\": 1080,
            \"fps\": 25,
            \"mjpeg_port\": $mjpeg_port,
            \"enabled\": true
        }")
    
    # 分离响应体和状态码
    http_code=$(echo "$response" | tail -n1)
    response_body=$(echo "$response" | head -n -1)
    
    if [ "$http_code" = "201" ]; then
        echo "   ✅ 添加成功!"
    else
        echo "   ❌ 添加失败 (HTTP $http_code)"
        echo "   错误: $response_body"
    fi
    echo ""
}

# 函数：查看摄像头列表
list_cameras() {
    echo "📋 当前摄像头配置:"
    response=$(curl -s "$API_BASE/api/cameras")
    echo "$response" | python3 -m json.tool 2>/dev/null || echo "$response"
    echo ""
}

# 函数：测试MJPEG流
test_mjpeg_streams() {
    echo "🎥 测试MJPEG流访问:"
    
    for port in 8161 8162 8163 8164; do
        echo "   测试端口 $port..."
        response=$(curl -s --max-time 3 --head "http://127.0.0.1:$port/stream.mjpg")
        if echo "$response" | grep -q "200 OK"; then
            echo "     ✅ 端口 $port MJPEG流正常"
        else
            echo "     ❌ 端口 $port MJPEG流无法访问"
        fi
    done
    echo ""
}

# 主流程
echo "1. 检查API服务状态..."
status_response=$(curl -s "$API_BASE/api/system/status")
if [ $? -eq 0 ]; then
    echo "   ✅ API服务正常运行"
else
    echo "   ❌ API服务无法访问"
    echo "   请确保后端服务运行在 $API_BASE"
    exit 1
fi
echo ""

echo "2. 查看当前摄像头配置..."
list_cameras

echo "3. 添加新的摄像头配置（使用不同端口）..."

# 添加4个摄像头，每个使用不同的MJPEG端口
add_camera "camera_01" "主摄像头 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8161
add_camera "camera_02" "副摄像头 (192.168.1.2)" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" 8162
add_camera "camera_03" "备用摄像头1 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8163
add_camera "camera_04" "备用摄像头2 (192.168.1.2)" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" 8164

echo "4. 查看最终摄像头配置..."
list_cameras

echo "5. 等待摄像头初始化..."
echo "   等待10秒让摄像头完全初始化..."
sleep 10

echo "6. 测试MJPEG流..."
test_mjpeg_streams

echo "=== 配置完成 ==="
echo ""
echo "💡 摄像头配置信息:"
echo "   camera_01: http://127.0.0.1:8161/stream.mjpg"
echo "   camera_02: http://127.0.0.1:8162/stream.mjpg"
echo "   camera_03: http://127.0.0.1:8163/stream.mjpg"
echo "   camera_04: http://127.0.0.1:8164/stream.mjpg"
echo ""
echo "🌐 前端界面: http://localhost:3001"
echo "🔧 API服务: http://localhost:8080"
