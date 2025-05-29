#!/bin/bash

# AI安防视频监控系统 Web UI 启动脚本

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 打印带颜色的消息
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

# 检查Node.js版本
check_node() {
    if ! command -v node &> /dev/null; then
        print_error "Node.js 未安装，请先安装 Node.js 16.0 或更高版本"
        exit 1
    fi
    
    NODE_VERSION=$(node -v | cut -d'v' -f2)
    REQUIRED_VERSION="16.0.0"
    
    if ! node -e "process.exit(require('semver').gte('$NODE_VERSION', '$REQUIRED_VERSION') ? 0 : 1)" 2>/dev/null; then
        print_error "Node.js 版本过低，当前版本: $NODE_VERSION，需要: $REQUIRED_VERSION 或更高"
        exit 1
    fi
    
    print_success "Node.js 版本检查通过: $NODE_VERSION"
}

# 检查npm或yarn
check_package_manager() {
    if command -v yarn &> /dev/null; then
        PACKAGE_MANAGER="yarn"
        print_info "使用 Yarn 作为包管理器"
    elif command -v npm &> /dev/null; then
        PACKAGE_MANAGER="npm"
        print_info "使用 npm 作为包管理器"
    else
        print_error "未找到 npm 或 yarn，请先安装"
        exit 1
    fi
}

# 安装依赖
install_dependencies() {
    print_info "检查依赖..."
    
    if [ ! -d "node_modules" ] || [ ! -f "package-lock.json" ] && [ ! -f "yarn.lock" ]; then
        print_info "安装依赖包..."
        if [ "$PACKAGE_MANAGER" = "yarn" ]; then
            yarn install
        else
            npm install
        fi
        print_success "依赖安装完成"
    else
        print_info "依赖已存在，跳过安装"
    fi
}

# 检查后端服务
check_backend() {
    print_info "检查后端服务连接..."
    
    # 检查API服务 (默认8080端口)
    if curl -s --connect-timeout 3 http://localhost:8080/api/system/status > /dev/null 2>&1; then
        print_success "后端API服务连接正常 (localhost:8080)"
    else
        print_warning "后端API服务未响应 (localhost:8080)"
        print_warning "请确保C++后端服务已启动"
    fi
    
    # 检查视频流服务 (默认8161端口)
    if curl -s --connect-timeout 3 http://localhost:8161 > /dev/null 2>&1; then
        print_success "视频流服务连接正常 (localhost:8161)"
    else
        print_warning "视频流服务未响应 (localhost:8161)"
        print_warning "请确保视频流服务已启动"
    fi
}

# 启动开发服务器
start_dev_server() {
    print_info "启动开发服务器..."
    
    if [ "$PACKAGE_MANAGER" = "yarn" ]; then
        yarn dev
    else
        npm run dev
    fi
}

# 构建生产版本
build_production() {
    print_info "构建生产版本..."
    
    if [ "$PACKAGE_MANAGER" = "yarn" ]; then
        yarn build
    else
        npm run build
    fi
    
    print_success "构建完成，输出目录: dist/"
}

# 启动生产预览
start_preview() {
    print_info "启动生产预览服务器..."
    
    if [ "$PACKAGE_MANAGER" = "yarn" ]; then
        yarn preview
    else
        npm run preview
    fi
}

# 显示帮助信息
show_help() {
    echo "AI安防视频监控系统 Web UI 启动脚本"
    echo ""
    echo "用法: $0 [选项]"
    echo ""
    echo "选项:"
    echo "  dev, -d, --dev        启动开发服务器 (默认)"
    echo "  build, -b, --build    构建生产版本"
    echo "  preview, -p, --preview 启动生产预览服务器"
    echo "  help, -h, --help      显示帮助信息"
    echo ""
    echo "示例:"
    echo "  $0                    # 启动开发服务器"
    echo "  $0 dev                # 启动开发服务器"
    echo "  $0 build              # 构建生产版本"
    echo "  $0 preview            # 启动生产预览"
    echo ""
}

# 主函数
main() {
    print_info "AI安防视频监控系统 Web UI 启动脚本"
    print_info "========================================"
    
    # 检查环境
    check_node
    check_package_manager
    
    # 安装依赖
    install_dependencies
    
    # 检查后端服务
    check_backend
    
    # 根据参数执行相应操作
    case "${1:-dev}" in
        "dev"|"-d"|"--dev")
            start_dev_server
            ;;
        "build"|"-b"|"--build")
            build_production
            ;;
        "preview"|"-p"|"--preview")
            if [ ! -d "dist" ]; then
                print_warning "dist 目录不存在，先构建生产版本..."
                build_production
            fi
            start_preview
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
trap 'print_error "脚本执行失败"; exit 1' ERR

# 检查是否在正确的目录
if [ ! -f "package.json" ]; then
    print_error "请在 web-ui 目录下运行此脚本"
    exit 1
fi

# 执行主函数
main "$@"
