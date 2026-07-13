#pragma once
#include <string>
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

namespace ai_chat_sdk{

// 模型错误码枚举
enum class ModelErrorCode{
    None,              // 无错误
    InvalidConfig,     // 配置无效
    NetworkError,      // 网络错误
    Timeout,           // 请求超时
    Unauthorized,      // 鉴权失败（API Key 无效等）
    RateLimited,       // 触发限流
    ServerError,       // 服务端错误（5xx 等）
    ParseError,        // 响应解析失败
    StreamError,       // 流式响应异常
    Unknown            // 未知错误
};

// 统一的错误类型，承载 provider / code / message / trace_id
struct ModelError{
    ModelErrorCode code = ModelErrorCode::None;   // 错误码
    std::string provider;                          // 出错的 provider 名称
    std::string message;                           // 错误描述
    std::string trace_id;                          // 链路追踪 ID

    // 判断是否成功（无错误）
    bool ok() const { return code == ModelErrorCode::None; }

    // 格式化输出：[provider=XXX][code=YYY] message (trace_id=ZZZ)
    std::string toString() const {
        std::ostringstream oss;
        oss << "[provider=" << (provider.empty() ? std::string("unknown") : provider) << "]";
        oss << "[code=" << errorCodeToString(code) << "]";
        oss << " " << (message.empty() ? std::string("no message") : message);
        oss << " (trace_id=" << (trace_id.empty() ? std::string("-") : trace_id) << ")";
        return oss.str();
    }

    // 错误码转字符串
    static std::string errorCodeToString(ModelErrorCode c){
        switch(c){
            case ModelErrorCode::None:           return "None";
            case ModelErrorCode::InvalidConfig:  return "InvalidConfig";
            case ModelErrorCode::NetworkError:   return "NetworkError";
            case ModelErrorCode::Timeout:        return "Timeout";
            case ModelErrorCode::Unauthorized:   return "Unauthorized";
            case ModelErrorCode::RateLimited:     return "RateLimited";
            case ModelErrorCode::ServerError:     return "ServerError";
            case ModelErrorCode::ParseError:      return "ParseError";
            case ModelErrorCode::StreamError:     return "StreamError";
            case ModelErrorCode::Unknown:
            default:                              return "Unknown";
        }
    }
};

// 生成链路追踪 ID：trace_<unix毫秒>_<4位十六进制随机数>
inline std::string generateTraceId(){
    // 获取当前时间戳（毫秒）
    auto now = std::chrono::system_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();

    // 生成 4 位十六进制随机数
    static thread_local std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 0xFFFF);
    int rnd = dist(rng);

    std::ostringstream oss;
    oss << "trace_" << ms << "_" << std::hex << std::setw(4) << std::setfill('0') << rnd;
    return oss.str();
}

} // end ai_chat_sdk
