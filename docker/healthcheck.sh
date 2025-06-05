#!/bin/bash
# AI Security Vision System - Health Check Script
# 这个脚本用于Docker容器的健康检查

set -e

# 配置
API_URL="http://localhost:8080"
TIMEOUT=10
MAX_RETRIES=3

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# 日志函数
log_info() {
    echo -e "${GREEN}[HEALTH]${NC} $1"
}

log_warn() {
    echo -e "${YELLOW}[HEALTH]${NC} $1"
}

log_error() {
    echo -e "${RED}[HEALTH]${NC} $1"
}

# 检查API端点
check_api_endpoint() {
    local endpoint="$1"
    local description="$2"
    
    if curl -f -s --max-time "$TIMEOUT" "${API_URL}${endpoint}" >/dev/null 2>&1; then
        return 0
    else
        return 1
    fi
}

# 检查系统状态
check_system_status() {
    log_info "Checking system status..."
    
    local response
    response=$(curl -f -s --max-time "$TIMEOUT" "${API_URL}/api/system/status" 2>/dev/null)
    
    if [[ $? -eq 0 ]] && [[ -n "$response" ]]; then
        # 检查响应是否包含预期的字段
        if echo "$response" | grep -q '"success".*true' && echo "$response" | grep -q '"status"'; then
            log_info "System status check passed"
            return 0
        else
            log_error "System status response invalid"
            return 1
        fi
    else
        log_error "System status check failed"
        return 1
    fi
}

# 检查数据库连接
check_database() {
    log_info "Checking database connectivity..."
    
    if check_api_endpoint "/api/system/info" "Database connectivity"; then
        log_info "Database connectivity check passed"
        return 0
    else
        log_error "Database connectivity check failed"
        return 1
    fi
}

# 检查AI推理
check_ai_inference() {
    log_info "Checking AI inference capability..."
    
    if check_api_endpoint "/api/detection/config" "AI inference"; then
        log_info "AI inference check passed"
        return 0
    else
        log_warn "AI inference check failed (may be normal if no cameras configured)"
        return 0  # 不强制要求AI推理正常
    fi
}

# 检查内存使用
check_memory_usage() {
    log_info "Checking memory usage..."
    
    local memory_usage
    memory_usage=$(free | grep Mem | awk '{printf "%.1f", $3/$2 * 100.0}')
    
    if (( $(echo "$memory_usage > 90" | bc -l) )); then
        log_warn "High memory usage: ${memory_usage}%"
        return 1
    else
        log_info "Memory usage: ${memory_usage}%"
        return 0
    fi
}

# 检查磁盘空间
check_disk_space() {
    log_info "Checking disk space..."
    
    local disk_usage
    disk_usage=$(df /opt/aibox | tail -1 | awk '{print $5}' | sed 's/%//')
    
    if [[ "$disk_usage" -gt 90 ]]; then
        log_warn "High disk usage: ${disk_usage}%"
        return 1
    else
        log_info "Disk usage: ${disk_usage}%"
        return 0
    fi
}

# 检查进程状态
check_process() {
    log_info "Checking process status..."
    
    if pgrep -f "AISecurityVision" >/dev/null; then
        log_info "Main process is running"
        return 0
    else
        log_error "Main process not found"
        return 1
    fi
}

# 检查端口监听
check_ports() {
    log_info "Checking port availability..."
    
    local ports=(8080 8161 8162 8163 8164)
    local failed_ports=()
    
    for port in "${ports[@]}"; do
        if netstat -ln 2>/dev/null | grep -q ":${port} "; then
            log_info "Port $port is listening"
        else
            log_warn "Port $port is not listening"
            failed_ports+=("$port")
        fi
    done
    
    # API端口必须监听
    if [[ " ${failed_ports[@]} " =~ " 8080 " ]]; then
        log_error "Critical port 8080 (API) is not listening"
        return 1
    fi
    
    return 0
}

# 检查日志文件
check_logs() {
    log_info "Checking log files..."
    
    local log_dir="/opt/aibox/logs"
    
    if [[ -d "$log_dir" ]]; then
        # 检查是否有最近的日志
        local recent_logs
        recent_logs=$(find "$log_dir" -name "*.log" -mmin -5 2>/dev/null | wc -l)
        
        if [[ "$recent_logs" -gt 0 ]]; then
            log_info "Recent log files found"
            return 0
        else
            log_warn "No recent log files found"
            return 0  # 不强制要求有日志文件
        fi
    else
        log_warn "Log directory not found"
        return 0
    fi
}

# 综合健康检查
comprehensive_health_check() {
    log_info "Starting comprehensive health check..."
    
    local checks=(
        "check_process"
        "check_ports"
        "check_system_status"
        "check_database"
        "check_ai_inference"
        "check_memory_usage"
        "check_disk_space"
        "check_logs"
    )
    
    local failed_checks=()
    local critical_failed=false
    
    for check in "${checks[@]}"; do
        if ! $check; then
            failed_checks+=("$check")
            
            # 关键检查失败
            if [[ "$check" == "check_process" ]] || [[ "$check" == "check_ports" ]] || [[ "$check" == "check_system_status" ]]; then
                critical_failed=true
            fi
        fi
    done
    
    # 输出结果
    if [[ ${#failed_checks[@]} -eq 0 ]]; then
        log_info "All health checks passed ✅"
        return 0
    elif [[ "$critical_failed" == true ]]; then
        log_error "Critical health checks failed: ${failed_checks[*]}"
        return 1
    else
        log_warn "Some non-critical health checks failed: ${failed_checks[*]}"
        return 0
    fi
}

# 快速健康检查
quick_health_check() {
    log_info "Starting quick health check..."
    
    # 只检查关键组件
    if check_process && check_system_status; then
        log_info "Quick health check passed ✅"
        return 0
    else
        log_error "Quick health check failed ❌"
        return 1
    fi
}

# 主函数
main() {
    local check_type="${1:-quick}"
    
    case "$check_type" in
        quick)
            quick_health_check
            ;;
        comprehensive|full)
            comprehensive_health_check
            ;;
        api)
            check_system_status
            ;;
        process)
            check_process
            ;;
        ports)
            check_ports
            ;;
        memory)
            check_memory_usage
            ;;
        disk)
            check_disk_space
            ;;
        *)
            log_error "Unknown check type: $check_type"
            echo "Usage: $0 {quick|comprehensive|api|process|ports|memory|disk}"
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@"
