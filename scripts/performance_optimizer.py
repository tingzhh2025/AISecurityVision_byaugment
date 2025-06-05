#!/usr/bin/env python3
"""
AI Security Vision System - Performance Optimizer

这个脚本用于优化系统性能：
- RKNN NPU性能调优
- 数据库查询优化
- 内存使用优化
- 网络连接优化
- 系统资源监控
"""

import os
import sys
import json
import time
import psutil
import sqlite3
import subprocess
import argparse
from datetime import datetime, timedelta
from typing import Dict, List, Any, Optional
from dataclasses import dataclass
import requests

@dataclass
class PerformanceMetrics:
    """性能指标数据类"""
    timestamp: datetime
    cpu_usage: float
    memory_usage: float
    disk_usage: float
    network_io: Dict[str, int]
    api_response_time: float
    inference_time: float
    database_query_time: float
    active_connections: int

class PerformanceOptimizer:
    """性能优化器"""
    
    def __init__(self, api_url: str = "http://localhost:8080", db_path: str = "aibox.db"):
        self.api_url = api_url
        self.db_path = db_path
        self.metrics_history: List[PerformanceMetrics] = []
        
    def collect_system_metrics(self) -> PerformanceMetrics:
        """收集系统性能指标"""
        # CPU使用率
        cpu_usage = psutil.cpu_percent(interval=1)
        
        # 内存使用率
        memory = psutil.virtual_memory()
        memory_usage = memory.percent
        
        # 磁盘使用率
        disk = psutil.disk_usage('/')
        disk_usage = (disk.used / disk.total) * 100
        
        # 网络IO
        network = psutil.net_io_counters()
        network_io = {
            'bytes_sent': network.bytes_sent,
            'bytes_recv': network.bytes_recv,
            'packets_sent': network.packets_sent,
            'packets_recv': network.packets_recv
        }
        
        # API响应时间
        api_response_time = self.measure_api_response_time()
        
        # AI推理时间
        inference_time = self.measure_inference_time()
        
        # 数据库查询时间
        database_query_time = self.measure_database_query_time()
        
        # 活跃连接数
        active_connections = self.get_active_connections()
        
        return PerformanceMetrics(
            timestamp=datetime.now(),
            cpu_usage=cpu_usage,
            memory_usage=memory_usage,
            disk_usage=disk_usage,
            network_io=network_io,
            api_response_time=api_response_time,
            inference_time=inference_time,
            database_query_time=database_query_time,
            active_connections=active_connections
        )
    
    def measure_api_response_time(self) -> float:
        """测量API响应时间"""
        try:
            start_time = time.time()
            response = requests.get(f"{self.api_url}/api/system/status", timeout=5)
            end_time = time.time()
            
            if response.status_code == 200:
                return (end_time - start_time) * 1000  # 转换为毫秒
            else:
                return -1
        except Exception:
            return -1
    
    def measure_inference_time(self) -> float:
        """测量AI推理时间"""
        try:
            response = requests.get(f"{self.api_url}/api/detection/stats", timeout=5)
            if response.status_code == 200:
                data = response.json()
                if 'data' in data and 'average_inference_time' in data['data']:
                    return data['data']['average_inference_time']
            return -1
        except Exception:
            return -1
    
    def measure_database_query_time(self) -> float:
        """测量数据库查询时间"""
        try:
            if not os.path.exists(self.db_path):
                return -1
                
            start_time = time.time()
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            cursor.execute("SELECT COUNT(*) FROM sqlite_master WHERE type='table'")
            cursor.fetchone()
            conn.close()
            end_time = time.time()
            
            return (end_time - start_time) * 1000  # 转换为毫秒
        except Exception:
            return -1
    
    def get_active_connections(self) -> int:
        """获取活跃连接数"""
        try:
            response = requests.get(f"{self.api_url}/api/system/metrics", timeout=5)
            if response.status_code == 200:
                data = response.json()
                if 'data' in data and 'active_connections' in data['data']:
                    return data['data']['active_connections']
            return -1
        except Exception:
            return -1
    
    def optimize_rknn_performance(self) -> Dict[str, Any]:
        """优化RKNN NPU性能"""
        print("🚀 Optimizing RKNN NPU performance...")
        
        optimizations = []
        
        try:
            # 设置NPU性能模式
            if os.path.exists("/sys/class/devfreq/fdab0000.npu/governor"):
                with open("/sys/class/devfreq/fdab0000.npu/governor", "w") as f:
                    f.write("performance")
                optimizations.append("Set NPU governor to performance mode")
            
            # 设置NPU频率
            if os.path.exists("/sys/class/devfreq/fdab0000.npu/max_freq"):
                with open("/sys/class/devfreq/fdab0000.npu/max_freq", "r") as f:
                    max_freq = f.read().strip()
                with open("/sys/class/devfreq/fdab0000.npu/min_freq", "w") as f:
                    f.write(max_freq)
                optimizations.append(f"Set NPU frequency to maximum: {max_freq}")
            
            # 优化CMA内存
            try:
                subprocess.run(["echo", "3", ">", "/proc/sys/vm/drop_caches"], 
                             shell=True, check=True)
                optimizations.append("Cleared system caches")
            except subprocess.CalledProcessError:
                pass
            
            print(f"   ✅ Applied {len(optimizations)} RKNN optimizations")
            return {"success": True, "optimizations": optimizations}
            
        except Exception as e:
            print(f"   ❌ RKNN optimization failed: {e}")
            return {"success": False, "error": str(e)}
    
    def optimize_database_performance(self) -> Dict[str, Any]:
        """优化数据库性能"""
        print("🗄️  Optimizing database performance...")
        
        optimizations = []
        
        try:
            if not os.path.exists(self.db_path):
                return {"success": False, "error": "Database file not found"}
            
            conn = sqlite3.connect(self.db_path)
            cursor = conn.cursor()
            
            # 启用WAL模式
            cursor.execute("PRAGMA journal_mode=WAL")
            optimizations.append("Enabled WAL mode")
            
            # 设置缓存大小
            cursor.execute("PRAGMA cache_size=10000")  # 10MB缓存
            optimizations.append("Set cache size to 10MB")
            
            # 设置同步模式
            cursor.execute("PRAGMA synchronous=NORMAL")
            optimizations.append("Set synchronous mode to NORMAL")
            
            # 设置临时存储
            cursor.execute("PRAGMA temp_store=MEMORY")
            optimizations.append("Set temp store to memory")
            
            # 分析表统计信息
            cursor.execute("ANALYZE")
            optimizations.append("Updated table statistics")
            
            # 检查并创建缺失的索引
            tables = cursor.execute("SELECT name FROM sqlite_master WHERE type='table'").fetchall()
            for (table_name,) in tables:
                if table_name == "events":
                    # 为events表创建索引
                    try:
                        cursor.execute("CREATE INDEX IF NOT EXISTS idx_events_timestamp ON events(timestamp)")
                        cursor.execute("CREATE INDEX IF NOT EXISTS idx_events_camera_id ON events(camera_id)")
                        optimizations.append(f"Created indexes for {table_name} table")
                    except sqlite3.Error:
                        pass
            
            conn.commit()
            conn.close()
            
            print(f"   ✅ Applied {len(optimizations)} database optimizations")
            return {"success": True, "optimizations": optimizations}
            
        except Exception as e:
            print(f"   ❌ Database optimization failed: {e}")
            return {"success": False, "error": str(e)}
    
    def optimize_system_performance(self) -> Dict[str, Any]:
        """优化系统性能"""
        print("⚙️  Optimizing system performance...")
        
        optimizations = []
        
        try:
            # 设置CPU调度器
            try:
                subprocess.run(["echo", "performance", ">", "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"], 
                             shell=True, check=True)
                optimizations.append("Set CPU governor to performance")
            except subprocess.CalledProcessError:
                pass
            
            # 优化网络参数
            network_params = {
                "/proc/sys/net/core/rmem_max": "134217728",
                "/proc/sys/net/core/wmem_max": "134217728",
                "/proc/sys/net/ipv4/tcp_rmem": "4096 87380 134217728",
                "/proc/sys/net/ipv4/tcp_wmem": "4096 65536 134217728",
                "/proc/sys/net/core/netdev_max_backlog": "5000"
            }
            
            for param, value in network_params.items():
                try:
                    with open(param, "w") as f:
                        f.write(value)
                    optimizations.append(f"Set {param} = {value}")
                except (OSError, IOError):
                    pass
            
            # 设置进程优先级
            try:
                pid = subprocess.check_output(["pgrep", "-f", "AISecurityVision"]).decode().strip()
                if pid:
                    subprocess.run(["renice", "-10", pid], check=True)
                    optimizations.append("Increased process priority")
            except (subprocess.CalledProcessError, FileNotFoundError):
                pass
            
            print(f"   ✅ Applied {len(optimizations)} system optimizations")
            return {"success": True, "optimizations": optimizations}
            
        except Exception as e:
            print(f"   ❌ System optimization failed: {e}")
            return {"success": False, "error": str(e)}
    
    def monitor_performance(self, duration_minutes: int = 10) -> Dict[str, Any]:
        """监控系统性能"""
        print(f"📊 Monitoring performance for {duration_minutes} minutes...")
        
        start_time = datetime.now()
        end_time = start_time + timedelta(minutes=duration_minutes)
        
        metrics_list = []
        
        while datetime.now() < end_time:
            metrics = self.collect_system_metrics()
            metrics_list.append(metrics)
            self.metrics_history.append(metrics)
            
            print(f"   CPU: {metrics.cpu_usage:.1f}%, "
                  f"Memory: {metrics.memory_usage:.1f}%, "
                  f"API: {metrics.api_response_time:.1f}ms, "
                  f"Inference: {metrics.inference_time:.1f}ms")
            
            time.sleep(30)  # 每30秒收集一次
        
        # 计算统计信息
        if metrics_list:
            avg_cpu = sum(m.cpu_usage for m in metrics_list) / len(metrics_list)
            avg_memory = sum(m.memory_usage for m in metrics_list) / len(metrics_list)
            avg_api_time = sum(m.api_response_time for m in metrics_list if m.api_response_time > 0) / len([m for m in metrics_list if m.api_response_time > 0])
            avg_inference_time = sum(m.inference_time for m in metrics_list if m.inference_time > 0) / len([m for m in metrics_list if m.inference_time > 0])
            
            stats = {
                "duration_minutes": duration_minutes,
                "samples_collected": len(metrics_list),
                "average_cpu_usage": avg_cpu,
                "average_memory_usage": avg_memory,
                "average_api_response_time": avg_api_time,
                "average_inference_time": avg_inference_time,
                "peak_cpu_usage": max(m.cpu_usage for m in metrics_list),
                "peak_memory_usage": max(m.memory_usage for m in metrics_list)
            }
            
            print(f"   📈 Performance Summary:")
            print(f"      Average CPU: {avg_cpu:.1f}%")
            print(f"      Average Memory: {avg_memory:.1f}%")
            print(f"      Average API Response: {avg_api_time:.1f}ms")
            print(f"      Average Inference: {avg_inference_time:.1f}ms")
            
            return {"success": True, "stats": stats, "metrics": metrics_list}
        else:
            return {"success": False, "error": "No metrics collected"}
    
    def generate_performance_report(self) -> str:
        """生成性能报告"""
        if not self.metrics_history:
            return "No performance data available"
        
        report = f"""
# AI Security Vision Performance Report

Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
Monitoring Period: {len(self.metrics_history)} samples

## System Performance Summary

"""
        
        if self.metrics_history:
            latest = self.metrics_history[-1]
            avg_cpu = sum(m.cpu_usage for m in self.metrics_history) / len(self.metrics_history)
            avg_memory = sum(m.memory_usage for m in self.metrics_history) / len(self.metrics_history)
            
            report += f"""
- **Current CPU Usage**: {latest.cpu_usage:.1f}%
- **Current Memory Usage**: {latest.memory_usage:.1f}%
- **Current Disk Usage**: {latest.disk_usage:.1f}%
- **Average CPU Usage**: {avg_cpu:.1f}%
- **Average Memory Usage**: {avg_memory:.1f}%

## API Performance

- **Latest API Response Time**: {latest.api_response_time:.1f}ms
- **Latest Inference Time**: {latest.inference_time:.1f}ms
- **Latest Database Query Time**: {latest.database_query_time:.1f}ms
- **Active Connections**: {latest.active_connections}

## Recommendations

"""
            
            # 性能建议
            if avg_cpu > 80:
                report += "- ⚠️  High CPU usage detected. Consider optimizing AI inference or reducing camera load.\n"
            if avg_memory > 80:
                report += "- ⚠️  High memory usage detected. Consider increasing system memory or optimizing memory usage.\n"
            if latest.api_response_time > 100:
                report += "- ⚠️  Slow API response time. Consider optimizing database queries or enabling caching.\n"
            if latest.inference_time > 100:
                report += "- ⚠️  Slow AI inference. Consider optimizing RKNN NPU settings or using smaller models.\n"
            
            if avg_cpu < 50 and avg_memory < 50 and latest.api_response_time < 50:
                report += "- ✅ System performance is optimal.\n"
        
        return report

