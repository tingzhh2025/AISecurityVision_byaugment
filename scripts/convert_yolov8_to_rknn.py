#!/usr/bin/env python3
"""
YOLOv8 to RKNN Model Conversion Script

This script converts YOLOv8 ONNX models to RKNN format for use with Rockchip NPUs.
Requires RKNN-Toolkit2 to be installed.

Usage:
    python convert_yolov8_to_rknn.py --input yolov8n.onnx --output yolov8n.rknn --platform rk3588

Requirements:
    - rknn-toolkit2
    - numpy
    - opencv-python
"""

import argparse
import sys
import os
import numpy as np
import cv2

try:
    from rknn.api import RKNN
except ImportError:
    print("Error: RKNN-Toolkit2 not found. Please install it first.")
    print("Installation guide: https://github.com/airockchip/rknn-toolkit2")
    sys.exit(1)


def parse_args():
    parser = argparse.ArgumentParser(description='Convert YOLOv8 ONNX to RKNN')
    parser.add_argument('--input', '-i', required=True, help='Input ONNX model path')
    parser.add_argument('--output', '-o', required=True, help='Output RKNN model path')
    parser.add_argument('--platform', '-p', default='rk3588', 
                       choices=['rk3562', 'rk3566', 'rk3568', 'rk3576', 'rk3588'],
                       help='Target platform')
    parser.add_argument('--quantize', '-q', default='INT8', 
                       choices=['FP16', 'INT8'],
                       help='Quantization type')
    parser.add_argument('--dataset', '-d', help='Dataset for quantization (optional)')
    parser.add_argument('--input-size', default='640,640', help='Input size (width,height)')
    return parser.parse_args()


def create_dataset(dataset_path, input_size, num_samples=100):
    """Create dataset for quantization"""
    if not dataset_path or not os.path.exists(dataset_path):
        print("Creating synthetic dataset for quantization...")
        # Create synthetic data
        width, height = input_size
        dataset = []
        for i in range(num_samples):
            # Generate random image data
            img = np.random.randint(0, 256, (height, width, 3), dtype=np.uint8)
            dataset.append(img)
        return dataset
    
    print(f"Loading dataset from {dataset_path}...")
    dataset = []
    image_files = []
    
    # Collect image files
    for ext in ['*.jpg', '*.jpeg', '*.png', '*.bmp']:
        import glob
        image_files.extend(glob.glob(os.path.join(dataset_path, ext)))
        image_files.extend(glob.glob(os.path.join(dataset_path, ext.upper())))
    
    if not image_files:
        print("No images found in dataset path, using synthetic data")
        return create_dataset(None, input_size, num_samples)
    
    # Load and preprocess images
    width, height = input_size
    for i, img_path in enumerate(image_files[:num_samples]):
        img = cv2.imread(img_path)
        if img is not None:
            img = cv2.resize(img, (width, height))
            dataset.append(img)
        
        if len(dataset) >= num_samples:
            break
    
    print(f"Loaded {len(dataset)} images for quantization")
    return dataset


def convert_model(args):
    """Convert ONNX model to RKNN"""
    
    # Parse input size
    try:
        width, height = map(int, args.input_size.split(','))
        input_size = (width, height)
    except:
        print("Error: Invalid input size format. Use 'width,height' (e.g., '640,640')")
        return False
    
    # Check input file
    if not os.path.exists(args.input):
        print(f"Error: Input file {args.input} not found")
        return False
    
    # Create RKNN object
    rknn = RKNN(verbose=True)
    
    print(f"Converting {args.input} to {args.output}")
    print(f"Platform: {args.platform}")
    print(f"Quantization: {args.quantize}")
    print(f"Input size: {input_size}")
    
    try:
        # Configuration
        print("Configuring RKNN...")
        ret = rknn.config(
            mean_values=[[0, 0, 0]],
            std_values=[[255, 255, 255]],
            target_platform=args.platform,
            quantized_dtype=args.quantize.lower(),
            optimization_level=3
        )
        if ret != 0:
            print("Config failed!")
            return False
        
        # Load model
        print("Loading ONNX model...")
        ret = rknn.load_onnx(model=args.input)
        if ret != 0:
            print("Load model failed!")
            return False
        
        # Build model
        print("Building RKNN model...")
        if args.quantize == 'INT8':
            # Create dataset for quantization
            dataset = create_dataset(args.dataset, input_size)
            ret = rknn.build(do_quantization=True, dataset=dataset)
        else:
            ret = rknn.build(do_quantization=False)
        
        if ret != 0:
            print("Build model failed!")
            return False
        
        # Export model
        print(f"Exporting to {args.output}...")
        ret = rknn.export_rknn(args.output)
        if ret != 0:
            print("Export failed!")
            return False
        
        print("Conversion completed successfully!")
        
        # Optional: Test the model
        print("Testing the converted model...")
        ret = rknn.init_runtime()
        if ret == 0:
            # Create test input
            test_input = np.random.randint(0, 256, (1, height, width, 3), dtype=np.uint8)
            outputs = rknn.inference(inputs=[test_input])
            if outputs:
                print(f"Model test successful! Output shape: {[out.shape for out in outputs]}")
            else:
                print("Model test failed!")
        
        return True
        
    except Exception as e:
        print(f"Error during conversion: {e}")
        return False
    
    finally:
        rknn.release()


def main():
    args = parse_args()
    
    print("YOLOv8 to RKNN Conversion Tool")
    print("=" * 40)
    
    success = convert_model(args)
    
    if success:
        print("\nConversion completed successfully!")
        print(f"RKNN model saved to: {args.output}")
        print("\nTo use the model in your application:")
        print(f"  detector.initialize(\"{args.output}\", InferenceBackend::RKNN);")
    else:
        print("\nConversion failed!")
        sys.exit(1)


if __name__ == "__main__":
    main()
