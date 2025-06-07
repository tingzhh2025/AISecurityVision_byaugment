# Model Files Information

## 🎯 **TensorRT Model Pack Status**

由于网络限制，大模型文件暂时无法通过Git LFS推送到远程仓库。以下是模型文件的详细信息：

## 📦 **当前工作模型**

### **主要模型包**
- **文件名**: `Pikachu_x86_64.pack`
- **大小**: 60MB
- **版本**: TensorRT 10.11 Compatible
- **状态**: ✅ 已验证工作正常

### **模型包内容**
```
Pikachu_x86_64.pack (60MB)
├── _00_scrfd_2_5g_bnkps_shape640x640_fp16 (3.4MB)  # 人脸检测 640x640
├── _00_scrfd_2_5g_bnkps_shape320x320_fp16 (2.7MB)  # 人脸检测 320x320  
├── _00_scrfd_2_5g_bnkps_shape160x160_fp16 (2.3MB)  # 人脸检测 160x160
├── _01_hyplmkv2_0.25_112x_fp16 (1.6MB)             # 人脸关键点检测
├── _03_r18_Glint360K_fixed_fp16 (48.1MB)           # 人脸识别特征提取
├── _08_fairface_model_fp16 (1.4MB)                 # 年龄性别分析
├── _09_blink_crop_fp16 (0.5MB)                     # 眨眼检测
└── __inspire__ (配置文件)
```

## 🔄 **备份文件**

### **可用备份**
- `Pikachu_x86_64.pack.megatron_backup` (66MB) - 原始Megatron模型
- `Pikachu_x86_64.pack.backup4` (66MB) - 重建前备份
- `Pikachu_x86_64.pack.backup3` (32KB) - 兼容性测试版本

### **测试版本**
- `Pikachu_x86_64_real_tensorrt10.11.pack` (52KB) - 早期测试版本
- `Pikachu_x86_64.pack.placeholder` (12KB) - 占位符版本

## 🛠 **模型重建工具**

如果需要重新生成模型包，可以使用以下脚本：

### **1. 提取并重建现有模型**
```bash
python3 scripts/extract_and_rebuild_models.py
```

### **2. 从ONNX转换**
```bash
python3 scripts/convert_real_insightface_models.py
```

### **3. 创建兼容引擎**
```bash
python3 scripts/create_compatible_engines.py
```

## ✅ **验证状态**

### **系统测试结果**
- ✅ **模型加载**: InsightFace成功初始化
- ✅ **TensorRT引擎**: 所有7个引擎正常加载
- ✅ **人脸检测**: SCRFD模型正常工作
- ✅ **视频处理**: ~10 FPS处理速度
- ✅ **API集成**: 人员统计功能正常启用

### **性能指标**
- **检测精度**: 正常检测1个人员目标
- **处理速度**: ~10 FPS (1280x720视频)
- **内存使用**: ~2GB (包含所有模型)
- **启动时间**: 2-3秒完成初始化

## 🔧 **Git LFS配置**

已配置Git LFS跟踪以下文件类型：
```bash
git lfs track "*.pack"   # InsightFace模型包
git lfs track "*.onnx"   # ONNX模型文件  
git lfs track "*.engine" # TensorRT引擎文件
```

## 📋 **使用说明**

### **1. 确认模型文件存在**
```bash
ls -lh models/Pikachu_x86_64.pack
# 应该显示: -rw-rw-r-- 1 user user 60M 日期 Pikachu_x86_64.pack
```

### **2. 验证模型完整性**
```bash
# 检查模型包内容
tar -tf models/Pikachu_x86_64.pack

# 应该包含7个引擎文件和1个配置文件
```

### **3. 测试系统运行**
```bash
./build/AISecurityVision --config config/config_tensorrt.json
```

## 🚨 **故障排除**

### **如果模型文件丢失**
1. 从备份恢复：
   ```bash
   cp models/Pikachu_x86_64.pack.megatron_backup models/Pikachu_x86_64.pack
   ```

2. 重新构建：
   ```bash
   python3 scripts/extract_and_rebuild_models.py
   ```

### **如果出现兼容性问题**
1. 检查TensorRT版本：
   ```bash
   /usr/src/tensorrt/bin/trtexec --version
   ```

2. 重新转换模型：
   ```bash
   python3 scripts/convert_real_insightface_models.py
   ```

## 📞 **技术支持**

如果遇到模型相关问题，请：
1. 检查 `TENSORRT_MODEL_CONVERSION_SUCCESS.md` 文档
2. 运行 `scripts/test_model_compatibility.sh` 进行诊断
3. 查看系统日志获取详细错误信息

---
*更新时间: 2025-06-07*  
*模型版本: TensorRT 10.11 Compatible*  
*状态: 生产就绪*
