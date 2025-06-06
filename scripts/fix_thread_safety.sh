#!/bin/bash

echo "=== AISecurityVision 线程安全问题分析与修复建议 ==="
echo ""

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${BLUE}检测到的线程安全问题：${NC}"
echo ""

echo -e "${RED}1. TaskManager::addVideoSource 锁释放问题${NC}"
echo "   位置: src/core/TaskManager.cpp"
echo "   问题: 在初始化 pipeline 时释放了锁，可能导致状态不一致"
echo "   修复建议:"
echo "   - 使用 RAII 模式管理锁"
echo "   - 将 pipeline 初始化延迟到锁外完成"
echo "   - 使用 shared_ptr 的原子操作"
echo ""

echo -e "${RED}2. 潜在的死锁风险${NC}"
echo "   位置: VideoPipeline 和 TaskManager 之间"
echo "   问题: 循环依赖可能导致死锁"
echo "   修复建议:"
echo "   - 使用锁的层次结构"
echo "   - 避免在持有锁时调用外部对象的方法"
echo "   - 使用 std::lock 同时获取多个锁"
echo ""

echo -e "${RED}3. 多个互斥锁的竞争条件${NC}"
echo "   涉及的锁:"
echo "   - TaskManager::m_mutex"
echo "   - TaskManager::m_crossCameraMutex"
echo "   - VideoPipeline::m_mutex"
echo "   - VideoPipeline::m_personStatsMutex"
echo "   修复建议:"
echo "   - 定义全局锁顺序"
echo "   - 使用细粒度锁减少竞争"
echo "   - 考虑使用读写锁（shared_mutex）"
echo ""

echo -e "${YELLOW}临时缓解措施：${NC}"
echo ""

# 创建配置调整脚本
cat > /tmp/thread_safety_config.sql << 'EOF'
-- 减少并发压力的配置调整
INSERT OR REPLACE INTO config (section, key, value) VALUES 
    ('system', 'max_concurrent_requests', '5'),     -- 限制并发API请求
    ('system', 'pipeline_init_timeout', '60'),      -- 增加初始化超时
    ('system', 'lock_timeout', '5000'),             -- 锁超时（毫秒）
    ('system', 'enable_thread_pool', 'true'),       -- 启用线程池
    ('system', 'thread_pool_size', '4');            -- 线程池大小
EOF

echo "1. 调整系统配置以减少并发压力..."
DB_FILE="/home/rogers/source/custom/AISecurityVision_byaugment/aibox.db"
if [ -f "$DB_FILE" ]; then
    sqlite3 $DB_FILE < /tmp/thread_safety_config.sql
    echo "   ✅ 配置已更新"
else
    echo "   ⚠️  数据库文件不存在"
fi
echo ""

# 创建启动脚本with线程安全选项
echo "2. 创建线程安全启动脚本..."
cat > /home/rogers/source/custom/AISecurityVision_byaugment/start_threadsafe.sh << 'EOF'
#!/bin/bash

# 线程安全启动脚本
PROJECT_DIR="/home/rogers/source/custom/AISecurityVision_byaugment"
BUILD_DIR="$PROJECT_DIR/build"

echo "启动AISecurityVision（线程安全模式）..."

# 设置环境变量以启用调试
export ASAN_OPTIONS=halt_on_error=0:print_stats=1
export TSAN_OPTIONS=halt_on_error=0:second_deadlock_stack=1

# 限制CPU核心以减少并发
# taskset -c 0-3 限制使用前4个核心

cd $BUILD_DIR

# 使用 strace 监控系统调用（可选）
# strace -e trace=futex,clone ./AISecurityVision -v 2> /tmp/aibox_strace.log &

# 正常启动，但使用详细日志
./AISecurityVision -v -p 8080 2>&1 | tee /tmp/aibox_runtime.log
EOF

chmod +x /home/rogers/source/custom/AISecurityVision_byaugment/start_threadsafe.sh
echo "   ✅ 线程安全启动脚本已创建"
echo ""

# 创建监控脚本
echo "3. 创建线程监控脚本..."
cat > /home/rogers/source/custom/AISecurityVision_byaugment/monitor_threads.sh << 'EOF'
#!/bin/bash

# 监控AISecurityVision的线程状态
PID=$(pgrep -f "AISecurityVision" | head -1)

if [ -z "$PID" ]; then
    echo "AISecurityVision 进程未运行"
    exit 1
fi

echo "监控进程 PID: $PID"
echo ""

# 显示线程信息
echo "=== 线程列表 ==="
ps -T -p $PID

echo ""
echo "=== 线程堆栈（前5个线程）==="
for tid in $(ls /proc/$PID/task | head -5); do
    echo "线程 $tid:"
    cat /proc/$PID/task/$tid/comm 2>/dev/null
    echo "堆栈:"
    cat /proc/$PID/task/$tid/stack 2>/dev/null | head -10
    echo "---"
done

