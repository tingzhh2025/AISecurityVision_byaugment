#!/bin/bash

echo "=== AISecurityVision APIService诊断脚本 ==="
echo "时间: $(date)"
echo ""

# 检查端口占用
echo "1. 检查端口8080占用情况..."
if lsof -i:8080 > /dev/null 2>&1; then
    echo "   ❌ 端口8080已被占用："
    lsof -i:8080
    echo "   建议：停止占用端口的进程或修改API端口"
else
    echo "   ✅ 端口8080未被占用"
fi
echo ""

# 检查数据库文件
echo "2. 检查数据库文件..."
DB_FILE="/home/rogers/source/custom/AISecurityVision_byaugment/aibox.db"
if [ -f "$DB_FILE" ]; then
    echo "   ✅ 数据库文件存在"
    echo "   文件大小: $(ls -lh $DB_FILE | awk '{print $5}')"
    # 检查数据库是否可读写
    if [ -r "$DB_FILE" ] && [ -w "$DB_FILE" ]; then
        echo "   ✅ 数据库文件可读写"
    else
        echo "   ❌ 数据库文件权限问题"
        ls -l $DB_FILE
    fi
else
    echo "   ❌ 数据库文件不存在: $DB_FILE"
    echo "   建议：运行程序会自动创建数据库"
fi
echo ""

# 检查日志文件
echo "3. 检查日志文件..."
LOG_DIR="/home/rogers/source/custom/AISecurityVision_byaugment/logs"
if [ -d "$LOG_DIR" ]; then
    echo "   ✅ 日志目录存在"
    echo "   最新日志文件："
    ls -lt $LOG_DIR/*.log 2>/dev/null | head -5
else
    echo "   ⚠️  日志目录不存在，将在运行时创建"
fi
echo ""

# 检查进程状态
echo "4. 检查AISecurityVision进程..."
if pgrep -f "AISecurityVision" > /dev/null; then
    echo "   ✅ AISecurityVision进程正在运行"
    echo "   进程信息："
    ps aux | grep AISecurityVision | grep -v grep
else
    echo "   ❌ AISecurityVision进程未运行"
fi
echo ""

# 测试API连接
echo "5. 测试API连接..."
if command -v curl > /dev/null; then
    echo "   尝试连接 http://localhost:8080/api/system/status ..."
    response=$(curl -s -w "\n状态码: %{http_code}\n响应时间: %{time_total}s" -o /tmp/api_response.txt http://localhost:8080/api/system/status 2>&1)
    
    if [ $? -eq 0 ]; then
        echo "   ✅ API连接成功"
        echo "   $response"
        echo "   响应内容："
        cat /tmp/api_response.txt | head -20
    else
        echo "   ❌ API连接失败"
        echo "   错误信息: $response"
    fi
else
    echo "   ⚠️  curl未安装，跳过API测试"
fi
echo ""

# 检查网络配置
echo "6. 检查网络配置..."
echo "   本机IP地址："
ip addr | grep "inet " | grep -v "127.0.0.1" | awk '{print "   - " $2}'
echo ""

# 提供解决建议
echo "=== 问题解决建议 ==="
echo ""
echo "如果APIService无响应，请尝试："
echo "1. 修改API端口（避免端口冲突）："
echo "   ./AISecurityVision -p 8081"
echo ""
echo "2. 以详细日志模式运行："
echo "   ./AISecurityVision -v"
echo ""
echo "3. 检查防火墙设置："
echo "   sudo ufw status"
echo "   sudo ufw allow 8080/tcp"
echo ""
echo "4. 查看系统日志："
echo "   journalctl -u AISecurityVision -n 100"
echo ""

rm -f /tmp/api_response.txt
