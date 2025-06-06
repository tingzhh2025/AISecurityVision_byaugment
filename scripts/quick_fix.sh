#!/bin/bash

echo "=== AISecurityVision 快速修复脚本 ==="
echo "此脚本将尝试修复常见问题"
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目路径
PROJECT_DIR="/home/rogers/source/custom/AISecurityVision_byaugment"
BUILD_DIR="$PROJECT_DIR/build"

# 1. 创建必要的目录
echo -e "${GREEN}1. 创建必要的目录...${NC}"
mkdir -p $PROJECT_DIR/models
mkdir -p $PROJECT_DIR/logs
echo "   ✅ 目录创建完成"
echo ""

# 2. 设置文件权限
echo -e "${GREEN}2. 设置文件权限...${NC}"
if [ -f "$PROJECT_DIR/aibox.db" ]; then
    chmod 666 $PROJECT_DIR/aibox.db
    echo "   ✅ 数据库文件权限已设置"
else
    echo "   ⚠️  数据库文件将在首次运行时创建"
fi
echo ""

# 3. 检查并停止占用端口的进程
echo -e "${GREEN}3. 检查端口占用...${NC}"
if lsof -i:8080 > /dev/null 2>&1; then
    echo -e "   ${YELLOW}端口8080被占用，尝试停止相关进程...${NC}"
    # 获取占用端口的进程PID
    PID=$(lsof -ti:8080)
    if [ ! -z "$PID" ]; then
        echo "   找到进程PID: $PID"
        # 检查是否是AISecurityVision进程
        if ps -p $PID -o comm= | grep -q "AISecurityVision"; then
            echo "   停止AISecurityVision进程..."
            kill -TERM $PID
            sleep 2
            # 如果进程仍在运行，强制停止
            if kill -0 $PID 2>/dev/null; then
                kill -KILL $PID
            fi
            echo "   ✅ 已停止旧的AISecurityVision进程"
        else
            echo -e "   ${RED}端口被其他进程占用，请手动处理${NC}"
            lsof -i:8080
        fi
    fi
else
    echo "   ✅ 端口8080未被占用"
fi
echo ""

# 4. 创建模型文件占位符（如果不存在）
echo -e "${GREEN}4. 检查模型文件...${NC}"
RKNN_MODEL="$PROJECT_DIR/models/yolov8n.rknn"
if [ ! -f "$RKNN_MODEL" ]; then
    echo -e "   ${YELLOW}RKNN模型文件不存在${NC}"
    echo "   请下载yolov8n.rknn模型文件到: $PROJECT_DIR/models/"
    echo "   下载地址：https://github.com/rockchip-linux/rknn-toolkit2"
    
    # 创建一个提示文件
    echo "请放置yolov8n.rknn模型文件到此目录" > $PROJECT_DIR/models/README.txt
else
    echo "   ✅ RKNN模型文件存在"
fi
echo ""

# 5. 创建简化的启动脚本
echo -e "${GREEN}5. 创建启动脚本...${NC}"
cat > $PROJECT_DIR/start_aibox.sh << 'EOF'
#!/bin/bash

# AISecurityVision 启动脚本
PROJECT_DIR="/home/rogers/source/custom/AISecurityVision_byaugment"
BUILD_DIR="$PROJECT_DIR/build"

# 切换到构建目录
cd $BUILD_DIR

# 默认参数
PORT=8080
VERBOSE=""

# 解析命令行参数
while [[ $# -gt 0 ]]; do
    case $1 in
        -p|--port)
            PORT="$2"
            shift 2
            ;;
        -v|--verbose)
            VERBOSE="-v"
            shift
            ;;
        *)
            echo "未知参数: $1"
            echo "用法: $0 [-p PORT] [-v]"
            exit 1
            ;;
    esac
done

echo "启动AISecurityVision..."
echo "API端口: $PORT"
echo "详细日志: $([ "$VERBOSE" = "-v" ] && echo "启用" || echo "禁用")"
echo ""

# 启动程序
./AISecurityVision -p $PORT $VERBOSE
EOF

chmod +x $PROJECT_DIR/start_aibox.sh
echo "   ✅ 启动脚本创建完成: $PROJECT_DIR/start_aibox.sh"
echo ""

# 6. 创建测试脚本
echo -e "${GREEN}6. 创建API测试脚本...${NC}"
cat > $PROJECT_DIR/test_api.sh << 'EOF'
#!/bin/bash

# 简单的API测试脚本
API_URL="http://localhost:8080"

echo "测试API连接..."
echo ""

# 测试系统状态
echo "1. 测试系统状态接口..."
curl -s "$API_URL/api/system/status" | python3 -m json.tool || echo "API未响应"
echo ""

# 测试摄像头列表
echo "2. 测试摄像头列表接口..."
curl -s "$API_URL/api/cameras" | python3 -m json.tool || echo "API未响应"
echo ""

# 测试系统信息
echo "3. 测试系统信息接口..."
curl -s "$API_URL/api/system/info" | python3 -m json.tool || echo "API未响应"
echo ""
EOF

chmod +x $PROJECT_DIR/test_api.sh
echo "   ✅ API测试脚本创建完成: $PROJECT_DIR/test_api.sh"
echo ""

# 7. 提供使用说明
echo -e "${GREEN}=== 修复完成 ===${NC}"
echo ""
echo "接下来的步骤："
echo ""
echo "1. 如果还没有RKNN模型文件："
echo "   - 下载yolov8n.rknn到 $PROJECT_DIR/models/"
echo ""
echo "2. 编译项目（如果需要）："
echo "   cd $BUILD_DIR"
echo "   cmake .."
echo "   make -j$(nproc)"
echo ""
echo "3. 启动服务："
echo "   $PROJECT_DIR/start_aibox.sh"
echo "   # 或使用详细日志模式："
echo "   $PROJECT_DIR/start_aibox.sh -v"
echo "   # 或使用其他端口："
echo "   $PROJECT_DIR/start_aibox.sh -p 8081"
echo ""
echo "4. 测试API："
echo "   $PROJECT_DIR/test_api.sh"
echo ""
echo "5. 如果仍有问题，运行诊断脚本："
echo "   $PROJECT_DIR/scripts/diagnose_api_service.sh"
echo "   $PROJECT_DIR/scripts/diagnose_video_pipeline.sh"
echo ""
