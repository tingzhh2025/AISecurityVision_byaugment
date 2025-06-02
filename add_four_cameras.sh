#!/bin/bash

echo "Adding 4 RTSP cameras to the system..."

# Camera 1: 192.168.1.3:554 (Port 8161)
echo "Adding Camera 1..."
curl -X POST http://localhost:8080/api/cameras \
  -H "Content-Type: application/json" \
  -d '{
    "id": "camera_01",
    "name": "RTSP Camera 1 (192.168.1.3)",
    "url": "rtsp://admin:sharpi1688@192.168.1.3:554/1/1",
    "protocol": "rtsp",
    "mjpeg_port": 8161,
    "enabled": true,
    "detection_enabled": true,
    "recording_enabled": false
  }'
echo ""

sleep 2

# Camera 2: 192.168.1.3:554 (Port 8162)
echo "Adding Camera 2..."
curl -X POST http://localhost:8080/api/cameras \
  -H "Content-Type: application/json" \
  -d '{
    "id": "camera_02",
    "name": "RTSP Camera 2 (192.168.1.3)",
    "url": "rtsp://admin:sharpi1688@192.168.1.3:554/1/1",
    "protocol": "rtsp",
    "mjpeg_port": 8162,
    "enabled": true,
    "detection_enabled": true,
    "recording_enabled": false
  }'
echo ""

sleep 2

# Camera 3: 192.168.1.2:554 (Port 8163)
echo "Adding Camera 3..."
curl -X POST http://localhost:8080/api/cameras \
  -H "Content-Type: application/json" \
  -d '{
    "id": "camera_03",
    "name": "RTSP Camera 3 (192.168.1.2)",
    "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
    "protocol": "rtsp",
    "mjpeg_port": 8163,
    "enabled": true,
    "detection_enabled": true,
    "recording_enabled": false
  }'
echo ""

sleep 2

# Camera 4: 192.168.1.2:554 (Port 8164)
echo "Adding Camera 4..."
curl -X POST http://localhost:8080/api/cameras \
  -H "Content-Type: application/json" \
  -d '{
    "id": "camera_04",
    "name": "RTSP Camera 4 (192.168.1.2)",
    "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
    "protocol": "rtsp",
    "mjpeg_port": 8164,
    "enabled": true,
    "detection_enabled": true,
    "recording_enabled": false
  }'
echo ""

echo "Checking cameras list..."
curl -X GET http://localhost:8080/api/cameras

echo ""
echo "All cameras added successfully!"
