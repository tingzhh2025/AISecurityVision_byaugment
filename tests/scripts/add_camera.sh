#!/bin/bash

echo "Adding 4 RTSP cameras for person statistics and age/gender recognition testing..."
echo "Testing with 2 different camera sources, each added twice for comprehensive analysis"

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

# Add 4 cameras - 2 different sources, each added twice for comprehensive testing
echo "=== Adding Camera Source 1 (192.168.1.3) - Two instances ==="
add_camera "camera_01" "Person Stats Camera 1A (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8161
add_camera "camera_02" "Person Stats Camera 1B (192.168.1.3)" "rtsp://admin:sharpi1688@192.168.1.3:554/1/1" 8162

echo "=== Adding Camera Source 2 (192.168.1.2) - Two instances ==="
add_camera "camera_03" "Age/Gender Camera 2A (192.168.1.2)" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" 8163
add_camera "camera_04" "Age/Gender Camera 2B (192.168.1.2)" "rtsp://admin:sharpi1688@192.168.1.2:554/1/1" 8164

echo ""
echo "🎯 Person Statistics & Age/Gender Testing Setup Complete!"
echo "📊 4 RTSP cameras added for comprehensive AI analysis testing"
echo ""
echo "📺 MJPEG streams available at:"
echo "  Camera 1A (192.168.1.3): http://localhost:8161"
echo "  Camera 1B (192.168.1.3): http://localhost:8162"
echo "  Camera 2A (192.168.1.2): http://localhost:8163"
echo "  Camera 2B (192.168.1.2): http://localhost:8164"
echo ""
echo "🧠 AI Analysis Features:"
echo "  ✅ YOLOv8 RKNN Person Detection"
echo "  ✅ InsightFace Age/Gender Recognition"
echo "  ✅ ByteTracker Person Tracking"
echo "  ✅ Real-time Person Statistics"
echo ""
echo "📈 System monitoring:"
echo "  Status API: http://localhost:8080/api/status"
echo "  Cameras API: http://localhost:8080/api/cameras"
echo "  Person Stats API: http://localhost:8080/api/person-stats"
echo "  Frontend UI: http://localhost:3000"
echo ""
echo "🔍 Testing Focus Areas:"
echo "  - Person detection accuracy across different camera angles"
echo "  - Age/gender recognition performance"
echo "  - Person counting and statistics aggregation"
echo "  - Multi-camera tracking consistency"
echo "  - InsightFace inference performance (~15-35ms per person)"
echo "  - RKNN YOLOv8 detection speed (~14-26ms per frame)"
echo ""
echo "📊 Expected Analytics:"
echo "  - Real-time person count per camera"
echo "  - Age distribution (9 categories: 0-2, 3-9, 10-19, 20-29, 30-39, 40-49, 50-59, 60-69, 70+)"
echo "  - Gender distribution (Male/Female)"
echo "  - Person tracking across multiple cameras"
echo "  - Quality assessment and filtering"
