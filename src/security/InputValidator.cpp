#include "InputValidator.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

using namespace AISecurityVision;

InputValidator::InputValidator() : m_maxInputLength(1024) {
    // 初始化正则表达式
    m_emailRegex = std::regex(R"([a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,})");
    m_urlRegex = std::regex(R"(^https?://[^\s/$.?#].[^\s]*$)");
    m_ipv4Regex = std::regex(R"(^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\.){3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$)");
    m_ipv6Regex = std::regex(R"(^(?:[0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4}$)");
    m_alphanumericRegex = std::regex(R"(^[a-zA-Z0-9_-]+$)");
    m_usernameRegex = std::regex(R"(^[a-zA-Z0-9_]{3,32}$)");
    m_cameraIdRegex = std::regex(R"(^camera_[a-zA-Z0-9_]{1,16}$)");
    m_rtspUrlRegex = std::regex(R"(^rtsp://[^\s/$.?#].[^\s]*$)");
    
    // 初始化恶意模式
    initializeMaliciousPatterns();
    
    LOG_INFO() << "[InputValidator] Initialized with max input length: " << m_maxInputLength;
}

InputValidator::ValidationResult InputValidator::validate(const std::string& input, ValidationType type, size_t maxLength) {
    m_totalValidations++;
    
    // 检查长度限制
    if (!checkLengthLimit(input, maxLength)) {
        m_failedValidations++;
        return ValidationResult(false, "Input exceeds maximum length limit", "");
    }
    
    ValidationResult result;
    
    switch (type) {
        case ValidationType::ALPHANUMERIC:
            result = std::regex_match(input, m_alphanumericRegex) ? 
                ValidationResult(true, "", input) : 
                ValidationResult(false, "Input must contain only alphanumeric characters, underscores, and hyphens", "");
            break;
            
        case ValidationType::EMAIL:
            result = validateEmail(input);
            break;
            
        case ValidationType::URL:
            result = std::regex_match(input, m_urlRegex) ? 
                ValidationResult(true, "", input) : 
                ValidationResult(false, "Invalid URL format", "");
            break;
            
        case ValidationType::IP_ADDRESS:
            result = validateIpAddress(input);
            break;
            
        case ValidationType::CAMERA_ID:
            result = validateCameraId(input);
            break;
            
        case ValidationType::USERNAME:
            result = validateUsername(input);
            break;
            
        case ValidationType::PASSWORD:
            result = validatePassword(input);
            break;
            
        case ValidationType::JSON:
            result = validateJson(input);
            break;
            
        case ValidationType::RTSP_URL:
            result = validateRtspUrl(input);
            break;
            
        case ValidationType::FILE_PATH:
            result = validateFilePath(input);
            break;
            
        case ValidationType::SQL_SAFE:
            result = validateSqlSafe(input);
            break;
            
        case ValidationType::HTML_SAFE:
            result = validateHtmlSafe(input);
            break;
            
        default:
            result = ValidationResult(false, "Unknown validation type", "");
            break;
    }
    
    // 检查恶意模式
    if (result.valid && containsMaliciousPattern(input)) {
        m_maliciousAttempts++;
        result = ValidationResult(false, "Input contains potentially malicious patterns", "");
    }
    
    if (!result.valid) {
        m_failedValidations++;
    }
    
    logValidationEvent(type, input, result);
    return result;
}

InputValidator::ValidationResult InputValidator::validateSqlSafe(const std::string& input) {
    // 检查SQL关键字
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    
    for (const auto& keyword : m_sqlKeywords) {
        if (lowerInput.find(keyword) != std::string::npos) {
            return ValidationResult(false, "Input contains SQL keywords", "");
        }
    }
    
    // 检查SQL特殊字符
    if (input.find('\'') != std::string::npos || 
        input.find('"') != std::string::npos ||
        input.find(';') != std::string::npos ||
        input.find("--") != std::string::npos ||
        input.find("/*") != std::string::npos ||
        input.find("*/") != std::string::npos) {
        return ValidationResult(false, "Input contains SQL special characters", escapeSql(input));
    }
    
    return ValidationResult(true, "", input);
}

