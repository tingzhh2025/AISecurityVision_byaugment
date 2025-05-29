#!/usr/bin/env python3
"""
简单的RKNN YOLOv8性能测试
测试修复后的检测速度和质量
"""

import time
import numpy as np
import cv2
from rknnlite.api import RKNNLite

def test_rknn_performance():
    print("=== RKNN YOLOv8 性能测试 ===")
    
    # 检查模型文件
    model_path = "models/yolov8n.rknn"
    try:
        with open(model_path, 'rb') as f:
            model_data = f.read()
        print(f"✓ 模型文件加载成功: {model_path} ({len(model_data)} bytes)")
    except FileNotFoundError:
        print(f"❌ 模型文件未找到: {model_path}")
        return False
    
    # 初始化RKNN
    rknn = RKNNLite()
    
    # 加载模型
    print("正在加载RKNN模型...")
    ret = rknn.load_rknn(model_path)
    if ret != 0:
        print(f"❌ 模型加载失败: {ret}")
        return False
    
    # 初始化运行时
    print("正在初始化RKNN运行时...")
    ret = rknn.init_runtime(core_mask=RKNNLite.NPU_CORE_0_1_2)
    if ret != 0:
        print(f"❌ 运行时初始化失败: {ret}")
        return False
    
    print("✓ RKNN初始化成功，使用所有NPU核心")
    
    # 创建测试图像
    test_image = np.random.randint(0, 255, (640, 640, 3), dtype=np.uint8)
    
    # 预处理
    def preprocess(image):
        # 转换为RGB
        image_rgb = cv2.cvtColor(image, cv2.COLOR_BGR2RGB)
        # 归一化到[0,1]
        image_norm = image_rgb.astype(np.float32) / 255.0
        # 转换为NCHW格式
        image_input = np.transpose(image_norm, (2, 0, 1))
        image_input = np.expand_dims(image_input, axis=0)
        return image_input
    
    # 预热
    print("预热中...")
    input_data = preprocess(test_image)
    for i in range(10):
        outputs = rknn.inference(inputs=[input_data])
    
    # 性能测试
    print("开始性能测试...")
    test_frames = 100
    inference_times = []
    
    for i in range(test_frames):
        start_time = time.time()
        
        # 推理
        outputs = rknn.inference(inputs=[input_data])
        
        end_time = time.time()
        inference_time = (end_time - start_time) * 1000  # 转换为毫秒
        inference_times.append(inference_time)
        
        if i % 10 == 0:
            print(f"Frame {i}: {inference_time:.2f}ms")
    
    # 计算统计信息
    avg_time = np.mean(inference_times)
    min_time = np.min(inference_times)
    max_time = np.max(inference_times)
    std_time = np.std(inference_times)
    fps = 1000.0 / avg_time
    
    print("\n=== 性能测试结果 ===")
    print(f"测试帧数: {test_frames}")
    print(f"平均推理时间: {avg_time:.2f} ms")
    print(f"最小推理时间: {min_time:.2f} ms")
    print(f"最大推理时间: {max_time:.2f} ms")
    print(f"标准差: {std_time:.2f} ms")
    print(f"平均FPS: {fps:.1f}")
    
    # 性能评估
    print("\n=== 性能评估 ===")
    if avg_time <= 50.0:
        print("✅ 优秀: 性能达到RK3588预期!")
        print("   符合YOLOv8n在RK3588上的性能目标")
    elif avg_time <= 100.0:
        print("✅ 良好: 性能可接受")
    elif avg_time <= 200.0:
        print("⚠️  一般: 性能有改善空间")
    else:
        print("❌ 较差: 性能需要优化")
        print("   期望: ~13-50ms (YOLOv8n)")
        print(f"   实际: {avg_time:.2f}ms")
    
    # 检查输出格式
    if outputs and len(outputs) > 0:
        output_shape = outputs[0].shape
        print(f"\n输出张量形状: {output_shape}")
        
        # 检查是否有检测结果
        if len(output_shape) >= 2:
            print("✓ 输出格式正常")
        else:
            print("⚠️  输出格式异常")
    
    # 清理
    rknn.release()
    print("\n✓ 测试完成")
    
    return avg_time <= 100.0  # 100ms以内认为成功

if __name__ == "__main__":
    success = test_rknn_performance()
    exit(0 if success else 1)