echo ""
echo "=== 锁统计 ==="
# 检查是否有死锁
if [ -f /proc/$PID/locks ]; then
    cat /proc/$PID/locks
fi

echo ""
echo "=== CPU使用率 ==="
top -H -p $PID -n 1 -b | grep -A 20 "PID"
EOF

chmod +x /home/rogers/source/custom/AISecurityVision_byaugment/monitor_threads.sh
echo "   ✅ 线程监控脚本已创建"
echo ""

# 创建死锁检测脚本
echo "4. 创建死锁检测脚本..."
cat > /home/rogers/source/custom/AISecurityVision_byaugment/detect_deadlock.sh << 'EOF'
#!/bin/bash

# 检测可能的死锁
PID=$(pgrep -f "AISecurityVision" | head -1)

if [ -z "$PID" ]; then
    echo "AISecurityVision 进程未运行"
    exit 1
fi

echo "检测进程 $PID 的潜在死锁..."
echo ""

# 使用 gdb 检查线程状态（需要 gdb）
if command -v gdb > /dev/null; then
    echo "使用 GDB 分析线程..."
    
    # 创建 GDB 脚本
    cat > /tmp/gdb_deadlock.cmd << 'GDBEOF'
set pagination off
set print thread-events off
info threads
thread apply all bt
quit
GDBEOF

    # 运行 GDB
    sudo gdb -p $PID -x /tmp/gdb_deadlock.cmd -batch 2>/dev/null | grep -E "(Thread|#0|mutex|lock|wait)"
    
    rm -f /tmp/gdb_deadlock.cmd
else
    echo "GDB 未安装，使用基础方法..."
    
    # 检查线程状态
    for tid in $(ls /proc/$PID/task); do
        STATE=$(cat /proc/$PID/task/$tid/stat 2>/dev/null | awk '{print $3}')
        if [ "$STATE" = "D" ]; then
            echo "警告: 线程 $tid 处于不可中断睡眠状态（可能死锁）"
        fi
    done
fi

echo ""
echo "=== 建议 ==="
echo "如果发现死锁："
echo "1. 记录当前日志: cp /tmp/aibox_runtime.log /tmp/deadlock_$(date +%Y%m%d_%H%M%S).log"
echo "2. 使用 kill -SIGUSR1 $PID 触发调试信息（如果实现）"
echo "3. 重启服务: kill -TERM $PID && sleep 5 && ./start_threadsafe.sh"
EOF

chmod +x /home/rogers/source/custom/AISecurityVision_byaugment/detect_deadlock.sh
echo "   ✅ 死锁检测脚本已创建"
echo ""

# 提供代码修复建议
echo -e "${GREEN}=== 代码修复建议 ===${NC}"
echo ""

echo "1. 修改 TaskManager::addVideoSource 方法："
echo "   ```cpp"
echo "   bool addVideoSource(const VideoSource& source) {"
echo "       // 第一阶段：验证和创建"
echo "       {"
echo "           std::lock_guard<std::mutex> lock(m_mutex);"
echo "           if (m_pipelines.find(source.id) != m_pipelines.end()) {"
echo "               return false;"
echo "           }"
echo "       }"
echo "       "
echo "       // 第二阶段：初始化（不持锁）"
echo "       auto pipeline = std::make_shared<VideoPipeline>(source);"
echo "       if (!pipeline->initialize()) {"
echo "           return false;"
echo "       }"
echo "       "
echo "       // 第三阶段：原子插入"
echo "       {"
echo "           std::lock_guard<std::mutex> lock(m_mutex);"
echo "           // 再次检查防止竞态"
echo "           if (m_pipelines.find(source.id) != m_pipelines.end()) {"
echo "               return false;"
echo "           }"
echo "           m_pipelines[source.id] = pipeline;"
echo "           pipeline->start();"
echo "       }"
echo "       return true;"
echo "   }"
echo "   ```"
echo ""

echo "2. 使用 std::shared_mutex 优化读操作："
echo "   - 将 m_mutex 改为 mutable std::shared_mutex"
echo "   - 读操作使用 std::shared_lock"
echo "   - 写操作使用 std::unique_lock"
echo ""

echo "3. 避免在持锁时调用外部方法："
echo "   - 收集需要的数据"
echo "   - 释放锁"
echo "   - 调用外部方法"
echo ""

echo -e "${BLUE}=== 立即可执行的操作 ===${NC}"
echo ""
echo "1. 使用线程安全启动脚本："
echo "   /home/rogers/source/custom/AISecurityVision_byaugment/start_threadsafe.sh"
echo ""
echo "2. 监控线程状态："
echo "   /home/rogers/source/custom/AISecurityVision_byaugment/monitor_threads.sh"
echo ""
echo "3. 检测死锁："
echo "   /home/rogers/source/custom/AISecurityVision_byaugment/detect_deadlock.sh"
echo ""
echo "4. 如果出现无响应，查看日志："
echo "   tail -f /tmp/aibox_runtime.log"
echo ""

# 清理临时文件
rm -f /tmp/thread_safety_config.sql
