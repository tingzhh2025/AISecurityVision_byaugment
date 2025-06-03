#!/bin/bash

# AI Security Vision System - Single Instance Startup Script
# 确保只有一个AISecurityVision进程运行，避免NPU资源冲突

echo "=== AI Security Vision System - Single Instance Startup ==="
echo "Checking for existing processes..."

# 检查并停止现有的AISecurityVision进程
EXISTING_PIDS=$(pgrep -f "AISecurityVision" | grep -v $$)
if [ ! -z "$EXISTING_PIDS" ]; then
    echo "Found existing AISecurityVision processes: $EXISTING_PIDS"
    echo "Stopping existing processes..."
    pkill -f "AISecurityVision"
    sleep 2
    
    # 强制杀死仍在运行的进程
    REMAINING_PIDS=$(pgrep -f "AISecurityVision" | grep -v $$)
    if [ ! -z "$REMAINING_PIDS" ]; then
        echo "Force killing remaining processes: $REMAINING_PIDS"
        pkill -9 -f "AISecurityVision"
        sleep 1
    fi
fi

# 检查NPU资源是否可用
echo "Checking NPU availability..."
if [ -f "/dev/rknpu" ]; then
    echo "✓ NPU device found: /dev/rknpu"
else
    echo "⚠ Warning: NPU device not found, system will use CPU fallback"
fi

# 检查RKNN服务状态
RKNN_SERVER=$(pgrep rknn_server)
if [ ! -z "$RKNN_SERVER" ]; then
    echo "✓ RKNN server running (PID: $RKNN_SERVER)"
else
    echo "⚠ Warning: RKNN server not running"
fi

# 切换到构建目录
cd /userdata/source/source/AISecurityVision_byaugment/build

# 检查可执行文件
if [ ! -f "./AISecurityVision" ]; then
    echo "❌ Error: AISecurityVision executable not found in $(pwd)"
    exit 1
fi

echo "✓ AISecurityVision executable found"

# 启动新的AISecurityVision进程
echo "Starting AISecurityVision..."
echo "Working directory: $(pwd)"
echo "Command: ./AISecurityVision"
echo "================================"

# 启动进程并获取PID
./AISecurityVision &
NEW_PID=$!

echo "AISecurityVision started with PID: $NEW_PID"

# 等待几秒钟让服务启动
sleep 3

# 检查进程是否仍在运行
if kill -0 $NEW_PID 2>/dev/null; then
    echo "✓ AISecurityVision is running successfully"
    echo "API endpoints available at: http://localhost:8080"
    echo "Process PID: $NEW_PID"
    echo ""
    echo "To stop the service, run: kill $NEW_PID"
    echo "To check status, run: ps aux | grep AISecurityVision"
else
    echo "❌ Error: AISecurityVision failed to start"
    exit 1
fi

echo "================================"
echo "Single instance startup complete!"