InputValidator::ValidationResult InputValidator::validateHtmlSafe(const std::string& input) {
    // 检查HTML标签
    if (input.find('<') != std::string::npos || input.find('>') != std::string::npos) {
        return ValidationResult(false, "Input contains HTML tags", htmlEncode(input));
    }
    
    // 检查JavaScript事件
    std::string lowerInput = input;
    std::transform(lowerInput.begin(), lowerInput.end(), lowerInput.begin(), ::tolower);
    
    std::vector<std::string> jsEvents = {
        "onclick", "onload", "onerror", "onmouseover", "onmouseout", 
        "onfocus", "onblur", "onchange", "onsubmit", "javascript:"
    };
    
    for (const auto& event : jsEvents) {
        if (lowerInput.find(event) != std::string::npos) {
            return ValidationResult(false, "Input contains JavaScript events", htmlEncode(input));
        }
    }
    
    return ValidationResult(true, "", input);
}

InputValidator::ValidationResult InputValidator::validateFilePath(const std::string& path) {
    // 检查路径遍历攻击
    if (path.find("..") != std::string::npos ||
        path.find("./") != std::string::npos ||
        path.find("\\") != std::string::npos ||
        path.find("//") != std::string::npos) {
        return ValidationResult(false, "Path contains directory traversal patterns", "");
    }
    
    // 检查绝对路径
    if (path.empty() || path[0] == '/') {
        return ValidationResult(false, "Absolute paths are not allowed", "");
    }
    
    // 检查文件名长度
    if (path.length() > 255) {
        return ValidationResult(false, "Path too long", "");
    }
    
    return ValidationResult(true, "", path);
}

InputValidator::ValidationResult InputValidator::validateJson(const std::string& json) {
    try {
        auto parsed = nlohmann::json::parse(json);
        (void)parsed; // 避免未使用变量警告
        return ValidationResult(true, "", json);
    } catch (const std::exception& e) {
        return ValidationResult(false, "Invalid JSON format: " + std::string(e.what()), "");
    }
}

InputValidator::ValidationResult InputValidator::validateRtspUrl(const std::string& url) {
    if (!std::regex_match(url, m_rtspUrlRegex)) {
        return ValidationResult(false, "Invalid RTSP URL format", "");
    }
    
    // 检查URL长度
    if (url.length() > 512) {
        return ValidationResult(false, "RTSP URL too long", "");
    }
    
    return ValidationResult(true, "", url);
}

InputValidator::ValidationResult InputValidator::validateIpAddress(const std::string& ip) {
    if (std::regex_match(ip, m_ipv4Regex) || std::regex_match(ip, m_ipv6Regex)) {
        return ValidationResult(true, "", ip);
    }
    return ValidationResult(false, "Invalid IP address format", "");
}

InputValidator::ValidationResult InputValidator::validateEmail(const std::string& email) {
    if (email.length() > 254) {
        return ValidationResult(false, "Email address too long", "");
    }
    
    if (std::regex_match(email, m_emailRegex)) {
        return ValidationResult(true, "", email);
    }
    return ValidationResult(false, "Invalid email format", "");
}

InputValidator::ValidationResult InputValidator::validateUsername(const std::string& username) {
    if (!std::regex_match(username, m_usernameRegex)) {
        return ValidationResult(false, "Username must be 3-32 characters, alphanumeric and underscores only", "");
    }
    return ValidationResult(true, "", username);
}

