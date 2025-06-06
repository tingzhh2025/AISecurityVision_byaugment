#!/usr/bin/env python3
"""
Convert YOLOv8 ONNX model to TensorRT engine
This script converts YOLOv8 ONNX models to optimized TensorRT engines
"""

import argparse
import os
import sys
import logging
import numpy as np

try:
    import tensorrt as trt
except ImportError:
    print("Error: TensorRT is not installed. Please install it first:")
    print("  pip install tensorrt")
    sys.exit(1)

# Setup logger
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger(__name__)

class YOLOv8EngineBuilder:
    """
    Build TensorRT engine for YOLOv8 models
    """
    
    def __init__(self, onnx_path, engine_path, precision='FP16', max_batch_size=1, 
                 workspace_size=1<<30, verbose=False):
        self.onnx_path = onnx_path
        self.engine_path = engine_path
        self.precision = precision
        self.max_batch_size = max_batch_size
        self.workspace_size = workspace_size
        self.verbose = verbose
        
        # TensorRT logger
        self.trt_logger = trt.Logger(trt.Logger.VERBOSE if verbose else trt.Logger.INFO)
        
    def build_engine(self):
        """Build TensorRT engine from ONNX model"""
        
        # Create builder
        builder = trt.Builder(self.trt_logger)
        network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
        config = builder.create_builder_config()
        
        # Parse ONNX
        logger.info(f"Parsing ONNX file: {self.onnx_path}")
        parser = trt.OnnxParser(network, self.trt_logger)
        
        with open(self.onnx_path, 'rb') as f:
            if not parser.parse(f.read()):
                logger.error("Failed to parse ONNX file")
                for error in range(parser.num_errors):
                    logger.error(parser.get_error(error))
                return False
                
        logger.info("Successfully parsed ONNX file")
        
        # Configure builder
        config.max_workspace_size = self.workspace_size
        
        # Set precision
        if self.precision == 'FP16':
            if builder.platform_has_fast_fp16:
                config.set_flag(trt.BuilderFlag.FP16)
                logger.info("Using FP16 precision")
            else:
                logger.warning("FP16 not supported on this platform, using FP32")
        elif self.precision == 'INT8':
            if builder.platform_has_fast_int8:
                config.set_flag(trt.BuilderFlag.INT8)
                logger.info("Using INT8 precision (requires calibration)")
                # Note: INT8 requires calibration data
            else:
                logger.warning("INT8 not supported on this platform, using FP32")
        else:
            logger.info("Using FP32 precision")
            
        # Build engine
        logger.info("Building TensorRT engine...")
        engine = builder.build_engine(network, config)
        
        if engine is None:
            logger.error("Failed to build engine")
            return False
            
        # Save engine
        logger.info(f"Saving engine to: {self.engine_path}")
        with open(self.engine_path, 'wb') as f:
            f.write(engine.serialize())
            
        # Print engine info
        logger.info(f"Engine built successfully!")
        logger.info(f"  - Max batch size: {engine.max_batch_size}")
        logger.info(f"  - Number of layers: {network.num_layers}")
        logger.info(f"  - Number of inputs: {network.num_inputs}")
        logger.info(f"  - Number of outputs: {network.num_outputs}")
        
        # Print input/output info
        for i in range(network.num_inputs):
            input_tensor = network.get_input(i)
            logger.info(f"  - Input {i}: {input_tensor.name} {input_tensor.shape}")
            
        for i in range(network.num_outputs):
            output_tensor = network.get_output(i)
            logger.info(f"  - Output {i}: {output_tensor.name} {output_tensor.shape}")
            
        return True
        
    def verify_engine(self):
        """Verify the built engine"""
        
        if not os.path.exists(self.engine_path):
            logger.error(f"Engine file not found: {self.engine_path}")
            return False
            
        runtime = trt.Runtime(self.trt_logger)
        
        try:
            with open(self.engine_path, 'rb') as f:
                engine = runtime.deserialize_cuda_engine(f.read())
                
            if engine is None:
                logger.error("Failed to deserialize engine")
                return False
                
            logger.info("Engine verification successful!")
            logger.info(f"  - Number of bindings: {engine.num_bindings}")
            
            for i in range(engine.num_bindings):
                name = engine.get_binding_name(i)
                dtype = engine.get_binding_dtype(i)
                shape = engine.get_binding_shape(i)
                is_input = engine.binding_is_input(i)
                
                logger.info(f"  - Binding {i}: {name}")
                logger.info(f"      Type: {'Input' if is_input else 'Output'}")
                logger.info(f"      Dtype: {dtype}")
                logger.info(f"      Shape: {shape}")
                
            return True
            
        except Exception as e:
            logger.error(f"Engine verification failed: {e}")
            return False
            
def main():
    parser = argparse.ArgumentParser(description='Convert YOLOv8 ONNX to TensorRT')
    parser.add_argument('onnx_model', help='Path to ONNX model file')
    parser.add_argument('--output', '-o', help='Output engine path (default: model.engine)')
    parser.add_argument('--precision', '-p', choices=['FP32', 'FP16', 'INT8'], 
                        default='FP16', help='Inference precision (default: FP16)')
    parser.add_argument('--batch-size', '-b', type=int, default=1,
                        help='Max batch size (default: 1)')
    parser.add_argument('--workspace', '-w', type=int, default=1024,
                        help='Workspace size in MB (default: 1024)')
    parser.add_argument('--verify', action='store_true',
                        help='Verify engine after building')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Enable verbose output')
    
    args = parser.parse_args()
    
    # Check input file
    if not os.path.exists(args.onnx_model):
        logger.error(f"ONNX model not found: {args.onnx_model}")
        return 1
        
    # Set output path
    if args.output:
        engine_path = args.output
    else:
        base_name = os.path.splitext(os.path.basename(args.onnx_model))[0]
        engine_path = f"{base_name}_{args.precision.lower()}.engine"
        
    # Build engine
    builder = YOLOv8EngineBuilder(
        onnx_path=args.onnx_model,
        engine_path=engine_path,
        precision=args.precision,
        max_batch_size=args.batch_size,
        workspace_size=args.workspace << 20,  # Convert MB to bytes
        verbose=args.verbose
    )
    
    if not builder.build_engine():
        logger.error("Engine build failed")
        return 1
        
    # Verify if requested
    if args.verify:
        if not builder.verify_engine():
            return 1
            
    logger.info(f"Conversion complete! Engine saved to: {engine_path}")
    return 0
    
if __name__ == "__main__":
    sys.exit(main())
