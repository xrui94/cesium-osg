#include "SimpleAssetAccessor.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumUtility/Uri.h>

#include <curl/curl.h>

#include <fstream>
#include <iostream>
#include <algorithm>

// CURL写入回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::vector<std::byte>* data)
{
    size_t totalSize = size * nmemb;
    const std::byte* bytes = static_cast<const std::byte*>(contents);
    data->insert(data->end(), bytes, bytes + totalSize);
    return totalSize;
}

// ============================== SimpleAssetAccessor实现 ==============================
SimpleAssetAccessor::SimpleAssetAccessor()
{
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
SimpleAssetAccessor::get(const CesiumAsync::AsyncSystem& asyncSystem,
                        const std::string& url,
                        const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers)
{
    return request(asyncSystem, "GET", url, headers, {});
}

CesiumAsync::Future<std::shared_ptr<CesiumAsync::IAssetRequest>>
SimpleAssetAccessor::request(const CesiumAsync::AsyncSystem& asyncSystem,
                            const std::string& verb,
                            const std::string& url,
                            const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers,
                            const std::span<const std::byte>& contentPayload)
{
    
    // 解析URL（处理相对URL）
    std::string resolvedUrl = resolveUrl(url);
    
    // 更新基础URL用于后续的相对URL解析
    if (isHttpUrl(resolvedUrl)) {
        CesiumUtility::Uri uri(resolvedUrl);
        if (uri.isValid()) {
            std::string scheme = std::string(uri.getScheme());
            std::string host = std::string(uri.getHost());
            std::string path = std::string(uri.getPath());
            
            // 获取目录路径（去掉文件名）
            size_t lastSlash = path.find_last_of('/');
            if (lastSlash != std::string::npos) {
                path = path.substr(0, lastSlash + 1);
            }
            
            m_baseUrl = scheme + "//" + host + path;
        }
    }
    
    return asyncSystem.runInWorkerThread([this, &asyncSystem, verb, resolvedUrl, headers, contentPayload]() {
        std::shared_ptr<CesiumAsync::IAssetRequest> request;
        
        if (isHttpUrl(resolvedUrl)) {
            request = performHttpRequest(asyncSystem, verb, resolvedUrl, headers, contentPayload);
        } else if (isFilePath(resolvedUrl)) {
            request = performFileRequest(asyncSystem, resolvedUrl);
        } else {
            // 创建错误响应
            auto simpleRequest = std::make_shared<SimpleAssetRequest>(verb, resolvedUrl);
            auto response = std::make_unique<SimpleAssetResponse>(404, "text/plain", CesiumAsync::HttpHeaders{});
            std::string errorMsg = "Unsupported URL scheme: " + resolvedUrl;
            std::vector<std::byte> errorData;
            errorData.reserve(errorMsg.size());
            std::transform(errorMsg.begin(), errorMsg.end(), std::back_inserter(errorData),
                          [](char c) { return static_cast<std::byte>(c); });
            response->setData(std::move(errorData));
            simpleRequest->setResponse(std::move(response));
            request = simpleRequest;
        }
        
        return request;
    });
}

void SimpleAssetAccessor::tick() noexcept {
    // 这里可以处理异步操作的清理工作
}

std::shared_ptr<CesiumAsync::IAssetRequest> SimpleAssetAccessor::performHttpRequest(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::string& verb,
    const std::string& url,
    const std::vector<CesiumAsync::IAssetAccessor::THeader>& headers,
    const std::span<const std::byte>& contentPayload)
{
    
    auto request = std::make_shared<SimpleAssetRequest>(verb, url);
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        auto response = std::make_unique<SimpleAssetResponse>(500, "text/plain", CesiumAsync::HttpHeaders{});
        request->setResponse(std::move(response));
        return request;
    }
    
    std::vector<std::byte> responseData;
    
    // 设置CURL选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
    
    // 设置请求方法
    if (verb == "POST") {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        if (!contentPayload.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, contentPayload.data());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, contentPayload.size());
        }
    } else if (verb == "PUT") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PUT");
    } else if (verb == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    }
    
    // 设置请求头
    struct curl_slist* headerList = nullptr;
    for (const auto& header : headers) {
        std::string headerStr = header.first + ": " + header.second;
        headerList = curl_slist_append(headerList, headerStr.c_str());
    }
    if (headerList) {
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
    }
    
    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    
    long responseCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
    
    char* contentType = nullptr;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
    std::string contentTypeStr = contentType ? contentType : "unknown";
    
    // 添加调试输出
    std::cout << "HTTP Request to: " << url << std::endl;
    std::cout << "CURL result: " << curl_easy_strerror(res) << std::endl;
    std::cout << "Response code: " << responseCode << std::endl;
    std::cout << "Content type: " << contentTypeStr << std::endl;
    std::cout << "Response data size: " << responseData.size() << " bytes" << std::endl;
    
    // 检查数据的前几个字节，确保是有效的.b3dm文件
    if (responseData.size() >= 4) {
        std::cout << "First 4 bytes: " << std::hex 
                  << (int)(unsigned char)responseData[0] << " "
                  << (int)(unsigned char)responseData[1] << " "
                  << (int)(unsigned char)responseData[2] << " "
                  << (int)(unsigned char)responseData[3] << std::dec << std::endl;
    }
    
    // 清理
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);
    
    // 处理CURL错误
    if (res != CURLE_OK) {
        // 网络连接失败或其他CURL错误
        std::cout << "Network error: " << curl_easy_strerror(res) << std::endl;
        auto response = std::make_unique<SimpleAssetResponse>(
            0, // 状态码设为0表示网络错误
            "text/plain",
            CesiumAsync::HttpHeaders{}
        );
        
        // 设置错误信息作为响应数据
        std::string errorMsg = std::string("Network error: ") + curl_easy_strerror(res);
        std::vector<std::byte> errorData;
        errorData.reserve(errorMsg.size());
        std::transform(errorMsg.begin(), errorMsg.end(), std::back_inserter(errorData),
                      [](char c) { return static_cast<std::byte>(c); });
        response->setData(std::move(errorData));
        
        request->setResponse(std::move(response));
        return request;
    }
    
    // 创建成功响应
    auto response = std::make_unique<SimpleAssetResponse>(
        static_cast<uint16_t>(responseCode),
        contentTypeStr,
        CesiumAsync::HttpHeaders{}
    );
    
    std::cout << "Setting response for request. Data size: " << responseData.size() << " bytes" << std::endl;
    
    response->setData(std::move(responseData));
    std::cout << "Setting response for request. Data size after move: " << responseData.size() << " bytes" << std::endl;
    std::cout << "Response data successfully set" << std::endl;
    
    request->setResponse(std::move(response));
    
    std::cout << "Response set successfully" << std::endl;
    return request;
}

