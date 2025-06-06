#!/bin/bash

echo "=== AISecurityVision 配置优化工具 ==="
echo ""

PROJECT_DIR="/home/rogers/source/custom/AISecurityVision_byaugment"
DB_FILE="$PROJECT_DIR/aibox.db"

# 创建或更新系统配置
echo "1. 优化系统配置..."

# 如果数据库不存在，提示用户先运行程序
if [ ! -f "$DB_FILE" ]; then
    echo "   ❌ 数据库文件不存在"
    echo "   请先运行一次AISecurityVision以创建数据库"
    exit 1
fi

# 创建SQL脚本
cat > /tmp/optimize_config.sql << 'EOF'
-- 创建配置表（如果不存在）
CREATE TABLE IF NOT EXISTS config (
    section TEXT NOT NULL,
    key TEXT NOT NULL,
    value TEXT,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    PRIMARY KEY (section, key)
);

-- 优化系统配置
INSERT OR REPLACE INTO config (section, key, value) VALUES 
    ('system', 'optimized_detection', 'false'),  -- 暂时禁用优化检测，避免NPU问题
    ('system', 'detection_threads', '2'),        -- 减少检测线程数
    ('system', 'verbose_logging', 'true'),       -- 启用详细日志
    ('system', 'status_interval', '10');         -- 更频繁的状态更新

-- 设置默认检测配置
INSERT OR REPLACE INTO config (section, key, value) VALUES 
    ('detection', 'confidence_threshold', '0.5'),
    ('detection', 'nms_threshold', '0.4'),
    ('detection', 'enabled_categories', '["person", "car", "truck", "bus"]');

-- 查看配置
SELECT section, key, value FROM config ORDER BY section, key;
EOF

# 执行SQL脚本
echo "   执行配置优化..."
sqlite3 $DB_FILE < /tmp/optimize_config.sql

echo "   ✅ 系统配置已优化"
echo ""

# 创建示例摄像头配置
echo "2. 创建示例摄像头配置..."

cat > /tmp/sample_camera.sql << 'EOF'
-- 创建摄像头表（如果不存在）
CREATE TABLE IF NOT EXISTS cameras (
    id TEXT PRIMARY KEY,
    name TEXT,
    rtsp_url TEXT,
    config JSON,
    created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
    updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
);

-- 插入示例摄像头（如果不存在）
INSERT OR IGNORE INTO cameras (id, name, rtsp_url, config) VALUES 
    ('test_camera_1', 'Test Camera 1', 'rtsp://admin:admin@192.168.1.100:554/stream1', 
     '{"enabled": true, "detection_enabled": true, "recording_enabled": false, "stream_config": {"fps": 15, "quality": 70}}');

-- 查看摄像头
SELECT id, name, rtsp_url FROM cameras;
EOF

sqlite3 $DB_FILE < /tmp/sample_camera.sql

echo "   ✅ 示例摄像头配置已创建"
echo ""

# 创建运行配置文件
echo "3. 创建运行配置文件..."

cat > $PROJECT_DIR/aibox_config.json << 'EOF'
{
    "system": {
        "api_port": 8080,
        "log_level": "info",
        "enable_statistics": true,
        "performance": {
            "max_pipelines": 4,
            "max_memory_percent": 80,
            "frame_skip_threshold": 5
        }
    },
    "detection": {
        "backend": "RKNN",
        "model_path": "models/yolov8n.rknn",
        "input_size": [640, 640],
        "optimization": {
            "batch_size": 1,
            "num_threads": 2,
            "use_gpu": false,
            "use_npu": true
        }
    },
    "streaming": {
        "mjpeg": {
            "enabled": true,
            "default_port": 8000,
            "max_clients": 10,
            "frame_rate": 15,
            "quality": 75
        }
    },
    "recording": {
        "enabled": false,
        "path": "/var/recordings",
        "format": "mp4",
        "retention_days": 7
    }
}
EOF

echo "   ✅ 配置文件已创建: $PROJECT_DIR/aibox_config.json"
echo ""

# 生成性能调优建议
echo "4. 性能调优建议..."
echo ""
echo "   针对RK3588平台的优化建议："
echo "   - 使用NPU进行AI推理（需要正确的RKNN模型）"
echo "   - 限制同时运行的视频流数量（建议不超过4路）"
echo "   - 调整视频分辨率和帧率以平衡性能"
echo "   - 使用硬件解码（FFmpeg with rkmpp）"
echo ""

# 清理临时文件
rm -f /tmp/optimize_config.sql /tmp/sample_camera.sql

echo "=== 配置优化完成 ==="
echo ""
echo "现在可以使用以下命令启动："
echo "  cd $PROJECT_DIR/build"
echo "  ./AISecurityVision -v"
echo ""
echo "或使用启动脚本："
echo "  $PROJECT_DIR/start_aibox.sh -v"
echo ""