def main():
    """主函数"""
    parser = argparse.ArgumentParser(description="AI Security Vision Performance Optimizer")
    parser.add_argument("--api-url", default="http://localhost:8080", help="API base URL")
    parser.add_argument("--db-path", default="aibox.db", help="Database file path")
    parser.add_argument("--action", choices=["optimize", "monitor", "report"], 
                       default="optimize", help="Action to perform")
    parser.add_argument("--duration", type=int, default=10, 
                       help="Monitoring duration in minutes")
    
    args = parser.parse_args()
    
    optimizer = PerformanceOptimizer(args.api_url, args.db_path)
    
    print("🚀 AI Security Vision Performance Optimizer")
    print("=" * 50)
    
    if args.action == "optimize":
        print("Starting performance optimization...")
        
        # 执行各种优化
        rknn_result = optimizer.optimize_rknn_performance()
        db_result = optimizer.optimize_database_performance()
        sys_result = optimizer.optimize_system_performance()
        
        # 输出结果
        print("\n📊 Optimization Results:")
        print(f"   RKNN NPU: {'✅ Success' if rknn_result['success'] else '❌ Failed'}")
        print(f"   Database: {'✅ Success' if db_result['success'] else '❌ Failed'}")
        print(f"   System: {'✅ Success' if sys_result['success'] else '❌ Failed'}")
        
    elif args.action == "monitor":
        print(f"Starting performance monitoring for {args.duration} minutes...")
        result = optimizer.monitor_performance(args.duration)
        
        if result["success"]:
            print("\n✅ Monitoring completed successfully")
        else:
            print(f"\n❌ Monitoring failed: {result['error']}")
    
    elif args.action == "report":
        print("Generating performance report...")
        
        # 先收集一些指标
        for _ in range(5):
            metrics = optimizer.collect_system_metrics()
            optimizer.metrics_history.append(metrics)
            time.sleep(2)
        
        report = optimizer.generate_performance_report()
        
        # 保存报告
        report_file = f"performance_report_{datetime.now().strftime('%Y%m%d_%H%M%S')}.md"
        with open(report_file, 'w') as f:
            f.write(report)
        
        print(f"📄 Performance report saved to: {report_file}")
        print("\n" + report)

if __name__ == "__main__":
    main()
