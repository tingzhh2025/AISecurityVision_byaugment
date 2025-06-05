#!/bin/bash
set -e

# AI Security Vision System - Docker Entrypoint Script
# 这个脚本负责初始化和启动AI安全视觉系统

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 日志函数
log_info() {
    echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

log_debug() {
    if [[ "${DEBUG:-false}" == "true" ]]; then
        echo -e "${BLUE}[DEBUG]${NC} $1"
    fi
}

# 环境变量默认值
export AI_SECURITY_VISION_ENV=${AI_SECURITY_VISION_ENV:-production}
export DATABASE_PATH=${DATABASE_PATH:-/opt/aibox/data/aibox.db}
export LOG_LEVEL=${LOG_LEVEL:-INFO}
export LOG_PATH=${LOG_PATH:-/opt/aibox/logs}
export ENABLE_RKNN=${ENABLE_RKNN:-true}
export ENABLE_SECURITY=${ENABLE_SECURITY:-true}
export JWT_SECRET_KEY=${JWT_SECRET_KEY:-$(openssl rand -hex 32)}
export API_RATE_LIMIT=${API_RATE_LIMIT:-100}
export MAX_CONNECTIONS=${MAX_CONNECTIONS:-10}

# 检查必要的目录
check_directories() {
    log_info "Checking required directories..."
    
    local dirs=(
        "/opt/aibox/data"
        "/opt/aibox/logs"
        "/opt/aibox/config"
        "/opt/aibox/models"
    )
    
    for dir in "${dirs[@]}"; do
        if [[ ! -d "$dir" ]]; then
            log_warn "Creating missing directory: $dir"
            mkdir -p "$dir"
        fi
    done
    
    # 确保权限正确
    chown -R aibox:aibox /opt/aibox/data /opt/aibox/logs /opt/aibox/config 2>/dev/null || true
}

# 检查RKNN NPU支持
check_rknn_support() {
    log_info "Checking RKNN NPU support..."
    
    if [[ "${ENABLE_RKNN}" == "true" ]]; then
        # 检查RKNN设备
        if [[ -e "/dev/dri" ]]; then
            log_info "RKNN NPU device detected"
            export RKNN_AVAILABLE=true
        else
            log_warn "RKNN NPU device not found, falling back to CPU inference"
            export RKNN_AVAILABLE=false
        fi
        
        # 检查RKNN模型文件
        if [[ -f "/opt/aibox/models/yolov8n.rknn" ]]; then
            log_info "RKNN model file found"
        else
            log_warn "RKNN model file not found: /opt/aibox/models/yolov8n.rknn"
        fi
    else
        log_info "RKNN support disabled"
        export RKNN_AVAILABLE=false
    fi
}

# 初始化数据库
init_database() {
    log_info "Initializing database..."
    
    local db_dir=$(dirname "$DATABASE_PATH")
    if [[ ! -d "$db_dir" ]]; then
        mkdir -p "$db_dir"
    fi
    
    # 如果数据库不存在，创建初始数据库
    if [[ ! -f "$DATABASE_PATH" ]]; then
        log_info "Creating new database: $DATABASE_PATH"
        # 数据库将在应用启动时自动创建
    else
        log_info "Using existing database: $DATABASE_PATH"
    fi
}

# 生成配置文件
generate_config() {
    log_info "Generating configuration..."
    
    local config_file="/opt/aibox/config/production.json"
    
    cat > "$config_file" << EOF
{
    "environment": "${AI_SECURITY_VISION_ENV}",
    "database": {
        "path": "${DATABASE_PATH}",
        "max_connections": ${MAX_CONNECTIONS},
        "connection_timeout": 30,
        "enable_wal": true
    },
    "logging": {
        "level": "${LOG_LEVEL}",
        "path": "${LOG_PATH}",
        "max_file_size": "100MB",
        "max_files": 10,
        "enable_console": false
    },
    "api": {
        "port": 8080,
        "rate_limit": ${API_RATE_LIMIT},
        "enable_cors": true,
        "enable_security": ${ENABLE_SECURITY}
    },
    "ai": {
        "enable_rknn": ${RKNN_AVAILABLE:-false},
        "model_path": "/opt/aibox/models/yolov8n.rknn",
        "confidence_threshold": 0.5,
        "nms_threshold": 0.4
    },
    "security": {
        "jwt_secret": "${JWT_SECRET_KEY}",
        "token_expiration": 24,
        "enable_rate_limiting": true,
        "enable_input_validation": true
    },
    "streaming": {
        "mjpeg_ports": [8161, 8162, 8163, 8164],
        "enable_compression": true,
        "quality": 85
    }
}
EOF
    
    log_info "Configuration saved to: $config_file"
}

# 等待依赖服务
wait_for_dependencies() {
    log_info "Waiting for dependencies..."
    
    # 等待Redis（如果启用）
    if [[ "${ENABLE_REDIS:-false}" == "true" ]]; then
        local redis_host=${REDIS_HOST:-aibox-redis}
        local redis_port=${REDIS_PORT:-6379}
        
        log_info "Waiting for Redis at $redis_host:$redis_port..."
        while ! nc -z "$redis_host" "$redis_port" 2>/dev/null; do
            sleep 1
        done
        log_info "Redis is ready"
    fi
}

# 启动应用
start_application() {
    log_info "Starting AI Security Vision System..."
    
    # 设置环境变量
    export CONFIG_FILE="/opt/aibox/config/production.json"
    
    # 启动应用
    cd /opt/aibox
    exec ./bin/AISecurityVision --config="$CONFIG_FILE" --daemon=false
}

# 停止应用
stop_application() {
    log_info "Stopping AI Security Vision System..."
    
    # 发送SIGTERM信号给主进程
    if [[ -n "${MAIN_PID:-}" ]]; then
        kill -TERM "$MAIN_PID" 2>/dev/null || true
        wait "$MAIN_PID" 2>/dev/null || true
    fi
    
    log_info "Application stopped"
}

# 信号处理
handle_signal() {
    log_info "Received signal, shutting down gracefully..."
    stop_application
    exit 0
}

# 注册信号处理器
trap handle_signal SIGTERM SIGINT

# 主函数
main() {
    log_info "AI Security Vision System - Docker Container Starting"
    log_info "Environment: ${AI_SECURITY_VISION_ENV}"
    log_info "Version: $(cat /opt/aibox/VERSION 2>/dev/null || echo 'unknown')"
    
    case "${1:-start}" in
        start)
            check_directories
            check_rknn_support
            init_database
            generate_config
            wait_for_dependencies
            start_application
            ;;
        stop)
            stop_application
            ;;
        restart)
            stop_application
            sleep 2
            start_application
            ;;
        config)
            generate_config
            log_info "Configuration generated successfully"
            ;;
        health)
            # 健康检查
            if curl -f -s http://localhost:8080/api/system/status >/dev/null 2>&1; then
                log_info "Health check passed"
                exit 0
            else
                log_error "Health check failed"
                exit 1
            fi
            ;;
        version)
            echo "AI Security Vision System"
            echo "Version: $(cat /opt/aibox/VERSION 2>/dev/null || echo 'unknown')"
            echo "Build: $(cat /opt/aibox/BUILD_INFO 2>/dev/null || echo 'unknown')"
            ;;
        *)
            log_error "Unknown command: $1"
            echo "Usage: $0 {start|stop|restart|config|health|version}"
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"
