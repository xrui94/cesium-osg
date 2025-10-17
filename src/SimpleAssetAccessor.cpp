#include "SimpleAssetAccessor.h"
#include "Log.h"

#include <CesiumAsync/AsyncSystem.h>
#include <CesiumUtility/Uri.h>

#include <curl/curl.h>

#include <fstream>
#include <algorithm>

// CURL写入回调函数
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::vector<std::byte>* data)
{
    size_t totalSize = size * nmemb;
    const std::byte* bytes = static_cast<const std::byte*>(contents);
    data->insert(data->end(), bytes, bytes + totalSize);
    return totalSize;
}

// URL解码函数
static std::string urlDecode(const std::string& str) {
    std::string ret;
    char ch;
    int i, ii;
    for (i = 0; i < str.length(); i++) {
        if (int(str[i]) == 37) { // %
            sscanf(str.substr(i + 1, 2).c_str(), "%x", &ii);
            ch = static_cast<char>(ii);
            ret += ch;
            i = i + 2;
        } else {
            ret += str[i];
        }
    }
    return ret;
}

// 
static std::string fileUrlToLocalPath(const std::string& url) {
    if (url.rfind("file://", 0) == 0) {
        std::string path = url.substr(7);
        if (path.size() > 2 && path[0] == '/' && std::isalpha(path[1]) && path[2] == ':') {
            path = path.substr(1);
        }
        path = urlDecode(path); // 关键：解码
        std::replace(path.begin(), path.end(), '/', '\\');
        return path;
    }
    return url;
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
    CO_DEBUG("HTTP Request to: {}", url);
    CO_TRACE("CURL result: {}", curl_easy_strerror(res));
    CO_TRACE("Response code: {}", responseCode);
    CO_TRACE("Content type: {}", contentTypeStr);
    CO_TRACE("Response data size: {} bytes", responseData.size());
    
    // 检查数据的前几个字节，确保是有效的.b3dm文件
    if (responseData.size() >= 4) {
        CO_TRACE("First 4 bytes: {:02x} {:02x} {:02x} {:02x}",
            static_cast<unsigned int>(static_cast<unsigned char>(responseData[0])),
            static_cast<unsigned int>(static_cast<unsigned char>(responseData[1])),
            static_cast<unsigned int>(static_cast<unsigned char>(responseData[2])),
            static_cast<unsigned int>(static_cast<unsigned char>(responseData[3]))
        );
    }
    
    // 清理
    if (headerList) {
        curl_slist_free_all(headerList);
    }
    curl_easy_cleanup(curl);
    
    // 处理CURL错误
    if (res != CURLE_OK) {
        // 网络连接失败或其他CURL错误
        CO_CRITICAL("Network error: {}", curl_easy_strerror(res));
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
        // 关键：立即返回，防止后续代码执行导致崩溃
        return request;
    }
    
    // 创建成功响应
    auto response = std::make_unique<SimpleAssetResponse>(
        static_cast<uint16_t>(responseCode),
        contentTypeStr,
        CesiumAsync::HttpHeaders{}
    );
    
    CO_TRACE("Setting response for request. Data size: {} bytes", responseData.size());
    
    response->setData(std::move(responseData));
    CO_TRACE("Setting response for request. Data size after move: {} bytes" << responseData.size());
    CO_TRACE("Response data successfully set");
    
    request->setResponse(std::move(response));
    
    CO_TRACE("Response set successfully");
    return request;
}

std::shared_ptr<CesiumAsync::IAssetRequest> SimpleAssetAccessor::performFileRequest(
    const CesiumAsync::AsyncSystem& asyncSystem,
    const std::string& filePath)
{
    auto request = std::make_shared<SimpleAssetRequest>("GET", filePath);

    // 支持 file:// 路径
    std::string localPath = fileUrlToLocalPath(filePath);
    
    std::ifstream file(localPath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        auto response = std::make_unique<SimpleAssetResponse>(404, "text/plain", CesiumAsync::HttpHeaders{});
        request->setResponse(std::move(response));
        return request;
    }
    
    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<std::byte> data(size);
    if (file.read(reinterpret_cast<char*>(data.data()), size)) {
        // 根据扩展名设置 content-type
        std::string contentType = "application/octet-stream";
        if (localPath.size() > 5 && localPath.substr(localPath.size() - 5) == ".json") {
            contentType = "application/json";
        }
        auto response = std::make_unique<SimpleAssetResponse>(200, contentType, CesiumAsync::HttpHeaders{});
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
    // return url.substr(0, 7) == "file://" || (url.find("://") == std::string::npos);
    // 支持 file:// 或本地磁盘路径
    return url.rfind("file://", 0) == 0 ||
           (url.size() > 2 && std::isalpha(url[0]) && url[1] == ':' && (url[2] == '/' || url[2] == '\\'));
}

void SimpleAssetAccessor::setBaseUrl(const std::string& baseUrl)
{
    m_baseUrl = baseUrl;
}

std::string SimpleAssetAccessor::resolveUrl(const std::string& url) const
{
    // 如果是 http(s) 直接返回
    if (isHttpUrl(url)) {
        return url;
    }
    // 如果是 file:// 或 file:///，直接返回
    if (url.rfind("file://", 0) == 0) {
        return url;
    }
    // 如果是 Windows 盘符路径（如 C:/xxx 或 C:\xxx），自动加 file:///
    if (url.size() > 2 && std::isalpha(url[0]) && url[1] == ':' && (url[2] == '/' || url[2] == '\\')) {
        std::string fixed = url;
        std::replace(fixed.begin(), fixed.end(), '\\', '/');
        return "file:///" + fixed;
    }
    // 其它情况，尝试用 baseUrl 解析
    if (!m_baseUrl.empty()) {
        return CesiumUtility::Uri::resolve(m_baseUrl, url);
    }
    // 默认直接加 file:///
    std::string fixed = url;
    std::replace(fixed.begin(), fixed.end(), '\\', '/');
    return "file:///" + fixed;
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