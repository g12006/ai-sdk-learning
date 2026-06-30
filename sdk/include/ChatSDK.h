#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "common.h"

namespace ai_chat_sdk {

class LLMManager;
class SessionManager;
class DataManager;

/**
 * @brief ChatSDK - AI 聊天 SDK 统一入口（Facade 模式）
 *
 * ChatSDK 是整个 SDK 的唯一对外接口，封装了：
 *  - 模型初始化与注册（DeepSeek / ChatGPT / Gemini / Ollama）
 *  - 会话生命周期管理（创建 / 查询 / 删除）
 *  - 消息路由（同步全量 / 异步流式）
 *
 * 典型使用流程：
 *  1. 构造 ChatSDK 实例
 *  2. 调用 initModels() 传入配置列表，完成模型注册与初始化
 *  3. 调用 createSession() 创建会话，获得 sessionId
 *  4. 调用 sendMessage() 或 sendMessageStream() 发送消息
 *  5. 调用 deleteSession() 清理会话（可选）
 */
class ChatSDK {
public:
    /**
     * @brief 构造函数
     * @param dbPath SQLite 数据库路径，默认 "chatDB.db"
     *
     * 内部自动实例化 DataManager、SessionManager、LLMManager，
     * 并从数据库恢复历史会话。
     */
    explicit ChatSDK(const std::string& dbPath = "chatDB.db");

    /** @brief 析构函数 */
    ~ChatSDK();

    // ----------------------------------------------------------------
    // 模型初始化
    // ----------------------------------------------------------------

    /**
     * @brief 初始化所有模型提供商
     * @param configs Config 对象指针列表（APIConfig / OllamaConfig）
     * @return 全部初始化成功返回 true；任一失败返回 false
     *
     * 内部流程：
     *  1. registerAllProvider()：向 LLMManager 注册所有支持的 Provider
     *  2. initProviders()：依据 configs 类型（APIConfig / OllamaConfig）
     *     分别调用对应的初始化方法
     */
    bool initModels(const std::vector<std::shared_ptr<Config>>& configs);

    // ----------------------------------------------------------------
    // 模型查询
    // ----------------------------------------------------------------

    /**
     * @brief 获取当前可用的模型列表
     * @return ModelInfo 列表（仅包含 _isAvailable == true 的模型）
     */
    std::vector<ModelInfo> getAvailableModels() const;

    /**
     * @brief 检查指定模型是否可用
     * @param modelName 模型名称
     * @return 可用返回 true
     */
    bool isModelAvailable(const std::string& modelName) const;

    // ----------------------------------------------------------------
    // 会话管理
    // ----------------------------------------------------------------

    /**
     * @brief 创建新会话
     * @param modelName 绑定的模型名称（必须已初始化且可用）
     * @return 新会话的 session_id；失败返回空字符串
     */
    std::string createSession(const std::string& modelName);

    /**
     * @brief 获取指定会话
     * @param sessionId 会话 ID
     * @return 指向 Session 的 shared_ptr；不存在返回 nullptr
     */
    std::shared_ptr<Session> getSession(const std::string& sessionId);

    /**
     * @brief 删除指定会话
     * @param sessionId 要删除的会话 ID
     * @return 删除成功返回 true
     */
    bool deleteSession(const std::string& sessionId);

    /**
     * @brief 获取所有会话 ID
     * @return session_id 列表
     */
    std::vector<std::string> getSessionLists() const;

    // ----------------------------------------------------------------
    // 消息发送
    // ----------------------------------------------------------------

    /**
     * @brief 发送消息（同步，全量返回）
     * @param sessionId   目标会话 ID
     * @param userMessage 用户消息内容
     * @param requestParam 附加请求参数（如 temperature 覆盖等）
     * @return 模型完整回复字符串；失败返回空字符串
     *
     * 流程：
     *  1. 校验 SDK 已初始化、会话存在
     *  2. 拉取历史消息列表
     *  3. 追加用户消息并写入数据库
     *  4. 调用 LLMManager::sendMessage()
     *  5. 将助手回复追加到会话并写入数据库
     */
    std::string sendMessage(const std::string& sessionId,
                            const std::string& userMessage,
                            const std::map<std::string, std::string>& requestParam = {});

    /**
     * @brief 发送消息（异步，流式响应）
     * @param sessionId    目标会话 ID
     * @param userMessage  用户消息内容
     * @param requestParam 附加请求参数
     * @param callback     增量回调：void(const std::string& delta, bool isFinished)
     *                     - delta：本次增量文本
     *                     - isFinished：是否为最后一个增量
     * @return 完整的助手回复（流结束后返回）；失败返回空字符串
     *
     * 内部包装器：
     *  - 执行用户 callback 推送增量给调用方（实时更新 UI）
     *  - 累积所有增量到 fullResponse
     *  - isFinished == true 时持久化完整回复到数据库
     */
    std::string sendMessageStream(const std::string& sessionId,
                                  const std::string& userMessage,
                                  const std::map<std::string, std::string>& requestParam,
                                  std::function<void(const std::string&, bool)> callback);

private:
    /**
     * @brief 注册所有支持的 LLM Provider
     *
     * 向 _llmManager 依次注册：
     *  - DeepSeekProvider  → "deepseek"
     *  - ChatGPTProvider   → "chatgpt"
     *  - GeminiProvider    → "gemini"
     *  - OllamaProvider    → "ollama"
     */
    void registerAllProvider();

    /**
     * @brief 根据 Config 类型分派初始化逻辑
     * @param configs Config 指针列表
     * @return 全部成功返回 true
     */
    bool initProviders(const std::vector<std::shared_ptr<Config>>& configs);

    /**
     * @brief 初始化云端 API 模型（APIConfig）
     * @param modelName 模型标识符
     * @param config    APIConfig 对象
     * @return 初始化成功返回 true
     */
    bool initAPIModelProviders(const std::string& modelName, const APIConfig& config);

    /**
     * @brief 初始化本地 Ollama 模型（OllamaConfig）
     * @param modelName 模型标识符
     * @param config    OllamaConfig 对象
     * @return 初始化成功返回 true
     */
    bool initOllamaModelProviders(const std::string& modelName, const OllamaConfig& config);

private:
    bool _initialized = false; ///< SDK 是否已完成初始化

    std::unique_ptr<DataManager>    _dataManager;    ///< 持久化层
    std::unique_ptr<SessionManager> _sessionManager; ///< 会话管理层
    std::unique_ptr<LLMManager>     _llmManager;     ///< 模型管理层
};

} // end ai_chat_sdk
