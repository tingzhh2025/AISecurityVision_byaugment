#!/bin/bash

echo "=== AISecurityVision VideoPipeline诊断脚本 ==="
echo "时间: $(date)"
echo ""

# 检查模型文件
echo "1. 检查AI模型文件..."
MODEL_DIR="/home/rogers/source/custom/AISecurityVision_byaugment/models"
RKNN_MODEL="$MODEL_DIR/yolov8n.rknn"

if [ -f "$RKNN_MODEL" ]; then
    echo "   ✅ RKNN模型文件存在"
    echo "   文件大小: $(ls -lh $RKNN_MODEL | awk '{print $5}')"
    echo "   修改时间: $(ls -l $RKNN_MODEL | awk '{print $6, $7, $8}')"
else
    echo "   ❌ RKNN模型文件不存在: $RKNN_MODEL"
    echo "   建议：确保已下载并放置YOLOv8n RKNN模型"
fi

# 列出所有模型文件
echo ""
echo "   模型目录内容："
if [ -d "$MODEL_DIR" ]; then
    ls -la $MODEL_DIR/
else
    echo "   ❌ 模型目录不存在: $MODEL_DIR"
    echo "   建议：创建目录并下载模型文件"
fi
echo ""

# 检查NPU驱动
echo "2. 检查RK3588 NPU驱动..."
if [ -e "/dev/rknpu0" ]; then
    echo "   ✅ RKNPU设备存在"
    ls -l /dev/rknpu*
else
    echo "   ❌ RKNPU设备不存在"
    echo "   建议：检查NPU驱动是否正确安装"
fi

# 检查RKNN运行时库
echo ""
echo "3. 检查RKNN运行时库..."
if ldconfig -p | grep -q "librknnrt.so"; then
    echo "   ✅ RKNN运行时库已安装"
    ldconfig -p | grep rknn
else
    echo "   ❌ RKNN运行时库未找到"
    echo "   建议：安装RKNN-Toolkit2运行时库"
fi
echo ""

# 检查系统资源
echo "4. 检查系统资源..."
echo "   内存使用情况："
free -h | grep -E "^Mem|^Swap"
echo ""
echo "   NPU使用情况："
if command -v rknn_demo > /dev/null 2>&1; then
    rknn_demo -l 2>/dev/null || echo "   ⚠️  无法获取NPU状态"
else
    echo "   ⚠️  rknn_demo工具未安装"
fi
echo ""

# 检查视频源配置
echo "5. 检查摄像头配置..."
DB_FILE="/home/rogers/source/custom/AISecurityVision_byaugment/aibox.db"
if [ -f "$DB_FILE" ]; then
    echo "   数据库中的摄像头配置："
    sqlite3 $DB_FILE "SELECT id, name, rtsp_url FROM cameras;" 2>/dev/null || echo "   ⚠️  无法读取数据库"
else
    echo "   ⚠️  数据库文件不存在"
fi
echo ""

# 检查RTSP连接
echo "6. 测试RTSP连接（如果有配置）..."
# 这里可以添加ffprobe测试RTSP流的代码
echo "   提示：可以使用ffprobe测试RTSP流："
echo "   ffprobe -v error -show_entries stream=codec_name,width,height -of json rtsp://your_camera_url"
echo ""

# 检查依赖库
echo "7. 检查关键依赖库..."
libs=("libopencv_core.so" "libavcodec.so" "libavformat.so")
for lib in "${libs[@]}"; do
    if ldconfig -p | grep -q "$lib"; then
        echo "   ✅ $lib 已安装"
    else
        echo "   ❌ $lib 未找到"
    fi
done
echo ""

# 提供解决建议
echo "=== VideoPipeline创建失败解决建议 ==="
echo ""
echo "1. 确保RKNN模型文件存在："
echo "   - 下载yolov8n.rknn模型"
echo "   - 放置到 $MODEL_DIR/ 目录"
echo ""
echo "2. 安装RKNN运行时环境："
echo "   - 下载RKNN-Toolkit2"
echo "   - 安装librknnrt.so库"
echo ""
echo "3. 降低系统负载："
echo "   - 减少同时运行的摄像头数量"
echo "   - 调整检测线程数"
echo ""
echo "4. 检查摄像头配置："
echo "   - 确保RTSP URL正确"
echo "   - 测试网络连接"
echo ""
echo "5. 查看详细日志："
echo "   ./AISecurityVision -v"
echo ""

# 生成配置建议
echo "=== 推荐的启动配置 ==="
echo ""
echo "# 创建模型目录"
echo "mkdir -p $MODEL_DIR"
echo ""
echo "# 以详细日志模式运行，使用备用端口"
echo "cd /home/rogers/source/custom/AISecurityVision_byaugment/build"
echo "./AISecurityVision -v -p 8081"
echo ""
echo "# 或者禁用优化检测（如果NPU有问题）"
echo "# 在数据库中设置 optimized_detection = false"
echo ""
