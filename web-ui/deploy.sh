#!/bin/bash

# AI安防视频监控系统 Web UI 部署脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 配置变量
DEPLOY_DIR="/var/www/ai-security-vision"
NGINX_CONFIG_DIR="/etc/nginx/sites-available"
NGINX_ENABLED_DIR="/etc/nginx/sites-enabled"
SERVICE_NAME="ai-security-vision"
BACKUP_DIR="/var/backups/ai-security-vision"

# 打印消息函数
print_message() {
    echo -e "${2}[$(date +'%Y-%m-%d %H:%M:%S')] $1${NC}"
}

print_info() {
    print_message "$1" "$BLUE"
}

print_success() {
    print_message "$1" "$GREEN"
}

print_warning() {
    print_message "$1" "$YELLOW"
}

print_error() {
    print_message "$1" "$RED"
}

# 检查权限
check_permissions() {
    if [ "$EUID" -ne 0 ]; then
        print_error "请使用 sudo 运行此脚本"
        exit 1
    fi
}

# 检查依赖
check_dependencies() {
    print_info "检查系统依赖..."
    
    # 检查nginx
    if ! command -v nginx &> /dev/null; then
        print_warning "Nginx 未安装，正在安装..."
        apt-get update
        apt-get install -y nginx
    fi
    
    # 检查Node.js
    if ! command -v node &> /dev/null; then
        print_warning "Node.js 未安装，正在安装..."
        curl -fsSL https://deb.nodesource.com/setup_18.x | bash -
        apt-get install -y nodejs
    fi
    
    print_success "依赖检查完成"
}

# 备份现有部署
backup_existing() {
    if [ -d "$DEPLOY_DIR" ]; then
        print_info "备份现有部署..."
        mkdir -p "$BACKUP_DIR"
        BACKUP_NAME="backup_$(date +%Y%m%d_%H%M%S)"
        cp -r "$DEPLOY_DIR" "$BACKUP_DIR/$BACKUP_NAME"
        print_success "备份完成: $BACKUP_DIR/$BACKUP_NAME"
    fi
}

# 构建项目
build_project() {
    print_info "构建项目..."
    
    # 安装依赖
    if command -v yarn &> /dev/null; then
        yarn install --frozen-lockfile
        yarn build
    else
        npm ci
        npm run build
    fi
    
    print_success "项目构建完成"
}

