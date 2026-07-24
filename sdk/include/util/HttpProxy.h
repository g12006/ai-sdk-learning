#pragma once
#include <httplib.h>
#include <cstdlib>
#include <string>

namespace ai_chat_sdk {

/**
 * @brief 从环境变量读取 HTTP/HTTPS 代理并应用到 httplib::Client
 *
 * 支持的环境变量（按优先级）：
 *   - https:// endpoint  → https_proxy / HTTPS_PROXY
 *   - 回退              → http_proxy  / HTTP_PROXY
 *
 * 代理 URL 格式：http://host:port
 *
 * @param client   待配置的 httplib::Client
 * @param endpoint 目标服务端点 URL（用于决定使用 https_proxy 还是 http_proxy）
 */
inline void applyProxyFromEnv(httplib::Client& client, const std::string& endpoint) {
    const char* proxyEnv = nullptr;

    // https endpoint 优先使用 https_proxy
    if (endpoint.size() >= 8 && endpoint.compare(0, 8, "https://") == 0) {
        proxyEnv = std::getenv("https_proxy");
        if (!proxyEnv || proxyEnv[0] == '\0') proxyEnv = std::getenv("HTTPS_PROXY");
    }

    // 回退到 http_proxy
    if (!proxyEnv || proxyEnv[0] == '\0') {
        proxyEnv = std::getenv("http_proxy");
        if (!proxyEnv || proxyEnv[0] == '\0') proxyEnv = std::getenv("HTTP_PROXY");
    }

    if (!proxyEnv || proxyEnv[0] == '\0') {
        return;  // 未配置代理，直连
    }

    // 提取 endpoint 中的 host，用于 NO_PROXY 匹配
    std::string host;
    {
        std::string rest = endpoint;
        size_t schemeEnd = rest.find("://");
        if (schemeEnd != std::string::npos) {
            rest = rest.substr(schemeEnd + 3);
        }
        size_t slash = rest.find('/');
        if (slash != std::string::npos) {
            rest = rest.substr(0, slash);
        }
        size_t colon = rest.find(':');
        host = (colon != std::string::npos) ? rest.substr(0, colon) : rest;
    }

    // 检查 NO_PROXY：若 host 命中则不走代理（如 localhost / 127.0.0.1）
    const char* noProxyEnv = std::getenv("no_proxy");
    if (!noProxyEnv || noProxyEnv[0] == '\0') noProxyEnv = std::getenv("NO_PROXY");
    if (noProxyEnv && noProxyEnv[0] != '\0') {
        std::string noProxy(noProxyEnv);
        // 逐项匹配（支持 .domain 后缀匹配）
        size_t start = 0;
        while (start <= noProxy.size()) {
            size_t comma = noProxy.find(',', start);
            std::string item = (comma == std::string::npos)
                ? noProxy.substr(start)
                : noProxy.substr(start, comma - start);
            // 去除首尾空白
            while (!item.empty() && (item.front() == ' ' || item.front() == '\t')) item.erase(0, 1);
            while (!item.empty() && (item.back() == ' ' || item.back() == '\t')) item.pop_back();
            if (!item.empty()) {
                if (item[0] == '.') {
                    // 后缀匹配：host 以 item 结尾
                    if (host.size() >= item.size() &&
                        host.compare(host.size() - item.size(), item.size(), item) == 0) {
                        return;  // 命中 NO_PROXY，直连
                    }
                } else if (host == item) {
                    return;  // 精确匹配，直连
                }
            }
            if (comma == std::string::npos) break;
            start = comma + 1;
        }
    }

    std::string proxyUrl(proxyEnv);

    // 去掉协议前缀 http:// 或 https://
    std::string hostPort = proxyUrl;
    size_t schemeEnd = proxyUrl.find("://");
    if (schemeEnd != std::string::npos) {
        hostPort = proxyUrl.substr(schemeEnd + 3);
    }

    // 去掉末尾的 /
    if (!hostPort.empty() && hostPort.back() == '/') {
        hostPort.pop_back();
    }

    // 分割 host:port
    std::string proxyHost;
    int port = 0;
    size_t colon = hostPort.find(':');
    if (colon != std::string::npos) {
        proxyHost = hostPort.substr(0, colon);
        try {
            port = std::stoi(hostPort.substr(colon + 1));
        } catch (...) {
            return;  // 端口解析失败，放弃代理
        }
    } else {
        proxyHost = hostPort;
        port = 80;
    }

    if (!proxyHost.empty() && port > 0) {
        client.set_proxy(proxyHost, port);
    }
}

} // end ai_chat_sdk
