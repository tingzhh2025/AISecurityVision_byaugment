#!/usr/bin/env python3
"""
Simple YOLOv8 ONNX to RKNN Conversion Script
"""

import os
import sys

def main():
    print("YOLOv8 ONNX to RKNN Conversion")
    print("=" * 40)
    
    try:
        from rknnlite.api import RKNNLite
        print("✓ RKNN Lite imported successfully")
    except ImportError as e:
        print(f"✗ Failed to import RKNN Lite: {e}")
        return 1
    
    onnx_path = "build/models/yolov8n.onnx"
    rknn_path = "build/models/yolov8n.rknn"
    
    if not os.path.exists(onnx_path):
        print(f"✗ ONNX model not found: {onnx_path}")
        return 1
    
    print(f"Input: {onnx_path}")
    print(f"Output: {rknn_path}")
    
    # Initialize RKNN
    rknn = RKNNLite()
    
    try:
        print("\n1. Loading ONNX model...")
        ret = rknn.load_onnx(model=onnx_path)
        if ret != 0:
            print(f"✗ Load ONNX failed: {ret}")
            return 1
        print("✓ ONNX model loaded")
        
        print("\n2. Building RKNN model...")
        ret = rknn.build(do_quantization=True)
        if ret != 0:
            print(f"✗ Build failed: {ret}")
            return 1
        print("✓ RKNN model built")
        
        print("\n3. Exporting RKNN model...")
        ret = rknn.export_rknn(rknn_path)
        if ret != 0:
            print(f"✗ Export failed: {ret}")
            return 1
        
        size_mb = os.path.getsize(rknn_path) / (1024 * 1024)
        print(f"✓ RKNN model exported: {size_mb:.2f} MB")
        
        print("\n🎉 Conversion successful!")
        return 0
        
    except Exception as e:
        print(f"✗ Conversion error: {e}")
        return 1
        
    finally:
        rknn.release()

if __name__ == "__main__":
    sys.exit(main())
