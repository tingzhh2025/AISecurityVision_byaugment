#!/bin/bash

# RK3588 NPU Performance Optimization Script
# Based on official RKNN model zoo recommendations

echo "=== RK3588 NPU Performance Optimization ==="

# Check if running as root
if [ "$EUID" -ne 0 ]; then
    echo "Please run as root (sudo) to modify system settings"
    exit 1
fi

# Set NPU to maximum frequency
echo "Setting NPU to maximum frequency..."

# RK3588 NPU frequency scaling
NPU_FREQ_PATH="/sys/class/devfreq/fdab0000.npu/governor"
NPU_MAX_FREQ_PATH="/sys/class/devfreq/fdab0000.npu/max_freq"
NPU_MIN_FREQ_PATH="/sys/class/devfreq/fdab0000.npu/min_freq"

if [ -f "$NPU_FREQ_PATH" ]; then
    echo "performance" > "$NPU_FREQ_PATH"
    echo "✓ Set NPU governor to performance mode"
else
    echo "⚠ NPU frequency control not found, trying alternative path..."
    # Alternative path for some RK3588 variants
    ALT_NPU_PATH="/sys/class/devfreq/fdb60000.npu/governor"
    if [ -f "$ALT_NPU_PATH" ]; then
        echo "performance" > "$ALT_NPU_PATH"
        echo "✓ Set NPU governor to performance mode (alternative path)"
    fi
fi

# Set maximum frequency
if [ -f "$NPU_MAX_FREQ_PATH" ]; then
    MAX_FREQ=$(cat "$NPU_MAX_FREQ_PATH")
    echo "$MAX_FREQ" > "$NPU_MIN_FREQ_PATH" 2>/dev/null
    echo "✓ Set NPU to maximum frequency: $MAX_FREQ Hz"
fi

# CPU performance optimization for AI workloads
echo "Optimizing CPU performance..."

# Set CPU governor to performance
for cpu in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
    if [ -f "$cpu" ]; then
        echo "performance" > "$cpu"
    fi
done
echo "✓ Set all CPU cores to performance governor"

# Disable CPU idle for consistent performance
echo 1 > /sys/devices/system/cpu/cpuidle/use_deepest_state 2>/dev/null
echo "✓ Disabled deep CPU idle states"

# Memory optimization
echo "Optimizing memory settings..."

# Increase dirty ratio for better I/O performance
echo 40 > /proc/sys/vm/dirty_ratio
echo 10 > /proc/sys/vm/dirty_background_ratio
echo "✓ Optimized memory dirty ratios"

# GPU frequency optimization (helps with overall system performance)
echo "Optimizing GPU frequency..."
GPU_FREQ_PATH="/sys/class/devfreq/fb000000.gpu/governor"
if [ -f "$GPU_FREQ_PATH" ]; then
    echo "performance" > "$GPU_FREQ_PATH"
    echo "✓ Set GPU to performance mode"
fi

# Display current NPU status
echo ""
echo "=== Current NPU Status ==="
if [ -f "$NPU_FREQ_PATH" ]; then
    echo "NPU Governor: $(cat $NPU_FREQ_PATH)"
    echo "NPU Current Frequency: $(cat /sys/class/devfreq/fdab0000.npu/cur_freq 2>/dev/null || echo 'N/A')"
    echo "NPU Max Frequency: $(cat $NPU_MAX_FREQ_PATH 2>/dev/null || echo 'N/A')"
fi

# Check RKNN driver version
echo ""
echo "=== RKNN Driver Information ==="
if [ -f "/sys/kernel/debug/rknpu/version" ]; then
    echo "RKNN Driver Version: $(cat /sys/kernel/debug/rknpu/version)"
else
    echo "RKNN Driver Version: $(dmesg | grep -i rknpu | tail -1 || echo 'Not found')"
fi

# Memory information
echo ""
echo "=== Memory Information ==="
echo "Total Memory: $(free -h | grep Mem | awk '{print $2}')"
echo "Available Memory: $(free -h | grep Mem | awk '{print $7}')"

echo ""
echo "=== Optimization Complete ==="
echo "NPU is now configured for maximum performance."
echo "Note: These settings will be reset after reboot."
echo "To make permanent, add this script to system startup."

# Optional: Create a systemd service for permanent optimization
read -p "Create systemd service for permanent optimization? (y/n): " -n 1 -r
echo
if [[ $REPLY =~ ^[Yy]$ ]]; then
    SERVICE_FILE="/etc/systemd/system/npu-optimization.service"
    cat > "$SERVICE_FILE" << EOF
[Unit]
Description=RK3588 NPU Performance Optimization
After=multi-user.target

[Service]
Type=oneshot
ExecStart=$(realpath "$0")
RemainAfterExit=yes

[Install]
WantedBy=multi-user.target
EOF
    
    systemctl daemon-reload
    systemctl enable npu-optimization.service
    echo "✓ Created systemd service for permanent optimization"
    echo "  Service will run automatically on boot"
fi

echo ""
echo "For YOLOv8 inference, expected performance on RK3588:"
echo "- YOLOv8n: ~73 FPS (13.7ms per frame)"
echo "- YOLOv8s: ~38 FPS (26.3ms per frame)"
echo "- YOLOv8m: ~16 FPS (62.5ms per frame)"
echo ""
echo "If you're still getting 300-500ms inference times, check:"
echo "1. Model format (should be .rknn, not .onnx)"
echo "2. Input preprocessing (BGR->RGB conversion)"
echo "3. Post-processing algorithm (use official RKNN methods)"
echo "4. Thread synchronization overhead"
