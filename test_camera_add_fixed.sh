#!/bin/bash

# Test script for adding cameras with fixed port allocation

API_BASE="http://localhost:8080/api"

echo "=== Testing Camera Addition with Fixed Port Allocation ==="

# Test 1: Add first camera
echo "Adding first camera (ch2)..."
curl -X POST "${API_BASE}/cameras" \
  -H "Content-Type: application/json" \
  -d '{
    "id": "camera_ch2",
    "name": "ch2",
    "url": "rtsp://admin:sharpi1688@192.168.1.2:554/1/1",
    "protocol": "rtsp",
    "username": "admin",
    "password": "sharpi1688",
    "width": 1920,
    "height": 1080,
    "fps": 25,
    "mjpeg_port": 8161,
    "enabled": true
  }' \
  -w "\nHTTP Status: %{http_code}\nTime: %{time_total}s\n\n"

echo "Waiting 5 seconds for pipeline initialization..."
sleep 5

# Test 2: Add second camera
echo "Adding second camera (ch3)..."
curl -X POST "${API_BASE}/cameras" \
  -H "Content-Type: application/json" \
  -d '{
    "id": "camera_ch3",
    "name": "ch3", 
    "url": "rtsp://admin:sharpi1688@192.168.1.3:554/1/1",
    "protocol": "rtsp",
    "username": "admin",
    "password": "sharpi1688",
    "width": 1920,
    "height": 1080,
    "fps": 25,
    "mjpeg_port": 8162,
    "enabled": true
  }' \
  -w "\nHTTP Status: %{http_code}\nTime: %{time_total}s\n\n"

echo "Waiting 5 seconds for pipeline initialization..."
sleep 5

# Test 3: Check camera list
echo "Checking camera list..."
curl -X GET "${API_BASE}/cameras" \
  -H "Content-Type: application/json" \
  -w "\nHTTP Status: %{http_code}\nTime: %{time_total}s\n\n"

echo "=== Test Complete ==="
