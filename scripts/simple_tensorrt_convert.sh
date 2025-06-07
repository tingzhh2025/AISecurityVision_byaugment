#!/bin/bash

# Simple TensorRT Model Conversion Script
# This script converts ONNX models to TensorRT engines for InsightFace

set -e

# Configuration
PROJECT_ROOT="/home/rogers/source/custom/AISecurityVision_byaugment"
MODELS_DIR="$PROJECT_ROOT/models"
TEMP_DIR="/tmp/insightface_convert"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}TensorRT InsightFace Model Converter${NC}"
echo -e "${GREEN}========================================${NC}"

# Create temporary directory
mkdir -p "$TEMP_DIR"
cd "$TEMP_DIR"

# Function to check if command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Check dependencies
echo -e "${YELLOW}Checking dependencies...${NC}"

# Check for trtexec in common locations
TRTEXEC_PATH=""
if command_exists trtexec; then
    TRTEXEC_PATH="trtexec"
elif [ -f "/usr/src/tensorrt/bin/trtexec" ]; then
    TRTEXEC_PATH="/usr/src/tensorrt/bin/trtexec"
else
    echo -e "${RED}Error: trtexec not found. Please install TensorRT.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ Found trtexec at: $TRTEXEC_PATH${NC}"

if ! command_exists wget; then
    echo -e "${RED}Error: wget not found. Please install wget.${NC}"
    exit 1
fi

echo -e "${GREEN}✓ All dependencies found${NC}"

# Download ONNX models
echo -e "${YELLOW}Downloading ONNX models...${NC}"

# Face detection model (lightweight)
echo -e "${BLUE}Downloading face detection model...${NC}"
wget -O det_10g.onnx "https://github.com/deepinsight/insightface/releases/download/v0.7/det_10g.onnx" || {
    echo -e "${YELLOW}Warning: Failed to download det_10g.onnx, trying alternative...${NC}"
    wget -O scrfd_10g_bnkps.onnx "https://github.com/deepinsight/insightface/releases/download/v0.7/scrfd_10g_bnkps.onnx" || {
        echo -e "${RED}Error: Failed to download face detection model${NC}"
        exit 1
    }
}

# Age/Gender model
echo -e "${BLUE}Downloading age/gender model...${NC}"
wget -O genderage.onnx "https://github.com/deepinsight/insightface/releases/download/v0.7/genderage.onnx" || {
    echo -e "${YELLOW}Warning: Failed to download genderage.onnx${NC}"
}

# Face recognition model (optional)
echo -e "${BLUE}Downloading face recognition model...${NC}"
wget -O w600k_r50.onnx "https://github.com/deepinsight/insightface/releases/download/v0.7/w600k_r50.onnx" || {
    echo -e "${YELLOW}Warning: Failed to download w600k_r50.onnx${NC}"
}

echo -e "${GREEN}✓ Model download completed${NC}"

# Convert ONNX to TensorRT
echo -e "${YELLOW}Converting models to TensorRT...${NC}"

convert_model() {
    local onnx_file="$1"
    local engine_file="${onnx_file%.onnx}.engine"
    
    if [ -f "$onnx_file" ]; then
        echo -e "${BLUE}Converting $onnx_file to $engine_file...${NC}"
        
        $TRTEXEC_PATH \
            --onnx="$onnx_file" \
            --saveEngine="$engine_file" \
            --fp16 \
            --workspace=1024 \
            --verbose \
            --minShapes=input:1x3x640x640 \
            --optShapes=input:1x3x640x640 \
            --maxShapes=input:1x3x640x640 || {
            echo -e "${YELLOW}Warning: Failed to convert $onnx_file${NC}"
            return 1
        }
        
        if [ -f "$engine_file" ]; then
            echo -e "${GREEN}✓ Successfully converted $onnx_file${NC}"
            return 0
        else
            echo -e "${RED}✗ Failed to create $engine_file${NC}"
            return 1
        fi
    else
        echo -e "${YELLOW}Warning: $onnx_file not found, skipping...${NC}"
        return 1
    fi
}

# Convert each model
convert_model "det_10g.onnx" || convert_model "scrfd_10g_bnkps.onnx"
convert_model "genderage.onnx"
convert_model "w600k_r50.onnx"

# Create a simple model pack
echo -e "${YELLOW}Creating model pack...${NC}"

PACK_NAME="Pikachu_x86_64_tensorrt.pack"
PACK_PATH="$MODELS_DIR/$PACK_NAME"

# Create pack directory structure
PACK_TEMP="$TEMP_DIR/pack_temp"
mkdir -p "$PACK_TEMP/__inspire__"

# Copy converted engines
for engine in *.engine; do
    if [ -f "$engine" ]; then
        cp "$engine" "$PACK_TEMP/__inspire__/"
        echo -e "${GREEN}Added $engine to pack${NC}"
    fi
done

# Create metadata
cat > "$PACK_TEMP/__inspire__/metadata.json" << EOF
{
    "name": "TensorRT x86_64 Pack",
    "version": "1.0.0",
    "platform": "x86_64",
    "backend": "TensorRT",
    "precision": "FP16",
    "created_by": "AISecurityVision Model Converter",
    "compatible_with": ["TensorRT-10.x", "CUDA-12.x"],
    "models": {
        "detection": "det_10g.engine",
        "age_gender": "genderage.engine",
        "recognition": "w600k_r50.engine"
    }
}
EOF

# Create the pack archive
cd "$PACK_TEMP"
tar -czf "$PACK_PATH" .

if [ -f "$PACK_PATH" ]; then
    echo -e "${GREEN}✓ Model pack created: $PACK_PATH${NC}"
    echo -e "${GREEN}Pack size: $(du -h "$PACK_PATH" | cut -f1)${NC}"
else
    echo -e "${RED}✗ Failed to create model pack${NC}"
    exit 1
fi

# Cleanup
cd "$PROJECT_ROOT"
rm -rf "$TEMP_DIR"

echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Model conversion completed successfully!${NC}"
echo -e "${GREEN}========================================${NC}"
echo -e "${GREEN}Model pack: $PACK_PATH${NC}"
echo ""
echo -e "${YELLOW}Next steps:${NC}"
echo -e "1. Backup current model: ${BLUE}mv models/Pikachu_x86_64.pack models/Pikachu_x86_64.pack.backup${NC}"
echo -e "2. Use new model pack: ${BLUE}mv $PACK_PATH models/Pikachu_x86_64.pack${NC}"
echo -e "3. Test the new models: ${BLUE}./build/AISecurityVision --config config/config_tensorrt.json${NC}"
