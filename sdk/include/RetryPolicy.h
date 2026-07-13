#pragma once
#include <chrono>
#include <functional>
#include <thread>
#include <cmath>
#include "ModelError.h"

namespace ai_chat_sdk{

// 可配置的重试策略
struct RetryPolicy {
    int maxRetries = 3;                    // 最大重试次数（不含首次调用）
    int baseDelayMs = 200;                 // 基础延迟（毫秒）
    int maxDelayMs = 5000;                 // 最大延迟（毫秒）
    float backoffMultiplier = 2.0f;        // 退避倍数

    // 计算第 n 次重试的延迟（n 从 1 开始）
    int delayFor(int attempt) const {
        int delay = static_cast<int>(baseDelayMs * std::pow(backoffMultiplier, attempt - 1));
        return delay > maxDelayMs ? maxDelayMs : delay;
    }
};

// 判断某个 ModelError 是否可重试
inline bool isRetryable(const ModelError& err) {
    return err.code == ModelErrorCode::Timeout
        || err.code == ModelErrorCode::RateLimited
        || err.code == ModelErrorCode::ServerError
        || err.code == ModelErrorCode::NetworkError;
}

// 通用重试执行器：调用 fn()，失败时按 policy 退避重试
// fn 的返回类型必须包含 ok() 与 trace_id 字段（ModelError 兼容）
// 注意：sleep 是阻塞的；适合在非主反应线程调用
template<typename Fn>
ModelError withRetry(const RetryPolicy& policy, const std::string& provider, Fn&& fn) {
    ModelError lastErr;
    for(int attempt = 0; attempt <= policy.maxRetries; ++attempt) {
        ModelError err = fn();
        if(err.ok()) {
            return ModelError{};  // 成功
        }
        lastErr = err;
        lastErr.provider = provider;
        if(!isRetryable(err) || attempt == policy.maxRetries) {
            return lastErr;
        }
        int delayMs = policy.delayFor(attempt + 1);
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }
    return lastErr;
}

} // end ai_chat_sdk