InputValidator::ValidationResult InputValidator::validatePassword(const std::string& password) {
    if (password.length() < 8) {
        return ValidationResult(false, "Password must be at least 8 characters long", "");
    }
    
    if (password.length() > 128) {
        return ValidationResult(false, "Password too long", "");
    }
    
    // 检查密码强度
    bool hasUpper = false, hasLower = false, hasDigit = false, hasSpecial = false;
    
    for (char c : password) {
        if (std::isupper(c)) hasUpper = true;
        else if (std::islower(c)) hasLower = true;
        else if (std::isdigit(c)) hasDigit = true;
        else if (std::ispunct(c)) hasSpecial = true;
    }
    
    int strength = hasUpper + hasLower + hasDigit + hasSpecial;
    if (strength < 3) {
        return ValidationResult(false, "Password must contain at least 3 of: uppercase, lowercase, digits, special characters", "");
    }
    
    return ValidationResult(true, "", password);
}

InputValidator::ValidationResult InputValidator::validateCameraId(const std::string& cameraId) {
    if (!std::regex_match(cameraId, m_cameraIdRegex)) {
        return ValidationResult(false, "Camera ID must follow format: camera_[alphanumeric]", "");
    }
    return ValidationResult(true, "", cameraId);
}

std::string InputValidator::htmlEncode(const std::string& input) {
    std::string result;
    result.reserve(input.length() * 1.2);
    
    for (char c : input) {
        switch (c) {
            case '<': result += "&lt;"; break;
            case '>': result += "&gt;"; break;
            case '&': result += "&amp;"; break;
            case '"': result += "&quot;"; break;
            case '\'': result += "&#x27;"; break;
            default: result += c; break;
        }
    }
    
    return result;
}

std::string InputValidator::escapeSql(const std::string& input) {
    std::string result;
    result.reserve(input.length() * 1.2);
    
    for (char c : input) {
        if (c == '\'') {
            result += "''";
        } else if (c == '"') {
            result += "\"\"";
        } else {
            result += c;
        }
    }
    
    return result;
}

bool InputValidator::containsMaliciousPattern(const std::string& input) {
    for (const auto& pattern : m_maliciousPatterns) {
        if (std::regex_search(input, pattern)) {
            return true;
        }
    }
    return false;
}

void InputValidator::initializeMaliciousPatterns() {
    // SQL注入关键字
    m_sqlKeywords = {
        "select", "insert", "update", "delete", "drop", "create", "alter",
        "union", "where", "from", "join", "having", "group", "order",
        "exec", "execute", "sp_", "xp_", "script", "declare"
    };
    
    // HTML标签
    m_htmlTags = {
        "script", "iframe", "object", "embed", "form", "input", "textarea",
        "button", "select", "option", "link", "meta", "style"
    };
    
    // 恶意模式
    std::vector<std::string> patterns = {
        R"(\b(union|select|insert|update|delete|drop|create|alter)\b)",  // SQL关键字
        R"(<\s*script[^>]*>)",                                           // Script标签
        R"(javascript\s*:)",                                             // JavaScript协议
        R"(\bon\w+\s*=)",                                               // 事件处理器
        R"(\.\./)",                                                      // 路径遍历
        R"(\$\{.*\})",                                                   // 模板注入
        R"(\{\{.*\}\})",                                                 // 模板注入
        R"(eval\s*\()",                                                  // eval函数
        R"(exec\s*\()",                                                  // exec函数
    };
    
    for (const auto& pattern : patterns) {
        try {
            m_maliciousPatterns.emplace_back(pattern, std::regex_constants::icase);
        } catch (const std::exception& e) {
            LOG_ERROR() << "[InputValidator] Failed to compile regex pattern: " << pattern << " - " << e.what();
        }
    }
    
    LOG_INFO() << "[InputValidator] Initialized " << m_maliciousPatterns.size() << " malicious patterns";
}

bool InputValidator::checkLengthLimit(const std::string& input, size_t maxLength) {
    size_t limit = (maxLength > 0) ? maxLength : m_maxInputLength;
    return input.length() <= limit;
}

void InputValidator::logValidationEvent(ValidationType type, const std::string& input, const ValidationResult& result) {
    if (!result.valid) {
        LOG_WARN() << "[InputValidator] Validation failed - Type: " << static_cast<int>(type) 
                   << ", Input length: " << input.length() 
                   << ", Error: " << result.message;
    }
}
