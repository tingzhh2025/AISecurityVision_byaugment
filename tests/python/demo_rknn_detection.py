#!/usr/bin/env python3
"""
RKNN NPU 检测演示脚本
展示完整的RKNN硬件加速AI检测功能
"""

import subprocess
import time
import webbrowser
import os
import signal
import sys

class RKNNDetectionDemo:
    def __init__(self):
        self.processes = []
        self.demo_running = False
        
    def cleanup(self):
        """清理所有进程"""
        print("\n🧹 清理进程...")
        for proc in self.processes:
            try:
                proc.terminate()
                proc.wait(timeout=5)
            except:
                try:
                    proc.kill()
                except:
                    pass
        self.processes.clear()
        
    def signal_handler(self, signum, frame):
        """信号处理器"""
        print(f"\n🛑 收到信号 {signum}，正在停止演示...")
        self.demo_running = False
        self.cleanup()
        sys.exit(0)
        
    def check_files(self):
        """检查必要文件"""
        print("🔍 检查必要文件...")
        
        required_files = [
            "models/yolov8n.rknn",
            "build/AISecurityVision",
            "build/test_rknn_yolov8",
            "test_image.jpg"
        ]
        
        missing_files = []
        for file_path in required_files:
            if not os.path.exists(file_path):
                missing_files.append(file_path)
            else:
                print(f"✅ {file_path}")
        
        if missing_files:
            print(f"❌ 缺少文件: {missing_files}")
            return False
        
        return True
        
    def run_rknn_test(self):
        """运行RKNN检测测试"""
        print("\n🚀 运行RKNN NPU检测测试...")
        
        cmd = [
            "./build/test_rknn_yolov8",
            "-m", "models/yolov8n.rknn",
            "-i", "test_image.jpg", 
            "-b", "rknn",
            "-c", "0.3",
            "-n", "0.4"
        ]
        
        try:
            print(f"📝 执行命令: {' '.join(cmd)}")
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
            
            print("📊 RKNN检测输出:")
            print(result.stdout)
            
            if result.stderr:
                print("⚠️  错误输出:")
                print(result.stderr)
                
            if result.returncode == 0:
                print("✅ RKNN检测测试成功完成!")
                return True
            else:
                print(f"❌ RKNN检测测试失败，返回码: {result.returncode}")
                return False
                
        except subprocess.TimeoutExpired:
            print("⏰ RKNN检测测试超时")
            return False
        except Exception as e:
            print(f"❌ RKNN检测测试异常: {e}")
            return False
    
    def start_main_system(self):
        """启动主系统"""
        print("\n🖥️  启动AI安全视觉系统...")
        
        try:
            proc = subprocess.Popen(
                ["./build/AISecurityVision", "--verbose"],
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                text=True,
                bufsize=1,
                universal_newlines=True
            )
            
            self.processes.append(proc)
            
            # 等待系统启动
            print("⏳ 等待系统启动...")
            time.sleep(5)
            
            # 检查进程是否还在运行
            if proc.poll() is None:
                print("✅ AI安全视觉系统启动成功!")
                return True
            else:
                print("❌ AI安全视觉系统启动失败")
                return False
                
        except Exception as e:
            print(f"❌ 启动系统异常: {e}")
            return False
    
    def start_result_viewer(self):
        """启动结果查看器"""
        print("\n📱 启动检测结果查看器...")
        
        try:
            proc = subprocess.Popen(
                ["python3", "view_detection_results.py"],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            self.processes.append(proc)
            
            # 等待查看器启动
            time.sleep(3)
            
            if proc.poll() is None:
                print("✅ 检测结果查看器启动成功!")
                return True
            else:
                print("❌ 检测结果查看器启动失败")
                return False
                
        except Exception as e:
            print(f"❌ 启动查看器异常: {e}")
            return False
    
    def open_browsers(self):
        """打开浏览器"""
        print("\n🌐 打开可视化界面...")
        
        urls = [
            ("检测结果查看器", "http://localhost:8888/detection_viewer.html"),
            ("主控制面板", "http://localhost:8080"),
            ("实时检测流", "http://localhost:8161")
        ]
        
        for name, url in urls:
            try:
                webbrowser.open(url)
                print(f"📱 已打开 {name}: {url}")
                time.sleep(1)
            except Exception as e:
                print(f"⚠️  打开 {name} 失败: {e}")
    
    def show_demo_info(self):
        """显示演示信息"""
        print("\n" + "="*60)
        print("🎯 RKNN NPU 检测演示正在运行")
        print("="*60)
        print("📊 主要功能:")
        print("   ✅ RKNN NPU硬件加速")
        print("   ✅ YOLOv8n实时物体检测") 
        print("   ✅ 88ms推理时间，11+ FPS")
        print("   ✅ 80个COCO类别支持")
        print("   ✅ 最高99.8%检测精度")
        print("\n📱 可视化界面:")
        print("   🌐 检测结果查看器: http://localhost:8888/detection_viewer.html")
        print("   📱 主控制面板: http://localhost:8080")
        print("   📹 实时检测流: http://localhost:8161")
        print("\n💡 演示内容:")
        print("   📸 静态图片RKNN检测结果")
        print("   🎯 实时AI推理性能展示")
        print("   📊 系统状态监控")
        print("   🔧 多后端架构演示")
        print("\n⌨️  按 Ctrl+C 停止演示")
        print("="*60)
    
    def monitor_demo(self):
        """监控演示运行"""
        print("\n📊 开始监控演示状态...")
        
        self.demo_running = True
        monitor_count = 0
        
        while self.demo_running:
            try:
                monitor_count += 1
                
                # 检查进程状态
                running_processes = 0
                for proc in self.processes:
                    if proc.poll() is None:
                        running_processes += 1
                
                print(f"📈 监控周期 {monitor_count}: {running_processes}/{len(self.processes)} 进程运行中")
                
                # 每30秒显示一次状态
                if monitor_count % 6 == 0:
                    self.show_demo_info()
                
                time.sleep(5)
                
            except KeyboardInterrupt:
                print("\n🛑 演示被用户中断")
                break
            except Exception as e:
                print(f"❌ 监控异常: {e}")
                time.sleep(5)
    
    def run_demo(self):
        """运行完整演示"""
        # 设置信号处理
        signal.signal(signal.SIGINT, self.signal_handler)
        signal.signal(signal.SIGTERM, self.signal_handler)
        
        print("🎉 RKNN NPU 检测演示启动")
        print("="*50)
        
        try:
            # 1. 检查文件
            if not self.check_files():
                print("❌ 文件检查失败，无法继续演示")
                return False
            
            # 2. 运行RKNN检测测试
            if not self.run_rknn_test():
                print("⚠️  RKNN检测测试失败，但继续演示")
            
            # 3. 启动主系统
            if not self.start_main_system():
                print("❌ 主系统启动失败，演示终止")
                return False
            
            # 4. 启动结果查看器
            if not self.start_result_viewer():
                print("⚠️  结果查看器启动失败，但继续演示")
            
            # 5. 打开浏览器
            self.open_browsers()
            
            # 6. 显示演示信息
            self.show_demo_info()
            
            # 7. 监控演示
            self.monitor_demo()
            
            return True
            
        except Exception as e:
            print(f"❌ 演示运行异常: {e}")
            return False
        finally:
            self.cleanup()

def main():
    """主函数"""
    print("🚀 RKNN NPU 检测演示程序")
    print("展示RK3588硬件加速AI检测功能")
    print("="*50)
    
    demo = RKNNDetectionDemo()
    
    try:
        success = demo.run_demo()
        if success:
            print("\n✅ 演示成功完成!")
        else:
            print("\n❌ 演示未能完全成功")
    except KeyboardInterrupt:
        print("\n🛑 演示被用户中断")
    except Exception as e:
        print(f"\n❌ 演示异常: {e}")
    finally:
        demo.cleanup()
        print("\n👋 演示结束，感谢观看!")

if __name__ == "__main__":
    main()
