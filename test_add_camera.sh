#!/bin/bash

echo "=== AI安全视觉系统 - 添加摄像头测试 ==="
echo ""

# 测试添加摄像头的API端点
API_BASE="http://localhost:8080"

# 函数：添加摄像头
add_camera() {
    local camera_id=$1
    local camera_name=$2
    local rtsp_url=$3
    local mjpeg_port=$4
    
    echo "📹 添加摄像头: $camera_name (ID: $camera_id)"
    echo "   RTSP地址: $rtsp_url"
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
        echo "   响应: $response_body" | python3 -m json.tool 2>/dev/null || echo "   响应: $response_body"
    else
        echo "   ❌ 添加失败 (HTTP $http_code)"
        echo "   错误: $response_body"
    fi
    echo ""
}

# 函数：查看所有摄像头
list_cameras() {
    echo "📋 当前摄像头列表:"
    response=$(curl -s "$API_BASE/api/cameras")
    echo "$response" | python3 -m json.tool 2>/dev/null || echo "$response"
    echo ""
}

# 函数：删除所有摄像头（用于测试）
clear_cameras() {
    echo "🗑️  清空所有摄像头配置..."
    
    # 获取当前摄像头列表
    cameras=$(curl -s "$API_BASE/api/cameras" | python3 -c "
import sys, json
try:
    data = json.load(sys.stdin)
    cameras = data.get('cameras', [])
    for camera in cameras:
        print(camera['id'])
except:
    pass
" 2>/dev/null)
    
    if [ -n "$cameras" ]; then
        echo "   找到摄像头: $cameras"
        # 注意：当前API可能没有删除端点，这里只是示例
        echo "   (删除功能需要后端支持 DELETE /api/cameras/{id})"
    else
        echo "   没有找到摄像头配置"
    fi
    echo ""
}

# 主测试流程
echo "1. 检查API服务状态..."
status_response=$(curl -s "$API_BASE/api/system/status")
if [ $? -eq 0 ]; then
    echo "   ✅ API服务正常运行"
    echo "   状态: $status_response" | python3 -m json.tool 2>/dev/null || echo "   状态: $status_response"
else
    echo "   ❌ API服务无法访问"
    echo "   请确保后端服务运行在 $API_BASE"
    exit 1
fi
echo ""

echo "2. 查看当前摄像头配置..."
list_cameras

echo "3. 添加测试摄像头..."

# 添加第一个摄像头
add_camera "test_camera_1" "测试摄像头1 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8161

# 添加第二个摄像头
add_camera "test_camera_2" "测试摄像头2 (192.168.1.2)" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" 8162

# 尝试添加重复ID的摄像头（应该失败）
echo "4. 测试重复ID错误处理..."
add_camera "test_camera_1" "重复ID摄像头" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8163

echo "5. 查看最终摄像头配置..."
list_cameras

echo "6. 测试MJPEG流访问..."
echo "📡 测试MJPEG流是否可访问:"
echo "   摄像头1流地址: http://127.0.0.1:8161/stream.mjpg"
echo "   摄像头2流地址: http://127.0.0.1:8162/stream.mjpg"

# 测试MJPEG流
for port in 8161 8162; do
    echo "   测试端口 $port..."
    response=$(curl -s --max-time 3 --head "http://127.0.0.1:$port/stream.mjpg")
    if echo "$response" | grep -q "200 OK"; then
        echo "     ✅ 端口 $port MJPEG流正常"
    else
        echo "     ❌ 端口 $port MJPEG流无法访问"
    fi
done

echo ""
echo "=== 测试完成 ==="
echo ""
echo "💡 提示:"
echo "   - 主要API端点: POST $API_BASE/api/cameras"
echo "   - 备用API端点: POST $API_BASE/api/source/add"
echo "   - 查看摄像头: GET $API_BASE/api/cameras"
echo "   - 前端界面: http://localhost:3001"
echo "   - MJPEG流格式: http://127.0.0.1:{mjpeg_port}/stream.mjpg"
