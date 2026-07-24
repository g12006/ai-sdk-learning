#include "../include/ChatSDK.h"
#include "../include/LLMManager.h"
#include "../include/SessionManager.h"
#include "../include/DataManager.h"
#include "../include/DeepSeekProvider.h"
#include "../include/ChatGPTProvider.h"
#include "../include/GeminiProvider.h"
#include "../include/OllamaLLMProvider.h"
#include "../include/util/myLog.h"
#include <map>

namespace ai_chat_sdk {

// ----------------------------------------------------------------
// 构造函数：实例化三层组件
// ----------------------------------------------------------------
ChatSDK::ChatSDK(const std::string& dbPath)
{
    _dataManager    = std::make_unique<DataManager>(dbPath);
    _llmManager     = std::make_unique<LLMManager>();

    // 初始化数据库（建表）
    if (!_dataManager->initDataBase()) {
        ERR("ChatSDK: DataManager initialization failed.");
        return;
    }

    // SessionManager 依赖 DataManager，需在 DataManager 初始化完后再构造
    _sessionManager = std::make_unique<SessionManager>(*_dataManager);
    INFO("ChatSDK instantiated.");
}

ChatSDK::~ChatSDK() = default;

// ----------------------------------------------------------------
// 注册所有支持的 LLM Provider
//
// 注意：注册 key 必须与后续 initModel() 传入的 modelName 一致，
// 否则 LLMManager::_providers 查找不到对应 provider。
// 云端 API 模型名称固定，直接用模型名注册；
// Ollama 模型名由运行时配置决定，在 initOllamaModelProviders() 中动态注册。
// ----------------------------------------------------------------
void ChatSDK::registerAllProvider()
{
    // DeepSeek - 模型名: deepseek-chat
    _llmManager->registerProvider("deepseek-chat", std::make_unique<DeepSeekProvider>());

    // ChatGPT - 模型名: gpt-4o-mini
    _llmManager->registerProvider("gpt-4o-mini", std::make_unique<ChatGPTProvider>());

    // Gemini - 模型名: gemini-2.0-flash
    _llmManager->registerProvider("gemini-2.0-flash", std::make_unique<GeminiProvider>());

    // Ollama: 模型名动态，在 initOllamaModelProviders() 中按需注册

    INFO("All static providers registered.");
}

// ----------------------------------------------------------------
// 根据 Config 类型分派初始化逻辑
// ----------------------------------------------------------------
bool ChatSDK::initProviders(const std::vector<std::shared_ptr<Config>>& configs)
{
    bool allSuccess = true;
    for (const auto& config : configs) {
        // 尝试向下转型为 APIConfig
        const auto* apiCfg = dynamic_cast<const APIConfig*>(config.get());
        if (apiCfg) {
            allSuccess &= initAPIModelProviders(apiCfg->_modelName, *apiCfg);
            continue;
        }

        // 尝试向下转型为 OllamaConfig
        const auto* ollamaCfg = dynamic_cast<const OllamaConfig*>(config.get());
        if (ollamaCfg) {
            allSuccess &= initOllamaModelProviders(ollamaCfg->_modelName, *ollamaCfg);
            continue;
        }

        WARN("initProviders: unknown config type for model [{}], skipping.", config->_modelName);
    }
    return allSuccess;
}

// ----------------------------------------------------------------
// 初始化云端 API 模型
// ----------------------------------------------------------------
bool ChatSDK::initAPIModelProviders(const std::string& modelName, const APIConfig& config)
{
    std::map<std::string, std::string> params;
    params["api_key"]     = config._apiKey;
    params["temperature"] = std::to_string(config._temperature);
    params["max_tokens"]  = std::to_string(config._maxTokens);

    bool ok = _llmManager->initModel(modelName, params);
    if (!ok) {
        ERR("initAPIModelProviders failed for model [{}]", modelName);
    }
    return ok;
}

// ----------------------------------------------------------------
// 初始化本地 Ollama 模型
//
// Ollama 的模型名由运行时配置决定（如 deepseek-r1:1.5b），
// 无法在 registerAllProvider() 中预注册，因此在此按需注册。
// ----------------------------------------------------------------
bool ChatSDK::initOllamaModelProviders(const std::string& modelName, const OllamaConfig& config)
{
    // 按需注册 Ollama Provider（模型名动态）
    _llmManager->registerProvider(modelName, std::make_unique<OllamaLLMProvider>());

    std::map<std::string, std::string> params;
    params["model_name"]  = config._modelName;
    params["endpoint"]    = config._endpoint;
    params["model_desc"]  = config._modelDesc;
    params["temperature"] = std::to_string(config._temperature);
    params["max_tokens"]  = std::to_string(config._maxTokens);

    bool ok = _llmManager->initModel(modelName, params);
    if (!ok) {
        ERR("initOllamaModelProviders failed for model [{}]", modelName);
    }
    return ok;
}

// ----------------------------------------------------------------
// 统一初始化入口
// ----------------------------------------------------------------
bool ChatSDK::initModels(const std::vector<std::shared_ptr<Config>>& configs)
{
    // 1. 注册所有 Provider
    registerAllProvider();

    // 2. 根据配置初始化各 Provider
    bool ok = initProviders(configs);

    _initialized = ok;
    if (_initialized) {
        INFO("ChatSDK initialized successfully.");
    } else {
        ERR("ChatSDK initialization completed with errors.");
    }
    return ok;
}

// ----------------------------------------------------------------
// 获取可用模型列表
// ----------------------------------------------------------------
std::vector<ModelInfo> ChatSDK::getAvailableModels() const
{
    return _llmManager->getAvailableModels();
}

bool ChatSDK::isModelAvailable(const std::string& modelName) const
{
    return _llmManager->isModelAvailable(modelName);
}

// ----------------------------------------------------------------
// 创建会话
// ----------------------------------------------------------------
std::string ChatSDK::createSession(const std::string& modelName)
{
    if (!_initialized) {
        ERR("ChatSDK not initialized. Call initModels() first.");
        return "";
    }
    if (!_llmManager->isModelAvailable(modelName)) {
        ERR("createSession failed: model [{}] is not available.", modelName);
        return "";
    }
    return _sessionManager->createSession(modelName);
}

// ----------------------------------------------------------------
// 获取会话
// ----------------------------------------------------------------
std::shared_ptr<Session> ChatSDK::getSession(const std::string& sessionId)
{
    if (!_initialized) {
        ERR("ChatSDK not initialized.");
        return nullptr;
    }
    return _sessionManager->getSession(sessionId);
}

// ----------------------------------------------------------------
// 删除会话
// ----------------------------------------------------------------
bool ChatSDK::deleteSession(const std::string& sessionId)
{
    if (!_initialized) {
        ERR("ChatSDK not initialized.");
        return false;
    }
    return _sessionManager->deleteSession(sessionId);
}

// ----------------------------------------------------------------
// 获取所有会话 ID
// ----------------------------------------------------------------
std::vector<std::string> ChatSDK::getSessionLists() const
{
    if (!_initialized) {
        ERR("ChatSDK not initialized.");
        return {};
    }
    return _sessionManager->getSessionLists();
}

// ----------------------------------------------------------------
// 发送消息（同步，全量返回）
// ----------------------------------------------------------------
std::string ChatSDK::sendMessage(const std::string& sessionId,
                                 const std::string& userMessage,
                                 const std::map<std::string, std::string>& requestParam)
{
    if (!_initialized) {
        ERR("sendMessage failed: SDK not initialized.");
        return "";
    }

    // 1. 获取会话
    auto session = _sessionManager->getSession(sessionId);
    if (!session) {
        ERR("sendMessage failed: session not found [{}]", sessionId);
        return "";
    }

    // 2. 持久化用户消息
    _sessionManager->addMessage(sessionId, "user", userMessage);

    // 3. 获取最新消息历史（含刚追加的用户消息）
    auto updatedSession = _sessionManager->getSession(sessionId);
    const auto& messages = updatedSession->_messages;

    // 4. 调用模型
    std::string reply = _llmManager->sendMessage(session->_modelName, messages, requestParam);
    if (reply.empty()) {
        ERR("sendMessage: model [{}] returned empty reply.", session->_modelName);
        return "";
    }

    // 5. 持久化助手回复
    _sessionManager->addMessage(sessionId, "assistant", reply);

    DBG("sendMessage done: sessionId = {}, replyLen = {}", sessionId, reply.size());
    return reply;
}

// ----------------------------------------------------------------
// 发送消息（异步，流式响应）
// ----------------------------------------------------------------
std::string ChatSDK::sendMessageStream(const std::string& sessionId,
                                       const std::string& userMessage,
                                       const std::map<std::string, std::string>& requestParam,
                                       std::function<void(const std::string&, bool)> callback)
{
    if (!_initialized) {
        ERR("sendMessageStream failed: SDK not initialized.");
        return "";
    }

    // 1. 获取会话
    auto session = _sessionManager->getSession(sessionId);
    if (!session) {
        ERR("sendMessageStream failed: session not found [{}]", sessionId);
        return "";
    }

    // 2. 持久化用户消息
    _sessionManager->addMessage(sessionId, "user", userMessage);

    // 3. 获取最新消息历史
    auto updatedSession = _sessionManager->getSession(sessionId);
    const auto& messages = updatedSession->_messages;

    // 4. 包装回调：累积完整回复 + 推送增量给调用方 + 结束时持久化
    std::string fullResponse;
    auto wrappedCallback = [&](const std::string& delta, bool isFinished) {
        // 累积增量
        fullResponse += delta;

        // 推送给调用方（实时更新 UI）
        callback(delta, isFinished);

        // 流结束后持久化完整回复
        if (isFinished) {
            _sessionManager->addMessage(sessionId, "assistant", fullResponse);
            DBG("sendMessageStream finished: sessionId = {}, totalLen = {}",
                sessionId, fullResponse.size());
        }
    };

    // 5. 调用模型（流式）
    std::function<void(const std::string&, bool)> cb = wrappedCallback;
    _llmManager->sendMessageStream(session->_modelName, messages, requestParam, cb);

    return fullResponse;
}

} // end ai_chat_sdk
