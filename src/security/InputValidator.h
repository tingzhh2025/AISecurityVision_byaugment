#pragma once

#include <string>
#include <vector>
#include <regex>
#include <unordered_set>
#include <functional>
#include <atomic>
#include "../core/Logger.h"

namespace AISecurityVision {

/**
 * @brief 输入验证和安全过滤器
 * 
 * 提供全面的输入验证和安全过滤功能：
 * - SQL注入防护
 * - XSS攻击防护
 * - 路径遍历防护
 * - 输入格式验证
 * - 数据清理和转义
 */
class InputValidator {
public:
    /**
     * @brief 验证结果
     */
    struct ValidationResult {
        bool valid;             // 是否有效
        std::string message;    // 错误消息
        std::string sanitized;  // 清理后的数据
        
        ValidationResult(bool v = true, const std::string& msg = "", const std::string& clean = "")
            : valid(v), message(msg), sanitized(clean) {}
    };

    /**
     * @brief 验证规则类型
     */
    enum class ValidationType {
        ALPHANUMERIC,       // 字母数字
        EMAIL,              // 邮箱地址
        URL,                // URL地址
        IP_ADDRESS,         // IP地址
        CAMERA_ID,          // 摄像头ID
        USERNAME,           // 用户名
        PASSWORD,           // 密码
        JSON,               // JSON格式
        RTSP_URL,           // RTSP URL
        FILE_PATH,          // 文件路径
        SQL_SAFE,           // SQL安全
        HTML_SAFE,          // HTML安全
        CUSTOM              // 自定义规则
    };

public:
    /**
     * @brief 构造函数
     */
    InputValidator();

    /**
     * @brief 析构函数
     */
    ~InputValidator() = default;

    /**
     * @brief 验证输入数据
     * @param input 输入数据
     * @param type 验证类型
     * @param maxLength 最大长度限制
     * @return 验证结果
     */
    ValidationResult validate(const std::string& input, ValidationType type, size_t maxLength = 1024);

    /**
     * @brief 使用自定义正则表达式验证
     * @param input 输入数据
     * @param pattern 正则表达式模式
     * @param errorMessage 错误消息
     * @param maxLength 最大长度限制
     * @return 验证结果
     */
    ValidationResult validateWithRegex(const std::string& input, const std::string& pattern, 
                                     const std::string& errorMessage, size_t maxLength = 1024);

    /**
     * @brief SQL注入检测和防护
     * @param input 输入数据
     * @return 验证结果
     */
    ValidationResult validateSqlSafe(const std::string& input);

    /**
     * @brief XSS攻击检测和防护
     * @param input 输入数据
     * @return 验证结果
     */
    ValidationResult validateHtmlSafe(const std::string& input);

    /**
     * @brief 路径遍历攻击检测
     * @param path 文件路径
     * @return 验证结果
     */
    ValidationResult validateFilePath(const std::string& path);

    /**
     * @brief 验证JSON格式
     * @param json JSON字符串
     * @return 验证结果
     */
    ValidationResult validateJson(const std::string& json);

    /**
     * @brief 验证RTSP URL格式
     * @param url RTSP URL
     * @return 验证结果
     */
    ValidationResult validateRtspUrl(const std::string& url);

    /**
     * @brief 验证IP地址格式
     * @param ip IP地址字符串
     * @return 验证结果
     */
    ValidationResult validateIpAddress(const std::string& ip);

    /**
     * @brief 验证邮箱地址格式
     * @param email 邮箱地址
     * @return 验证结果
     */
    ValidationResult validateEmail(const std::string& email);

    /**
     * @brief 验证用户名格式
     * @param username 用户名
     * @return 验证结果
     */
    ValidationResult validateUsername(const std::string& username);

    /**
     * @brief 验证密码强度
     * @param password 密码
     * @return 验证结果
     */
    ValidationResult validatePassword(const std::string& password);

    /**
     * @brief 验证摄像头ID格式
     * @param cameraId 摄像头ID
     * @return 验证结果
     */
    ValidationResult validateCameraId(const std::string& cameraId);

    /**
     * @brief HTML实体编码
     * @param input 输入字符串
     * @return 编码后的字符串
     */
    std::string htmlEncode(const std::string& input);

    /**
     * @brief HTML实体解码
     * @param input 编码的字符串
     * @return 解码后的字符串
     */
    std::string htmlDecode(const std::string& input);

    /**
     * @brief URL编码
     * @param input 输入字符串
     * @return 编码后的字符串
     */
    std::string urlEncode(const std::string& input);

    /**
     * @brief URL解码
     * @param input 编码的字符串
     * @return 解码后的字符串
     */
    std::string urlDecode(const std::string& input);

    /**
     * @brief 移除危险字符
     * @param input 输入字符串
     * @return 清理后的字符串
     */
    std::string removeDangerousChars(const std::string& input);

    /**
     * @brief 转义SQL特殊字符
     * @param input 输入字符串
     * @return 转义后的字符串
     */
    std::string escapeSql(const std::string& input);

    /**
     * @brief 检查是否包含恶意模式
     * @param input 输入字符串
     * @return 是否包含恶意模式
     */
    bool containsMaliciousPattern(const std::string& input);

    /**
     * @brief 添加自定义恶意模式
     * @param pattern 正则表达式模式
     */
    void addMaliciousPattern(const std::string& pattern);

    /**
     * @brief 设置最大输入长度
     * @param maxLength 最大长度
     */
    void setMaxInputLength(size_t maxLength);

    /**
     * @brief 获取验证统计信息
     * @return 统计信息JSON字符串
     */
    std::string getValidationStats() const;

private:
    /**
     * @brief 初始化预定义的恶意模式
     */
    void initializeMaliciousPatterns();

    /**
     * @brief 检查长度限制
     * @param input 输入字符串
     * @param maxLength 最大长度
     * @return 是否超过限制
     */
    bool checkLengthLimit(const std::string& input, size_t maxLength);

    /**
     * @brief 记录验证事件
     * @param type 验证类型
     * @param input 输入数据
     * @param result 验证结果
     */
    void logValidationEvent(ValidationType type, const std::string& input, const ValidationResult& result);

private:
    // 预编译的正则表达式
    std::regex m_emailRegex;
    std::regex m_urlRegex;
    std::regex m_ipv4Regex;
    std::regex m_ipv6Regex;
    std::regex m_alphanumericRegex;
    std::regex m_usernameRegex;
    std::regex m_cameraIdRegex;
    std::regex m_rtspUrlRegex;
    
    // 恶意模式检测
    std::vector<std::regex> m_maliciousPatterns;
    std::unordered_set<std::string> m_sqlKeywords;
    std::unordered_set<std::string> m_htmlTags;
    
    // 配置参数
    size_t m_maxInputLength;
    
    // 统计信息
    mutable std::atomic<uint64_t> m_totalValidations{0};
    mutable std::atomic<uint64_t> m_failedValidations{0};
    mutable std::atomic<uint64_t> m_maliciousAttempts{0};
};

} // namespace AISecurityVision
