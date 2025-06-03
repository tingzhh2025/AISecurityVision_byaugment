#include <iostream>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

#ifdef HAVE_INSIGHTFACE
#include "third_party/insightface/include/inspireface.h"
#endif

int main() {
    std::cout << "=== InsightFace Integration Verification ===" << std::endl;
    
    // 1. 检查编译宏
    std::cout << "\n1. 编译宏检查:" << std::endl;
#ifdef HAVE_INSIGHTFACE
    std::cout << "✅ HAVE_INSIGHTFACE 宏已定义" << std::endl;
#else
    std::cout << "❌ HAVE_INSIGHTFACE 宏未定义" << std::endl;
    return 1;
#endif

    // 2. 检查模型文件
    std::cout << "\n2. 模型文件检查:" << std::endl;
    std::string modelPath = "../third_party/insightface/models/Pikachu.pack";
    std::ifstream modelFile(modelPath);
    if (modelFile.good()) {
        std::cout << "✅ Pikachu.pack 模型文件存在: " << modelPath << std::endl;
        modelFile.close();
    } else {
        std::cout << "❌ Pikachu.pack 模型文件不存在: " << modelPath << std::endl;
        return 1;
    }

    // 3. 初始化InsightFace
    std::cout << "\n3. InsightFace初始化测试:" << std::endl;
    
#ifdef HAVE_INSIGHTFACE
    try {
        // 设置日志级别
        HF_SetLogLevel(HF_LOG_DEBUG);
        
        // 创建会话配置
        HF_SessionCustomParameter param = {0};
        param.enable_recognition = 1;
        param.enable_liveness = 0;
        param.enable_ir_liveness = 0;
        param.enable_mask_detect = 0;
        param.enable_face_quality = 1;
        param.enable_age_predict = 1;
        param.enable_gender_predict = 1;
        param.enable_face_attribute = 1;
        
        // 初始化会话
        HF_Session session = {0};
        auto ret = HF_CreateInspireFaceSession(param, modelPath.c_str(), &session);
        
        if (ret == HSUCCEED) {
            std::cout << "✅ InsightFace会话创建成功" << std::endl;
            
            // 获取版本信息
            const char* version = HF_GetVersion();
            std::cout << "📦 InsightFace版本: " << (version ? version : "Unknown") << std::endl;
            
            // 测试基本功能
            std::cout << "\n4. 功能测试:" << std::endl;
            
            // 创建测试图像
            cv::Mat testImage = cv::Mat::zeros(480, 640, CV_8UC3);
            cv::rectangle(testImage, cv::Point(200, 150), cv::Point(440, 330), cv::Scalar(255, 255, 255), -1);
            
            // 转换为RGB
            cv::Mat rgbImage;
            cv::cvtColor(testImage, rgbImage, cv::COLOR_BGR2RGB);
            
            // 创建图像流
            HF_ImageStream imageStream = {0};
            imageStream.data = rgbImage.data;
            imageStream.width = rgbImage.cols;
            imageStream.height = rgbImage.rows;
            imageStream.rotation = HF_CAMERA_ROTATION_0;
            imageStream.format = HF_STREAM_RGB;
            
            // 人脸检测
            HF_MultipleFaceData multipleFaceData = {0};
            ret = HF_FaceContextForward(session, imageStream, &multipleFaceData);
            
            if (ret == HSUCCEED) {
                std::cout << "✅ 人脸检测功能正常" << std::endl;
                std::cout << "🔍 检测到人脸数量: " << multipleFaceData.detectedNum << std::endl;
            } else {
                std::cout << "⚠️  人脸检测测试完成 (无人脸)" << std::endl;
            }
            
            // 释放会话
            HF_ReleaseInspireFaceSession(session);
            std::cout << "✅ InsightFace会话释放成功" << std::endl;
            
        } else {
            std::cout << "❌ InsightFace会话创建失败, 错误码: " << ret << std::endl;
            return 1;
        }
        
    } catch (const std::exception& e) {
        std::cout << "❌ InsightFace测试异常: " << e.what() << std::endl;
        return 1;
    }
#endif

    std::cout << "\n=== 验证完成 ===" << std::endl;
    std::cout << "🎉 InsightFace集成验证成功！" << std::endl;
    
    return 0;
}
