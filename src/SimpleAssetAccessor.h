#pragma once

#include <CesiumAsync/IAssetAccessor.h>
#include <CesiumAsync/IAssetRequest.h>
#include <CesiumAsync/IAssetResponse.h>
#include <CesiumUtility/Uri.h>

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <span>

// 前向声明
namespace spdlog {
    class logger;
}

/**
 * @brief 简单的资源访问器实现，支持HTTP和文件系统访问
 */
class SimpleAssetAccessor : public CesiumAsync::IAssetAccessor
{
public:
    SimpleAssetAccessor();
    virtual ~SimpleAssetAccessor() = default;
    
    // IAssetAccessor接口实现
    virtual CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    get(const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& url,
        const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers = {}) override;
    
    virtual CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
    request(const CesiumAsync::AsyncSystem& asyncSystem,
            const std::string& verb,
            const std::string& url,
            const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers = {},
            const std::span<const std::byte>& contentPayload = {}) override;
    
    virtual void tick() noexcept override;
    
    // 设置基础URL用于解析相对URL
    void setBaseUrl(const std::string& baseUrl);
    
private:
    // HTTP请求实现
    std::shared_ptr<CesiumAsync::IAssetRequest> performHttpRequest(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& verb,
        const std::string& url,
        const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers,
        const std::span<const std::byte>& contentPayload);
    
    // 文件系统请求实现
    std::shared_ptr<CesiumAsync::IAssetRequest> performFileRequest(
        const CesiumAsync::AsyncSystem& asyncSystem,
        const std::string& filePath);
    
    // 判断是否为HTTP URL
    bool isHttpUrl(const std::string& url) const;
    
    // 判断是否为文件路径
    bool isFilePath(const std::string& url) const;
    
    // 解析URL（处理相对URL）
    std::string resolveUrl(const std::string& url) const;
    
    // 基础URL用于解析相对URL
    std::string m_baseUrl;
};

/**
 * @brief 简单的资产请求实现
 */
class SimpleAssetRequest : public CesiumAsync::IAssetRequest {
public:
    SimpleAssetRequest(const std::string& method, const std::string& url);
    virtual ~SimpleAssetRequest() = default;
    
    virtual const std::string& method() const override { return _method; }
    virtual const std::string& url() const override { return _url; }
    virtual const CesiumAsync::HttpHeaders& headers() const override { return _headers; }
    virtual const CesiumAsync::IAssetResponse* response() const override { return _response.get(); }
    
    void setResponse(std::unique_ptr<CesiumAsync::IAssetResponse> response) {
        _response = std::move(response);
    }
    
private:
    std::string _method;
    std::string _url;
    CesiumAsync::HttpHeaders _headers;
    std::unique_ptr<CesiumAsync::IAssetResponse> _response;
};

/**
 * @brief 简单的资产响应实现
 */
class SimpleAssetResponse : public CesiumAsync::IAssetResponse
{
public:
    SimpleAssetResponse(uint16_t statusCode, const std::string& contentType, const CesiumAsync::HttpHeaders& headers);
    virtual ~SimpleAssetResponse() = default;
    
    virtual uint16_t statusCode() const override { return m_statusCode; }
    virtual std::string contentType() const override { return m_contentType; }
    virtual const CesiumAsync::HttpHeaders& headers() const override { return m_headers; }
    virtual std::span<const std::byte> data() const override { return m_data; }
    
    void setData(std::vector<std::byte>&& data) {
        m_data = std::move(data);
    }
    
private:
    uint16_t m_statusCode;
    std::string m_contentType;
    CesiumAsync::HttpHeaders m_headers;
    std::vector<std::byte> m_data;
};