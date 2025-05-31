#!/bin/bash

echo "Adding 8 RTSP cameras for stress testing AI Security Vision System..."
echo "This will test system performance with multiple concurrent streams"

# Function to add a camera
add_camera() {
    local id=$1
    local name=$2
    local url=$3
    local port=$4

    echo "Adding $name (ID: $id) on port $port..."

    curl --noproxy localhost,127.0.0.1 -X POST http://localhost:8080/api/cameras \
      -H "Content-Type: application/json" \
      -d "{
        \"id\": \"$id\",
        \"name\": \"$name\",
        \"url\": \"$url\",
        \"protocol\": \"rtsp\",
        \"username\": \"admin\",
        \"password\": \"sharpi1688\",
        \"width\": 1920,
        \"height\": 1080,
        \"fps\": 25,
        \"mjpeg_port\": $port,
        \"enabled\": true
      }" && echo

    sleep 1  # Small delay between requests
}

# Add 8 cameras - all using the working RTSP URL (192.168.1.3)
echo "=== Adding cameras 1-4 (all using 192.168.1.3) ==="
add_camera "camera_01" "RTSP Camera 1 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8161
add_camera "camera_02" "RTSP Camera 2 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8162
add_camera "camera_03" "RTSP Camera 3 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8163
add_camera "camera_04" "RTSP Camera 4 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8164

echo "=== Adding cameras 5-8 (all using 192.168.1.3) ==="
add_camera "camera_05" "RTSP Camera 5 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8165
add_camera "camera_06" "RTSP Camera 6 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8166
add_camera "camera_07" "RTSP Camera 7 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8167
add_camera "camera_08" "RTSP Camera 8 (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8168

echo ""
echo "🎯 Stress Test Setup Complete!"
echo "📊 8 RTSP cameras added for performance testing"
echo ""
echo "📺 MJPEG streams available at:"
echo "  Camera 1: http://localhost:8161"
echo "  Camera 2: http://localhost:8162"
echo "  Camera 3: http://localhost:8163"
echo "  Camera 4: http://localhost:8164"
echo "  Camera 5: http://localhost:8165"
echo "  Camera 6: http://localhost:8166"
echo "  Camera 7: http://localhost:8167"
echo "  Camera 8: http://localhost:8168"
echo ""
echo "📈 System monitoring:"
echo "  Status API: http://localhost:8080/api/status"
echo "  Cameras API: http://localhost:8080/api/cameras"
echo ""
echo "⚡ Performance metrics to monitor:"
echo "  - RKNN inference time per camera"
echo "  - Total system CPU/NPU usage"
echo "  - Memory consumption"
echo "  - Frame processing rate"