std::shared_ptr<CesiumAsync::IAssetRequest> SimpleAssetAccessor::performFileRequest(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::string& filePath)
{
    
    auto request = std::make_shared<SimpleAssetRequest>("GET", filePath);
    
    std::ifstream file(filePath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        auto response = std::make_unique<SimpleAssetResponse>(404, "text/plain", CesiumAsync::HttpHeaders{});
        request->setResponse(std::move(response));
        return request;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<std::byte> data(size);
    if (file.read(reinterpret_cast<char*>(data.data()), size)) {
        auto response = std::make_unique<SimpleAssetResponse>(200, "application/octet-stream", CesiumAsync::HttpHeaders{});
        response->setData(std::move(data));
        request->setResponse(std::move(response));
    } else {
        auto response = std::make_unique<SimpleAssetResponse>(500, "text/plain", CesiumAsync::HttpHeaders{});
        request->setResponse(std::move(response));
    }
    
    return request;
}

bool SimpleAssetAccessor::isHttpUrl(const std::string& url) const
{
    return url.substr(0, 7) == "http://" || url.substr(0, 8) == "https://";
}

bool SimpleAssetAccessor::isFilePath(const std::string& url) const
{
    return url.substr(0, 7) == "file://" || (url.find("://") == std::string::npos);
}

void SimpleAssetAccessor::setBaseUrl(const std::string& baseUrl)
{
    m_baseUrl = baseUrl;
}

std::string SimpleAssetAccessor::resolveUrl(const std::string& url) const
{
    // 如果URL是绝对URL，直接返回
    if (isHttpUrl(url) || isFilePath(url)) {
        return url;
    }
    
    // 如果没有基础URL，返回原URL
    if (m_baseUrl.empty()) {
        return url;
    }
    
    // 使用cesium-native的Uri::resolve来解析相对URL
    std::string resolved = CesiumUtility::Uri::resolve(m_baseUrl, url);
    return resolved;
}

// ============================== SimpleAssetRequest实现 ==============================
SimpleAssetRequest::SimpleAssetRequest(const std::string& method, const std::string& url)
    : _method(method), _url(url)
{
}

// ============================== SimpleAssetResponse实现 ==============================
SimpleAssetResponse::SimpleAssetResponse(uint16_t statusCode, const std::string& contentType, const CesiumAsync::HttpHeaders& headers)
    : m_statusCode(statusCode), m_contentType(contentType), m_headers(headers)
{
}