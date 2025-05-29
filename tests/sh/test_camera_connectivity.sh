#!/bin/bash

echo "=== Camera Connectivity Test ==="
echo "Testing connectivity to real RTSP cameras"
echo

# Camera configurations
CAMERA1_IP="192.168.1.2"
CAMERA2_IP="192.168.1.3"
RTSP_PORT="554"
USERNAME="admin"
PASSWORD="sharpi1688"

echo "1. Testing network connectivity..."
echo "Pinging Camera 1 ($CAMERA1_IP):"
ping -c 3 $CAMERA1_IP

echo
echo "Pinging Camera 2 ($CAMERA2_IP):"
ping -c 3 $CAMERA2_IP

echo
echo "2. Testing RTSP port connectivity..."
echo "Testing Camera 1 RTSP port:"
timeout 5 nc -zv $CAMERA1_IP $RTSP_PORT

echo
echo "Testing Camera 2 RTSP port:"
timeout 5 nc -zv $CAMERA2_IP $RTSP_PORT

echo
echo "3. Testing RTSP stream with FFmpeg (if available)..."
if command -v ffmpeg &> /dev/null; then
    echo "Testing Camera 1 RTSP stream:"
    timeout 10 ffmpeg -rtsp_transport tcp -i "rtsp://$USERNAME:$PASSWORD@$CAMERA1_IP:$RTSP_PORT/1/1" -t 1 -f null - 2>&1 | head -20
    
    echo
    echo "Testing Camera 2 RTSP stream:"
    timeout 10 ffmpeg -rtsp_transport tcp -i "rtsp://$USERNAME:$PASSWORD@$CAMERA2_IP:$RTSP_PORT/1/1" -t 1 -f null - 2>&1 | head -20
else
    echo "FFmpeg not available for RTSP testing"
fi

echo
echo "4. Testing with curl (HTTP check)..."
echo "Testing Camera 1 HTTP interface:"
timeout 5 curl -u "$USERNAME:$PASSWORD" "http://$CAMERA1_IP/" 2>&1 | head -5

echo
echo "Testing Camera 2 HTTP interface:"
timeout 5 curl -u "$USERNAME:$PASSWORD" "http://$CAMERA2_IP/" 2>&1 | head -5

echo
echo "5. Network route information..."
echo "Route to Camera 1:"
ip route get $CAMERA1_IP

echo
echo "Route to Camera 2:"
ip route get $CAMERA2_IP

echo
echo "6. Local network interface information..."
ip addr show | grep -E "(inet|UP|DOWN)"

echo
echo "=== Connectivity Test Complete ==="
echo "If cameras are not reachable, please check:"
echo "1. Camera power and network connection"
echo "2. Network configuration (IP addresses, subnet)"
echo "3. Firewall settings"
echo "4. Camera credentials (username/password)"
echo "5. RTSP stream path (/1/1)"
