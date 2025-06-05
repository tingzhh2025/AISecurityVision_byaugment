#pragma once

#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include <atomic>
#include <chrono>
#include "../core/Logger.h"

namespace AISecurityVision {

/**
 * @brief Swagger UI集成组件
 * 
 * 提供API文档的Web界面：
 * - 自动生成OpenAPI规范
 * - 嵌入式Swagger UI
 * - 交互式API测试
 * - 实时API文档更新
 */
class SwaggerUI {
public:
    /**
     * @brief API端点信息
     */
    struct EndpointInfo {
        std::string method;         // HTTP方法
        std::string path;           // 路径
        std::string summary;        // 摘要
        std::string description;    // 描述
        std::string tag;            // 标签
        bool requiresAuth;          // 是否需要认证
        std::string requestSchema;  // 请求模式
        std::string responseSchema; // 响应模式
        
        EndpointInfo() : requiresAuth(false) {}
    };

    /**
     * @brief API模式定义
     */
    struct SchemaDefinition {
        std::string name;           // 模式名称
        std::string type;           // 类型
        std::string description;    // 描述
        std::string properties;     // 属性定义(JSON)
        std::vector<std::string> required; // 必需字段
        
        SchemaDefinition() = default;
    };

public:
    /**
     * @brief 构造函数
     */
    SwaggerUI();

    /**
     * @brief 析构函数
     */
    ~SwaggerUI() = default;

    /**
     * @brief 初始化Swagger UI
     * @param title API标题
     * @param version API版本
     * @param description API描述
     * @param baseUrl 基础URL
     * @return 是否成功
     */
    bool initialize(const std::string& title = "AI Security Vision API",
                   const std::string& version = "1.0.0",
                   const std::string& description = "AI安全视觉系统API接口",
                   const std::string& baseUrl = "http://localhost:8080");

    /**
     * @brief 添加API端点
     * @param endpoint 端点信息
     */
    void addEndpoint(const EndpointInfo& endpoint);

    /**
     * @brief 添加模式定义
     * @param schema 模式定义
     */
    void addSchema(const SchemaDefinition& schema);

    /**
     * @brief 生成OpenAPI规范JSON
     * @return OpenAPI规范JSON字符串
     */
    std::string generateOpenAPISpec() const;

    /**
     * @brief 生成Swagger UI HTML页面
     * @return HTML页面内容
     */
    std::string generateSwaggerUIHtml() const;

    /**
     * @brief 处理Swagger UI相关的HTTP请求
     * @param path 请求路径
     * @param method HTTP方法
     * @param response 响应内容
     * @return 是否处理了请求
     */
    bool handleRequest(const std::string& path, const std::string& method, std::string& response);

    /**
     * @brief 设置API服务器信息
     * @param url 服务器URL
     * @param description 服务器描述
     */
    void addServer(const std::string& url, const std::string& description = "");

    /**
     * @brief 设置联系信息
     * @param name 联系人姓名
     * @param email 邮箱地址
     * @param url 网站URL
     */
    void setContact(const std::string& name, const std::string& email = "", const std::string& url = "");

    /**
     * @brief 设置许可证信息
     * @param name 许可证名称
     * @param url 许可证URL
     */
    void setLicense(const std::string& name, const std::string& url = "");

    /**
     * @brief 添加标签
     * @param name 标签名称
     * @param description 标签描述
     */
    void addTag(const std::string& name, const std::string& description = "");

    /**
     * @brief 启用/禁用认证
     * @param enabled 是否启用
     * @param scheme 认证方案 (bearer, basic, apiKey)
     * @param description 认证描述
     */
    void setAuthentication(bool enabled, const std::string& scheme = "bearer", 
                          const std::string& description = "JWT认证令牌");

    /**
     * @brief 获取API统计信息
     * @return 统计信息JSON字符串
     */
    std::string getApiStats() const;

private:
    /**
     * @brief 生成路径定义
     * @return 路径定义JSON字符串
     */
    std::string generatePaths() const;

    /**
     * @brief 生成组件定义
     * @return 组件定义JSON字符串
     */
    std::string generateComponents() const;

    /**
     * @brief 生成安全定义
     * @return 安全定义JSON字符串
     */
    std::string generateSecurity() const;

    /**
     * @brief 生成服务器定义
     * @return 服务器定义JSON字符串
     */
    std::string generateServers() const;

    /**
     * @brief 生成标签定义
     * @return 标签定义JSON字符串
     */
    std::string generateTags() const;

    /**
     * @brief 转义JSON字符串
     * @param input 输入字符串
     * @return 转义后的字符串
     */
    std::string escapeJson(const std::string& input) const;

    /**
     * @brief 获取HTTP方法的颜色
     * @param method HTTP方法
     * @return 颜色代码
     */
    std::string getMethodColor(const std::string& method) const;

    /**
     * @brief 生成示例请求
     * @param endpoint 端点信息
     * @return 示例请求JSON
     */
    std::string generateExampleRequest(const EndpointInfo& endpoint) const;

    /**
     * @brief 生成示例响应
     * @param endpoint 端点信息
     * @return 示例响应JSON
     */
    std::string generateExampleResponse(const EndpointInfo& endpoint) const;

    /**
     * @brief 加载内置的模式定义
     */
    void loadBuiltinSchemas();

    /**
     * @brief 加载内置的端点定义
     */
    void loadBuiltinEndpoints();

private:
    // API基本信息
    std::string m_title;
    std::string m_version;
    std::string m_description;
    std::string m_baseUrl;
    
    // 联系和许可证信息
    std::string m_contactName;
    std::string m_contactEmail;
    std::string m_contactUrl;
    std::string m_licenseName;
    std::string m_licenseUrl;
    
    // 认证配置
    bool m_authEnabled;
    std::string m_authScheme;
    std::string m_authDescription;
    
    // API定义
    std::vector<EndpointInfo> m_endpoints;
    std::vector<SchemaDefinition> m_schemas;
    std::vector<std::pair<std::string, std::string>> m_servers; // URL, Description
    std::vector<std::pair<std::string, std::string>> m_tags;    // Name, Description
    
    // 统计信息
    mutable std::atomic<uint64_t> m_specRequests{0};
    mutable std::atomic<uint64_t> m_uiRequests{0};
    mutable std::chrono::system_clock::time_point m_startTime;
};

} // namespace AISecurityVision
