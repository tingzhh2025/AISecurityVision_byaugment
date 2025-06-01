#!/usr/bin/env python3
"""
Age Gender Model Conversion Script for RKNN
============================================

This script converts age and gender recognition models to RKNN format
for deployment on RK3588 NPU.

Requirements:
- rknn-toolkit2 (for RK3588)
- numpy
- opencv-python

Usage:
    python convert_age_gender_to_rknn.py --input model.onnx --output age_gender.rknn

Model Requirements:
- Input: 224x224x3 RGB image
- Output 1: Gender classification (2 classes: female, male)
- Output 2: Age group classification (4 classes: child, young, middle, senior)
"""

import argparse
import os
import sys
import numpy as np
import cv2

try:
    from rknn.api import RKNN
except ImportError:
    print("Error: rknn-toolkit2 not found. Please install it first:")
    print("pip install rknn-toolkit2")
    sys.exit(1)

def create_calibration_dataset(output_path="calibration_dataset.txt", num_images=100):
    """Create calibration dataset for quantization"""
    print(f"Creating calibration dataset with {num_images} synthetic images...")
    
    calibration_images = []
    
    for i in range(num_images):
        # Create synthetic face-like images for calibration
        img = np.random.randint(0, 256, (224, 224, 3), dtype=np.uint8)
        
        # Add some face-like features
        center = (112, 112)
        
        # Face oval
        cv2.ellipse(img, center, (80, 100), 0, 0, 360, (200, 180, 160), -1)
        
        # Eyes
        cv2.circle(img, (90, 90), 8, (50, 50, 50), -1)
        cv2.circle(img, (134, 90), 8, (50, 50, 50), -1)
        
        # Nose
        cv2.circle(img, (112, 110), 3, (150, 120, 100), -1)
        
        # Mouth
        cv2.ellipse(img, (112, 130), (15, 8), 0, 0, 180, (100, 50, 50), -1)
        
        # Add some noise for diversity
        noise = np.random.normal(0, 10, img.shape).astype(np.int16)
        img = np.clip(img.astype(np.int16) + noise, 0, 255).astype(np.uint8)
        
        # Save calibration image
        img_path = f"calibration_img_{i:03d}.jpg"
        cv2.imwrite(img_path, img)
        calibration_images.append(img_path)
    
    # Write calibration dataset file
    with open(output_path, 'w') as f:
        for img_path in calibration_images:
            f.write(f"{img_path}\n")
    
    print(f"Calibration dataset created: {output_path}")
    return output_path

def convert_model(input_path, output_path, calibration_dataset=None):
    """Convert ONNX model to RKNN format"""
    
    if not os.path.exists(input_path):
        print(f"Error: Input model file not found: {input_path}")
        return False
    
    print(f"Converting {input_path} to RKNN format...")
    
    # Initialize RKNN
    rknn = RKNN(verbose=True)
    
    # Configure for RK3588
    print("Configuring RKNN for RK3588...")
    ret = rknn.config(
        mean_values=[[123.675, 116.28, 103.53]],  # ImageNet mean
        std_values=[[58.395, 57.12, 57.375]],     # ImageNet std
        target_platform='rk3588',
        quantized_algorithm='normal',
        quantized_method='channel',
        quantized_hybrid_level=3,  # Mixed precision
        optimization_level=3
    )
    
    if ret != 0:
        print("Error: RKNN config failed")
        return False
    
    # Load ONNX model
    print("Loading ONNX model...")
    ret = rknn.load_onnx(model=input_path)
    if ret != 0:
        print("Error: Failed to load ONNX model")
        return False
    
    # Build model
    print("Building RKNN model...")
    
    # Create calibration dataset if not provided
    if calibration_dataset is None:
        calibration_dataset = create_calibration_dataset()
    
    ret = rknn.build(
        do_quantization=True,
        dataset=calibration_dataset,
        rknn_batch_size=1
    )
    
    if ret != 0:
        print("Error: RKNN build failed")
        return False
    
    # Export RKNN model
    print(f"Exporting to {output_path}...")
    ret = rknn.export_rknn(output_path)
    if ret != 0:
        print("Error: Failed to export RKNN model")
        return False
    
    # Test inference (optional)
    print("Testing inference...")
    ret = rknn.init_runtime()
    if ret == 0:
        # Create test input
        test_input = np.random.randint(0, 256, (1, 224, 224, 3), dtype=np.uint8)
        
        # Run inference
        outputs = rknn.inference(inputs=[test_input])
        if outputs is not None:
            print(f"Inference test successful!")
            print(f"Output shapes: {[output.shape for output in outputs]}")
            
            if len(outputs) >= 2:
                print(f"Gender output shape: {outputs[0].shape}")
                print(f"Age output shape: {outputs[1].shape}")
        else:
            print("Warning: Inference test failed")
    
    # Cleanup
    rknn.release()
    
    # Clean up calibration images
    if calibration_dataset == "calibration_dataset.txt":
        print("Cleaning up calibration images...")
        for i in range(100):
            img_path = f"calibration_img_{i:03d}.jpg"
            if os.path.exists(img_path):
                os.remove(img_path)
        if os.path.exists(calibration_dataset):
            os.remove(calibration_dataset)
    
    print(f"Model conversion completed successfully: {output_path}")
    return True

def main():
    parser = argparse.ArgumentParser(description="Convert Age Gender model to RKNN format")
    parser.add_argument("--input", "-i", required=True, help="Input ONNX model path")
    parser.add_argument("--output", "-o", required=True, help="Output RKNN model path")
    parser.add_argument("--calibration", "-c", help="Calibration dataset file (optional)")
    parser.add_argument("--platform", "-p", default="rk3588", help="Target platform (default: rk3588)")
    
    args = parser.parse_args()
    
    print("=" * 60)
    print("Age Gender Model Conversion to RKNN")
    print("=" * 60)
    print(f"Input model: {args.input}")
    print(f"Output model: {args.output}")
    print(f"Target platform: {args.platform}")
    print(f"Calibration dataset: {args.calibration or 'Auto-generated'}")
    print()
    
    # Validate input
    if not args.input.endswith('.onnx'):
        print("Warning: Input file should be an ONNX model (.onnx)")
    
    if not args.output.endswith('.rknn'):
        print("Warning: Output file should have .rknn extension")
    
    # Convert model
    success = convert_model(args.input, args.output, args.calibration)
    
    if success:
        print("\n" + "=" * 60)
        print("Conversion completed successfully!")
        print("=" * 60)
        print(f"RKNN model saved to: {args.output}")
        print("\nNext steps:")
        print("1. Copy the .rknn file to your models/ directory")
        print("2. Update your application to use the new model")
        print("3. Test with the person_stats_test program")
        print("\nExample usage in C++:")
        print("  AgeGenderAnalyzer analyzer;")
        print(f"  analyzer.initialize(\"{args.output}\");")
        return 0
    else:
        print("\n" + "=" * 60)
        print("Conversion failed!")
        print("=" * 60)
        print("Please check the error messages above and try again.")
        return 1

if __name__ == "__main__":
    main()
