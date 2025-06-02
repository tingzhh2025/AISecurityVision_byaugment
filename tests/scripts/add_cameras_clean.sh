#!/bin/bash

# Function to add a camera with better error handling
add_camera() {
    local id="$1"
    local name="$2"
    local url="$3"
    local port="$4"
    
    echo "=== Adding camera: $id ($name) on port $port ==="
    
    # Add camera with timeout and error handling
    response=$(curl --noproxy localhost,127.0.0.1 -X POST http://localhost:8080/api/cameras \
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
      }" --max-time 30 -w "%{http_code}" -s)
    
    if [[ "$response" == *"200"* ]] || [[ "$response" == *"201"* ]]; then
        echo "✅ Successfully added $id"
    else
        echo "❌ Failed to add $id (Response: $response)"
    fi
    
    echo "Waiting 10 seconds for pipeline initialization..."
    sleep 10
}

# Check if API service is running
echo "🔍 Checking API service..."
if ! curl --noproxy localhost,127.0.0.1 -s http://localhost:8080/api/system/status > /dev/null; then
    echo "❌ Error: API service is not running on port 8080"
    exit 1
fi

echo "✅ API service is running. Starting to add cameras..."
echo ""

# Add 8 cameras - all using the working RTSP URL (192.168.1.3)
echo "🎥 Adding 8 cameras using rtsp://admin:sharpi1688@192.168.1.3:554/1/1"
echo ""

add_camera "camera_01" "RTSP Camera 1" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8161
add_camera "camera_02" "RTSP Camera 2" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8162
add_camera "camera_03" "RTSP Camera 3" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8163
add_camera "camera_04" "RTSP Camera 4" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8164
add_camera "camera_05" "RTSP Camera 5" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8165
add_camera "camera_06" "RTSP Camera 6" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8166
add_camera "camera_07" "RTSP Camera 7" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8167
add_camera "camera_08" "RTSP Camera 8" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8168

echo ""
echo "🎉 All cameras added!"
echo ""
echo "📺 MJPEG streams available at:"
echo "  Camera 1: http://localhost:8161/stream.mjpg"
echo "  Camera 2: http://localhost:8162/stream.mjpg"
echo "  Camera 3: http://localhost:8163/stream.mjpg"
echo "  Camera 4: http://localhost:8164/stream.mjpg"
echo "  Camera 5: http://localhost:8165/stream.mjpg"
echo "  Camera 6: http://localhost:8166/stream.mjpg"
echo "  Camera 7: http://localhost:8167/stream.mjpg"
echo "  Camera 8: http://localhost:8168/stream.mjpg"
echo ""
echo "🌐 Frontend available at: http://localhost:3000"
