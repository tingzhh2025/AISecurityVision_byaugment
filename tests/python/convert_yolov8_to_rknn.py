#!/usr/bin/env python3
"""
YOLOv8 ONNX to RKNN Conversion Script
Converts YOLOv8 ONNX model to RKNN format for RK3588 NPU acceleration
"""

import os
import sys
import numpy as np
from rknnlite.api import RKNNLite

def convert_yolov8_to_rknn(onnx_path, rknn_path, platform='rk3588', quantize=True):
    """
    Convert YOLOv8 ONNX model to RKNN format
    
    Args:
        onnx_path: Path to input ONNX model
        rknn_path: Path to output RKNN model
        platform: Target platform (rk3588)
        quantize: Whether to quantize the model
    """
    
    print(f"Converting {onnx_path} to {rknn_path}")
    print(f"Platform: {platform}")
    print(f"Quantization: {'Enabled' if quantize else 'Disabled'}")
    
    # Initialize RKNN
    rknn = RKNNLite()
    
    try:
        # Load ONNX model
        print("\n1. Loading ONNX model...")
        ret = rknn.load_onnx(model=onnx_path)
        if ret != 0:
            print(f"Load ONNX model failed! ret={ret}")
            return False
        print("✓ ONNX model loaded successfully")
        
        # Build RKNN model
        print("\n2. Building RKNN model...")
        
        # YOLOv8 preprocessing configuration
        # Input: RGB, 0-255, NCHW format
        # Mean: [0, 0, 0], Std: [255, 255, 255] (normalization to 0-1)
        ret = rknn.build(do_quantization=quantize)
        if ret != 0:
            print(f"Build RKNN model failed! ret={ret}")
            return False
        print("✓ RKNN model built successfully")
        
        # Export RKNN model
        print("\n3. Exporting RKNN model...")
        ret = rknn.export_rknn(rknn_path)
        if ret != 0:
            print(f"Export RKNN model failed! ret={ret}")
            return False
        print(f"✓ RKNN model exported to: {rknn_path}")
        
        # Get model info
        model_size = os.path.getsize(rknn_path) / (1024 * 1024)
        print(f"✓ Model size: {model_size:.2f} MB")
        
        return True
        
    except Exception as e:
        print(f"Conversion failed: {e}")
        return False
        
    finally:
        # Release RKNN
        rknn.release()

def test_rknn_model(rknn_path):
    """
    Test the converted RKNN model
    """
    print(f"\n4. Testing RKNN model: {rknn_path}")
    
    rknn = RKNNLite()
    
    try:
        # Load RKNN model
        ret = rknn.load_rknn(rknn_path)
        if ret != 0:
            print(f"Load RKNN model failed! ret={ret}")
            return False
        
        # Initialize runtime
        ret = rknn.init_runtime()
        if ret != 0:
            print(f"Init runtime failed! ret={ret}")
            return False
        
        # Get model input/output info
        inputs = rknn.inputs
        outputs = rknn.outputs
        
        print("✓ Model loaded and runtime initialized")
        print(f"✓ Input shape: {inputs[0].shape}")
        print(f"✓ Input dtype: {inputs[0].dtype}")
        print(f"✓ Output count: {len(outputs)}")
        for i, output in enumerate(outputs):
            print(f"  Output {i}: {output.shape}, {output.dtype}")
        
        # Test inference with dummy data
        input_shape = inputs[0].shape
        dummy_input = np.random.randint(0, 255, input_shape, dtype=np.uint8)
        
        print("\n5. Running test inference...")
        outputs = rknn.inference(inputs=[dummy_input])
        if outputs is None:
            print("Inference failed!")
            return False
        
        print("✓ Test inference successful")
        print(f"✓ Output shapes: {[out.shape for out in outputs]}")
        
        return True
        
    except Exception as e:
        print(f"Test failed: {e}")
        return False
        
    finally:
        rknn.release()

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 convert_yolov8_to_rknn.py <onnx_model_path> [output_path]")
        print("Example: python3 convert_yolov8_to_rknn.py models/yolov8n.onnx models/yolov8n.rknn")
        sys.exit(1)
    
    onnx_path = sys.argv[1]
    
    if len(sys.argv) >= 3:
        rknn_path = sys.argv[2]
    else:
        # Auto-generate output path
        base_name = os.path.splitext(onnx_path)[0]
        rknn_path = f"{base_name}.rknn"
    
    # Check input file
    if not os.path.exists(onnx_path):
        print(f"Error: ONNX model file not found: {onnx_path}")
        sys.exit(1)
    
    # Create output directory if needed
    output_dir = os.path.dirname(rknn_path)
    if output_dir and not os.path.exists(output_dir):
        os.makedirs(output_dir)
    
    print("=" * 60)
    print("YOLOv8 ONNX to RKNN Conversion")
    print("=" * 60)
    
    # Convert model
    success = convert_yolov8_to_rknn(onnx_path, rknn_path)
    
    if success:
        # Test converted model
        test_success = test_rknn_model(rknn_path)
        
        if test_success:
            print("\n" + "=" * 60)
            print("🎉 CONVERSION SUCCESSFUL!")
            print("=" * 60)
            print(f"✓ RKNN model: {rknn_path}")
            print(f"✓ Ready for RK3588 NPU acceleration")
            print("\nNext steps:")
            print("1. Copy the .rknn file to your models directory")
            print("2. Update your AI system to use RKNN backend")
            print("3. Enjoy NPU-accelerated inference!")
        else:
            print("\n⚠️  Conversion completed but model test failed")
            print("The RKNN file was created but may have issues")
    else:
        print("\n❌ Conversion failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
