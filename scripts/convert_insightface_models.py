#!/usr/bin/env python3
"""
InsightFace Model Conversion Script for TensorRT
This script downloads ONNX models and converts them to TensorRT-compatible format
"""

import os
import sys
import argparse
import logging
import subprocess
import tempfile
import shutil
from pathlib import Path

# Setup logging
logging.basicConfig(level=logging.INFO, format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

class InsightFaceModelConverter:
    def __init__(self, output_dir="models"):
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)
        self.temp_dir = None
        
    def setup_temp_dir(self):
        """Create temporary directory for downloads"""
        self.temp_dir = Path(tempfile.mkdtemp(prefix="insightface_convert_"))
        logger.info(f"Created temporary directory: {self.temp_dir}")
        
    def cleanup_temp_dir(self):
        """Clean up temporary directory"""
        if self.temp_dir and self.temp_dir.exists():
            shutil.rmtree(self.temp_dir)
            logger.info(f"Cleaned up temporary directory: {self.temp_dir}")
    
    def download_onnx_models(self):
        """Download ONNX models from InsightFace model zoo"""
        logger.info("Downloading InsightFace ONNX models...")
        
        # Model URLs from InsightFace model zoo
        models = {
            "det_10g.onnx": "https://github.com/deepinsight/insightface/releases/download/v0.7/det_10g.onnx",
            "genderage.onnx": "https://github.com/deepinsight/insightface/releases/download/v0.7/genderage.onnx",
            "w600k_r50.onnx": "https://github.com/deepinsight/insightface/releases/download/v0.7/w600k_r50.onnx"
        }
        
        downloaded_models = {}
        
        for model_name, url in models.items():
            model_path = self.temp_dir / model_name
            logger.info(f"Downloading {model_name}...")
            
            try:
                # Use wget to download
                cmd = ["wget", "-O", str(model_path), url]
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
                
                if result.returncode == 0 and model_path.exists():
                    logger.info(f"Successfully downloaded {model_name}")
                    downloaded_models[model_name] = model_path
                else:
                    logger.warning(f"Failed to download {model_name}: {result.stderr}")
                    
            except subprocess.TimeoutExpired:
                logger.error(f"Timeout downloading {model_name}")
            except Exception as e:
                logger.error(f"Error downloading {model_name}: {e}")
                
        return downloaded_models
    
    def convert_onnx_to_tensorrt(self, onnx_path, output_path, input_shape=None):
        """Convert ONNX model to TensorRT engine"""
        logger.info(f"Converting {onnx_path} to TensorRT engine...")

        # TensorRT conversion command - use full path to trtexec
        cmd = [
            "/usr/src/tensorrt/bin/trtexec",
            f"--onnx={onnx_path}",
            f"--saveEngine={output_path}",
            "--fp16",  # Use FP16 precision
            "--workspace=1024",  # 1GB workspace
            "--verbose"
        ]

        # Add input shape if specified
        if input_shape:
            cmd.append(f"--shapes={input_shape}")

        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=600)

            if result.returncode == 0 and Path(output_path).exists():
                logger.info(f"Successfully converted to {output_path}")
                return True
            else:
                logger.error(f"TensorRT conversion failed: {result.stderr}")
                return False

        except subprocess.TimeoutExpired:
            logger.error(f"Timeout converting {onnx_path}")
            return False
        except FileNotFoundError:
            logger.error("trtexec not found at /usr/src/tensorrt/bin/trtexec")
            return False
        except Exception as e:
            logger.error(f"Error converting {onnx_path}: {e}")
            return False
    
    def create_model_pack(self, model_files, pack_name="Pikachu_x86_64_tensorrt.pack"):
        """Create InsightFace model pack from converted models"""
        logger.info(f"Creating model pack: {pack_name}")
        
        pack_path = self.output_dir / pack_name
        
        # Create a simple tar archive for now
        # In a real implementation, this would follow InsightFace pack format
        try:
            import tarfile
            
            with tarfile.open(pack_path, 'w') as tar:
                # Add model files to the pack
                for model_name, model_path in model_files.items():
                    if model_path.exists():
                        tar.add(model_path, arcname=model_name)
                        logger.info(f"Added {model_name} to pack")
                
                # Add metadata file
                metadata = self.create_metadata()
                metadata_path = self.temp_dir / "metadata.json"
                with open(metadata_path, 'w') as f:
                    import json
                    json.dump(metadata, f, indent=2)
                tar.add(metadata_path, arcname="metadata.json")
                
            logger.info(f"Model pack created: {pack_path}")
            return pack_path
            
        except Exception as e:
            logger.error(f"Error creating model pack: {e}")
            return None
    
    def create_metadata(self):
        """Create metadata for the model pack"""
        return {
            "name": "TensorRT x86_64 Pack",
            "version": "1.0.0",
            "platform": "x86_64",
            "backend": "TensorRT",
            "precision": "FP16",
            "models": {
                "detection": "det_10g.engine",
                "age_gender": "genderage.engine", 
                "recognition": "w600k_r50.engine"
            },
            "created_by": "AISecurityVision Model Converter",
            "compatible_with": ["TensorRT-10.x", "CUDA-12.x"]
        }
    
    def convert_models(self):
        """Main conversion workflow"""
        try:
            self.setup_temp_dir()
            
            # Step 1: Download ONNX models
            logger.info("Step 1: Downloading ONNX models...")
            onnx_models = self.download_onnx_models()
            
            if not onnx_models:
                logger.error("No models downloaded successfully")
                return False
            
            # Step 2: Convert to TensorRT
            logger.info("Step 2: Converting to TensorRT engines...")
            tensorrt_models = {}
            
            for model_name, onnx_path in onnx_models.items():
                engine_name = model_name.replace('.onnx', '.engine')
                engine_path = self.temp_dir / engine_name
                
                if self.convert_onnx_to_tensorrt(onnx_path, engine_path):
                    tensorrt_models[engine_name] = engine_path
            
            if not tensorrt_models:
                logger.error("No models converted successfully")
                return False
            
            # Step 3: Create model pack
            logger.info("Step 3: Creating model pack...")
            pack_path = self.create_model_pack(tensorrt_models)
            
            if pack_path:
                logger.info(f"Model conversion completed successfully!")
                logger.info(f"Model pack created at: {pack_path}")
                return True
            else:
                logger.error("Failed to create model pack")
                return False
                
        except Exception as e:
            logger.error(f"Conversion failed: {e}")
            return False
        finally:
            self.cleanup_temp_dir()

def main():
    parser = argparse.ArgumentParser(description="Convert InsightFace models to TensorRT")
    parser.add_argument("--output-dir", default="models", 
                       help="Output directory for converted models")
    parser.add_argument("--pack-name", default="Pikachu_x86_64_tensorrt.pack",
                       help="Name of the output model pack")
    parser.add_argument("--verbose", action="store_true",
                       help="Enable verbose logging")
    
    args = parser.parse_args()
    
    if args.verbose:
        logging.getLogger().setLevel(logging.DEBUG)
    
    converter = InsightFaceModelConverter(args.output_dir)
    success = converter.convert_models()
    
    if success:
        logger.info("Model conversion completed successfully!")
        sys.exit(0)
    else:
        logger.error("Model conversion failed!")
        sys.exit(1)

if __name__ == "__main__":
    main()
