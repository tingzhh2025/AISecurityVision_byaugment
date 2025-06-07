#!/usr/bin/env python3
"""
Package rebuilt TensorRT engines into InsightFace model pack
"""

import os
import shutil
import tarfile
from pathlib import Path

def create_insightface_pack():
    """Create InsightFace model pack with rebuilt TensorRT engines"""
    
    print("========================================")
    print("Packaging Rebuilt TensorRT Engines")
    print("========================================")
    
    engines_dir = Path("tensorrt_engines_rebuilt")
    pack_dir = Path("pack_content_rebuilt")
    pack_dir.mkdir(exist_ok=True)
    
    # Expected engine files
    expected_engines = [
        "_00_scrfd_2_5g_bnkps_shape640x640_fp16",
        "_00_scrfd_2_5g_bnkps_shape320x320_fp16", 
        "_00_scrfd_2_5g_bnkps_shape160x160_fp16",
        "_01_hyplmkv2_0.25_112x_fp16",
        "_03_r18_Glint360K_fixed_fp16",
        "_08_fairface_model_fp16",
        "_09_blink_crop_fp16"
    ]
    
    # Copy engine files
    copied_engines = []
    for engine_name in expected_engines:
        engine_path = engines_dir / engine_name
        if engine_path.exists():
            dest_path = pack_dir / engine_name
            shutil.copy2(engine_path, dest_path)
            size_mb = engine_path.stat().st_size / 1024 / 1024
            print(f"✓ Copied engine: {engine_name} ({size_mb:.1f} MB)")
            copied_engines.append(engine_name)
        else:
            print(f"⚠ Engine not found: {engine_name}")
    
    if not copied_engines:
        print("✗ No engines found to package")
        return None
    
    # Create configuration file
    config_content = """tag: Pikachu_TRT10.11_Rebuilt
version: 3.1
major: t3.1-tensorrt10.11-rebuilt
release: 2025-06-07

# Rebuilt TensorRT 10.11 compatible engines
similarity_converter:
  threshold: 0.32
  middle_score: 0.6
  steepness: 10.0
  output_min: 0.02
  output_max: 1.0

face_detect_640:
  name: _00_scrfd_2_5g_bnkps_shape640x640_fp16
  fullname: _00_scrfd_2_5g_bnkps_shape640x640_fp16.engine
  version: 0.1
  model_type: TensorRT
  infer_engine: TensorRT
  input_shape: [1, 3, 640, 640]
  output_shape: [1, 12800, 15]

face_detect_320:
  name: _00_scrfd_2_5g_bnkps_shape320x320_fp16
  fullname: _00_scrfd_2_5g_bnkps_shape320x320_fp16.engine
  version: 0.1
  model_type: TensorRT
  infer_engine: TensorRT
  input_shape: [1, 3, 320, 320]
  output_shape: [1, 3200, 15]

face_detect_160:
  name: _00_scrfd_2_5g_bnkps_shape160x160_fp16
  fullname: _00_scrfd_2_5g_bnkps_shape160x160_fp16.engine
  version: 0.1
  model_type: TensorRT
  infer_engine: TensorRT
  input_shape: [1, 3, 160, 160]
  output_shape: [1, 800, 15]

landmark:
  name: _01_hyplmkv2_0.25_112x_fp16
  fullname: _01_hyplmkv2_0.25_112x_fp16.engine
  version: 0.1
  model_type: TensorRT
  infer_engine: TensorRT
  input_shape: [1, 3, 112, 112]
  output_shape: [1, 106, 2]

recognition:
  name: _03_r18_Glint360K_fixed_fp16
  fullname: _03_r18_Glint360K_fixed_fp16.engine
  version: 0.1
  model_type: TensorRT
  infer_engine: TensorRT
  input_shape: [1, 3, 112, 112]
  output_shape: [1, 512]

age_gender:
  name: _08_fairface_model_fp16
  fullname: _08_fairface_model_fp16.engine
  version: 0.1
  model_type: TensorRT
  infer_engine: TensorRT
  input_shape: [1, 3, 224, 224]
  output_shape: [1, 18]

blink:
  name: _09_blink_crop_fp16
  fullname: _09_blink_crop_fp16.engine
  version: 0.1
  model_type: TensorRT
  infer_engine: TensorRT
  input_shape: [1, 3, 64, 64]
  output_shape: [1, 2]
"""
    
    config_path = pack_dir / "__inspire__"
    with open(config_path, 'w') as f:
        f.write(config_content)
    
    print("✓ Created configuration file")
    
    # Create the pack archive
    pack_name = "Pikachu_x86_64_rebuilt_tensorrt10.11.pack"
    pack_path = Path("models") / pack_name
    
    # Ensure models directory exists
    Path("models").mkdir(exist_ok=True)
    
    print(f"Creating pack archive: {pack_path}")
    
    with tarfile.open(pack_path, 'w') as tar:
        for item in pack_dir.iterdir():
            tar.add(item, arcname=item.name)
    
    total_size = pack_path.stat().st_size / 1024 / 1024
    print(f"✓ Created InsightFace pack: {pack_path}")
    print(f"Pack size: {total_size:.1f} MB")
    
    # Cleanup
    shutil.rmtree(pack_dir, ignore_errors=True)
    
    return pack_path

def main():
    try:
        pack_path = create_insightface_pack()
        
        if pack_path:
            print("\n========================================")
            print("Packaging completed successfully!")
            print("========================================")
            print(f"✓ Created pack: {pack_path}")
            print("\nNext steps:")
            print("1. Backup current model:")
            print("   mv models/Pikachu_x86_64.pack models/Pikachu_x86_64.pack.backup2")
            print("2. Use new rebuilt pack:")
            print(f"   mv {pack_path} models/Pikachu_x86_64.pack")
            print("3. Test the system:")
            print("   ./build/AISecurityVision --config config/config_tensorrt.json")
        else:
            print("✗ Packaging failed")
            return 1
            
    except Exception as e:
        print(f"Error: {e}")
        return 1
    
    return 0

if __name__ == "__main__":
    exit(main())