# 部署文件
deploy_files() {
    print_info "部署文件..."
    
    # 创建部署目录
    mkdir -p "$DEPLOY_DIR"
    
    # 复制构建文件
    cp -r dist/* "$DEPLOY_DIR/"
    
    # 设置权限
    chown -R www-data:www-data "$DEPLOY_DIR"
    chmod -R 755 "$DEPLOY_DIR"
    
    print_success "文件部署完成"
}

# 配置Nginx
configure_nginx() {
    print_info "配置Nginx..."
    
    # 创建Nginx配置文件
    cat > "$NGINX_CONFIG_DIR/$SERVICE_NAME" << 'EOF'
server {
    listen 80;
    server_name localhost;
    
    root /var/www/ai-security-vision;
    index index.html;
    
    # 前端路由支持
    location / {
        try_files $uri $uri/ /index.html;
    }
    
    # API代理
    location /api/ {
        proxy_pass http://localhost:8080/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # 超时设置
        proxy_connect_timeout 30s;
        proxy_send_timeout 30s;
        proxy_read_timeout 30s;
    }
    
    # 视频流代理
    location /stream/ {
        proxy_pass http://localhost:8161/;
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        
        # 视频流特殊设置
        proxy_buffering off;
        proxy_cache off;
        proxy_set_header Connection "";
        proxy_http_version 1.1;
        chunked_transfer_encoding off;
    }
    
    # WebSocket代理
    location /ws/ {
        proxy_pass http://localhost:8080/;
        proxy_http_version 1.1;
        proxy_set_header Upgrade $http_upgrade;
        proxy_set_header Connection "upgrade";
        proxy_set_header Host $host;
        proxy_set_header X-Real-IP $remote_addr;
        proxy_set_header X-Forwarded-For $proxy_add_x_forwarded_for;
        proxy_set_header X-Forwarded-Proto $scheme;
        
        # WebSocket超时设置
        proxy_read_timeout 86400;
        proxy_send_timeout 86400;
    }
    
    # 静态资源缓存
    location ~* \.(js|css|png|jpg|jpeg|gif|ico|svg|woff|woff2|ttf|eot)$ {
        expires 1y;
        add_header Cache-Control "public, immutable";
        access_log off;
    }
    
    # 安全头
    add_header X-Frame-Options "SAMEORIGIN" always;
    add_header X-Content-Type-Options "nosniff" always;
    add_header X-XSS-Protection "1; mode=block" always;
    add_header Referrer-Policy "strict-origin-when-cross-origin" always;
    
    # 日志
    access_log /var/log/nginx/ai-security-vision.access.log;
    error_log /var/log/nginx/ai-security-vision.error.log;
}
EOF
    
    # 启用站点
    ln -sf "$NGINX_CONFIG_DIR/$SERVICE_NAME" "$NGINX_ENABLED_DIR/"
    
    # 测试配置
    nginx -t
    
    print_success "Nginx配置完成"
}

# 重启服务
restart_services() {
    print_info "重启服务..."
    
    systemctl reload nginx
    systemctl enable nginx
    
    print_success "服务重启完成"
}

# 创建systemd服务（可选）
create_systemd_service() {
    print_info "创建systemd服务..."
    
    cat > "/etc/systemd/system/$SERVICE_NAME.service" << EOF
[Unit]
Description=AI Security Vision Web UI
After=network.target

[Service]
Type=simple
User=www-data
Group=www-data
WorkingDirectory=$DEPLOY_DIR
ExecStart=/usr/bin/nginx -g "daemon off;"
Restart=always
RestartSec=10

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
    systemctl enable "$SERVICE_NAME"
    
    print_success "systemd服务创建完成"
}

# 显示部署信息
show_deployment_info() {
    print_success "部署完成！"
    echo ""
    print_info "部署信息:"
    echo "  - 部署目录: $DEPLOY_DIR"
    echo "  - Nginx配置: $NGINX_CONFIG_DIR/$SERVICE_NAME"
    echo "  - 访问地址: http://localhost"
    echo "  - 日志文件: /var/log/nginx/ai-security-vision.*.log"
    echo ""
    print_info "服务状态:"
    systemctl status nginx --no-pager -l
    echo ""
    print_warning "注意事项:"
    echo "  - 请确保C++后端服务运行在 localhost:8080"
    echo "  - 请确保视频流服务运行在 localhost:8161"
    echo "  - 如需HTTPS，请配置SSL证书"
    echo ""
}

# 清理函数
cleanup() {
    print_info "清理临时文件..."
    # 这里可以添加清理逻辑
}

# 显示帮助
show_help() {
    echo "AI安防视频监控系统 Web UI 部署脚本"
    echo ""
    echo "用法: sudo $0 [选项]"
    echo ""
    echo "选项:"
    echo "  deploy, -d, --deploy    执行完整部署 (默认)"
    echo "  build, -b, --build      仅构建项目"
    echo "  nginx, -n, --nginx      仅配置Nginx"
    echo "  help, -h, --help        显示帮助信息"
    echo ""
    echo "示例:"
    echo "  sudo $0                 # 完整部署"
    echo "  sudo $0 build           # 仅构建"
    echo "  sudo $0 nginx           # 仅配置Nginx"
    echo ""
}

# 主函数
main() {
    print_info "AI安防视频监控系统 Web UI 部署脚本"
    print_info "=========================================="
    
    case "${1:-deploy}" in
        "deploy"|"-d"|"--deploy")
            check_permissions
            check_dependencies
            backup_existing
            build_project
            deploy_files
            configure_nginx
            restart_services
            show_deployment_info
            ;;
        "build"|"-b"|"--build")
            build_project
            ;;
        "nginx"|"-n"|"--nginx")
            check_permissions
            configure_nginx
            restart_services
            ;;
        "help"|"-h"|"--help")
            show_help
            ;;
        *)
            print_error "未知选项: $1"
            show_help
            exit 1
            ;;
    esac
}

# 错误处理
trap 'print_error "部署失败"; cleanup; exit 1' ERR
trap 'cleanup' EXIT

# 检查是否在正确的目录
if [ ! -f "package.json" ]; then
    print_error "请在 web-ui 目录下运行此脚本"
    exit 1
fi

# 执行主函数
main "$@"
