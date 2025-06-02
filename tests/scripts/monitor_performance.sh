#!/bin/bash

echo "🔍 AI Security Vision System - Performance Monitor"
echo "=================================================="
echo "Monitoring 8-camera stress test performance..."
echo ""

# Function to get system stats
get_system_stats() {
    echo "📊 System Performance Metrics:"
    echo "------------------------------"
    
    # CPU usage
    cpu_usage=$(top -bn1 | grep "Cpu(s)" | awk '{print $2}' | cut -d'%' -f1)
    echo "🖥️  CPU Usage: ${cpu_usage}%"
    
    # Memory usage
    mem_info=$(free -h | grep "Mem:")
    mem_used=$(echo $mem_info | awk '{print $3}')
    mem_total=$(echo $mem_info | awk '{print $2}')
    mem_percent=$(free | grep "Mem:" | awk '{printf "%.1f", $3/$2 * 100.0}')
    echo "💾 Memory: ${mem_used}/${mem_total} (${mem_percent}%)"
    
    # NPU temperature (if available)
    if [ -f /sys/class/thermal/thermal_zone0/temp ]; then
        temp=$(cat /sys/class/thermal/thermal_zone0/temp)
        temp_c=$((temp / 1000))
        echo "🌡️  NPU Temperature: ${temp_c}°C"
    fi
    
    # Process info
    ai_process=$(ps aux | grep AISecurityVision | grep -v grep | head -1)
    if [ ! -z "$ai_process" ]; then
        pid=$(echo $ai_process | awk '{print $2}')
        cpu_proc=$(echo $ai_process | awk '{print $3}')
        mem_proc=$(echo $ai_process | awk '{print $4}')
        echo "🤖 AI Process: PID=$pid, CPU=${cpu_proc}%, MEM=${mem_proc}%"
    fi
    
    echo ""
}

# Function to get API status
get_api_status() {
    echo "📡 API Status Check:"
    echo "-------------------"
    
    # Check if API is responding
    if curl -s http://localhost:8080/api/status > /dev/null 2>&1; then
        echo "✅ API Server: Online"
        
        # Get camera count
        camera_count=$(curl -s http://localhost:8080/api/cameras 2>/dev/null | grep -o '"id"' | wc -l)
        echo "📹 Active Cameras: $camera_count"
        
    else
        echo "❌ API Server: Offline"
    fi
    echo ""
}

# Function to check MJPEG streams
check_mjpeg_streams() {
    echo "📺 MJPEG Stream Status:"
    echo "----------------------"
    
    for port in 8161 8162 8163 8164 8165 8166 8167 8168; do
        if curl -s --max-time 2 http://localhost:$port/stream.mjpg > /dev/null 2>&1; then
            echo "✅ Port $port: Stream Active"
        else
            echo "❌ Port $port: Stream Inactive"
        fi
    done
    echo ""
}

# Function to monitor network usage
get_network_stats() {
    echo "🌐 Network Statistics:"
    echo "---------------------"
    
    # Get network interface stats
    rx_bytes=$(cat /proc/net/dev | grep eth0 | awk '{print $2}')
    tx_bytes=$(cat /proc/net/dev | grep eth0 | awk '{print $10}')
    
    if [ ! -z "$rx_bytes" ] && [ ! -z "$tx_bytes" ]; then
        rx_mb=$((rx_bytes / 1024 / 1024))
        tx_mb=$((tx_bytes / 1024 / 1024))
        echo "📥 RX: ${rx_mb} MB"
        echo "📤 TX: ${tx_mb} MB"
    fi
    echo ""
}

# Function to show MJPEG URLs
show_stream_urls() {
    echo "🔗 MJPEG Stream URLs:"
    echo "--------------------"
    for i in {1..8}; do
        port=$((8160 + i))
        echo "Camera $i: http://localhost:$port/stream.mjpg"
    done
    echo ""
}

# Main monitoring loop
echo "Starting performance monitoring... (Press Ctrl+C to stop)"
echo ""

# Show stream URLs once
show_stream_urls

while true; do
    clear
    echo "🔍 AI Security Vision System - Performance Monitor"
    echo "=================================================="
    echo "$(date '+%Y-%m-%d %H:%M:%S') - Monitoring 8-camera stress test"
    echo ""
    
    get_system_stats
    get_api_status
    check_mjpeg_streams
    get_network_stats
    
    echo "⏱️  Next update in 5 seconds... (Ctrl+C to stop)"
    sleep 5
done
