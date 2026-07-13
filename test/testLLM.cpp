/**
 * @file testLLM.cpp
 * @brief AI Chat SDK 单元测试 & 集成测试
 *
 * 测试覆盖：
 *   1. DeepSeekProvider  — initModel / isAvailable / sendMessage / sendMessageStream
 *   2. ChatGPTProvider    — initModel / isAvailable / sendMessage / sendMessageStream
 *   3. ChatSDK 集成测试   — 统一入口：注册 → 初始化 → 多轮对话
 *
 * 使用方式：
 *   - 纯单元测试（不依赖网络/API Key）：直接运行
 *   - 真实 API 调用测试：需设置环境变量 + 取消 #if 1 → #if 0 注释
 *       deepseek_apikey   = 你的 DeepSeek API Key
 *       chatgpt_apikey    = 你的 OpenAI API Key
 *
 * 编译：
 *   mkdir build && cd build && cmake ../test && make
 *
 * 运行：
 *   ./testLLM
 */

#include <gtest/gtest.h>
#include <memory>
#include <spdlog/common.h>
#include "../sdk/include/DeepSeekProvider.h"
#include "../sdk/include/ChatGPTProvider.h"
#include "../sdk/include/GeminiProvider.h"
#include "../sdk/include/OllamaLLMProvider.h"
#include "../sdk/include/util/myLog.h"
#include "../sdk/include/ChatSDK.h"
#include <iostream>
#include <string>
#include <vector>
#include <map>

// ============================================================================
// 一、DeepSeekProvider 单元测试（无需 API Key）
// ============================================================================

/**
 * @brief 测试默认 endpoint（不传 endpoint 参数时，使用默认的 api.deepseek.com）
 */
TEST(DeepSeekProviderTest, InitModelWithDefaultEndpoint)
{
    auto provider = std::make_shared<ai_chat_sdk::DeepSeekProvider>();
    ASSERT_NE(provider, nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = "sk-test-key";

    // 不传 endpoint，使用默认值
    bool ok = provider->initModel(modelParam);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(provider->isAvailable());
    EXPECT_EQ(provider->getModelName(), "deepseek-chat");
    EXPECT_FALSE(provider->getModelDesc().empty());
}

/**
 * @brief 测试自定义 endpoint
 */
TEST(DeepSeekProviderTest, InitModelWithCustomEndpoint)
{
    auto provider = std::make_shared<ai_chat_sdk::DeepSeekProvider>();
    ASSERT_NE(provider, nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = "sk-custom-key";
    modelParam["endpoint"] = "https://custom-api.example.com";

    bool ok = provider->initModel(modelParam);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(provider->isAvailable());
    EXPECT_EQ(provider->getModelName(), "deepseek-chat");
}

/**
 * @brief 测试缺少 api_key 时初始化失败
 */
TEST(DeepSeekProviderTest, InitModelWithoutApiKey)
{
    auto provider = std::make_shared<ai_chat_sdk::DeepSeekProvider>();
    ASSERT_NE(provider, nullptr);

    std::map<std::string, std::string> modelParam;
    // 故意不传 api_key

    bool ok = provider->initModel(modelParam);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(provider->isAvailable());
}

/**
 * @brief 测试模型未初始化时 sendMessage 返回空串
 */
TEST(DeepSeekProviderTest, SendMessageWhenNotAvailable)
{
    auto provider = std::make_shared<ai_chat_sdk::DeepSeekProvider>();
    ASSERT_NE(provider, nullptr);

    // 未初始化
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你好"});
    std::map<std::string, std::string> requestParam;

    std::string reply = provider->sendMessage(messages, requestParam);
    EXPECT_TRUE(reply.empty());
}

// ============================================================================
// 二、ChatGPTProvider 单元测试（无需 API Key）
// ============================================================================

/**
 * @brief 测试 ChatGPTProvider 默认 endpoint 初始化
 */
TEST(ChatGPTProviderTest, InitModelWithDefaultEndpoint)
{
    auto provider = std::make_shared<ai_chat_sdk::ChatGPTProvider>();
    ASSERT_NE(provider, nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = "sk-test-openai-key";

    // 不传 endpoint，默认使用 api.openai.com
    bool ok = provider->initModel(modelParam);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(provider->isAvailable());
    EXPECT_EQ(provider->getModelName(), "gpt-4o-mini");
    EXPECT_FALSE(provider->getModelDesc().empty());
}

/**
 * @brief 测试 ChatGPTProvider 自定义 endpoint
 */
TEST(ChatGPTProviderTest, InitModelWithCustomEndpoint)
{
    auto provider = std::make_shared<ai_chat_sdk::ChatGPTProvider>();
    ASSERT_NE(provider, nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = "sk-custom-key";
    modelParam["endpoint"] = "https://api.openai-proxy.com";

    bool ok = provider->initModel(modelParam);
    ASSERT_TRUE(ok);
    ASSERT_TRUE(provider->isAvailable());
}

/**
 * @brief 测试缺少 api_key 时初始化失败
 */
TEST(ChatGPTProviderTest, InitModelWithoutApiKey)
{
    auto provider = std::make_shared<ai_chat_sdk::ChatGPTProvider>();
    ASSERT_NE(provider, nullptr);

    std::map<std::string, std::string> modelParam;
    // 故意不传 api_key

    bool ok = provider->initModel(modelParam);
    EXPECT_FALSE(ok);
    EXPECT_FALSE(provider->isAvailable());
}

/**
 * @brief 测试模型未初始化时 sendMessage 返回空串
 */
TEST(ChatGPTProviderTest, SendMessageWhenNotAvailable)
{
    auto provider = std::make_shared<ai_chat_sdk::ChatGPTProvider>();
    ASSERT_NE(provider, nullptr);

    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "Hello"});
    std::map<std::string, std::string> requestParam;

    std::string reply = provider->sendMessage(messages, requestParam);
    EXPECT_TRUE(reply.empty());
}

// ============================================================================
// 三、真实 API 调用测试（无对应环境变量时自动 SKIP，不会失败）
// ============================================================================

#if 1   // 启用真实 API 测试

/**
 * @brief DeepSeek 真实 API：流式消息测试
 *
 * 前置条件：
 *   export deepseek_apikey="sk-xxxxx"
 */
TEST(DeepSeekProviderTest, SendMessageStreamRealApi)
{
    auto provider = std::make_shared<ai_chat_sdk::DeepSeekProvider>();
    ASSERT_NE(provider, nullptr);

    const char* apiKey = std::getenv("deepseek_apikey");
    if (!apiKey) {
        GTEST_SKIP() << "未设置环境变量 deepseek_apikey，跳过真实 API 测试";
    }

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = apiKey;
    modelParam["endpoint"] = "https://api.deepseek.com";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"max_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    auto writeChunk = [&](const std::string& chunk, bool last) {
        INFO("chunk : {}", chunk);
        if (last) {
            INFO("[DONE]");
        }
    };
    std::string fullData = provider->sendMessageStream(messages, requestParam, writeChunk);
    ASSERT_FALSE(fullData.empty());
    INFO("response : {}", fullData);
}

/**
 * @brief ChatGPT 真实 API：流式消息测试
 *
 * 前置条件：
 *   export chatgpt_apikey="sk-xxxxx"
 *
 * 注意：ChatGPTProvider 使用 OpenAI Responses API (/v1/responses)，
 *       消息体使用 "input" 字段，响应解析 output[0].content[0].text
 */
TEST(ChatGPTProviderTest, SendMessageStreamRealApi)
{
    auto provider = std::make_shared<ai_chat_sdk::ChatGPTProvider>();
    ASSERT_NE(provider, nullptr);

    const char* apiKey = std::getenv("chatgpt_apikey");
    if (!apiKey) {
        GTEST_SKIP() << "未设置环境变量 chatgpt_apikey，跳过真实 API 测试";
    }

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = apiKey;
    modelParam["endpoint"] = "https://api.openai.com";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"max_output_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    auto writeChunk = [&](const std::string& chunk, bool last) {
        INFO("chunk : {}", chunk);
        if (last) {
            INFO("[DONE]");
        }
    };
    std::string fullData = provider->sendMessageStream(messages, requestParam, writeChunk);
    ASSERT_FALSE(fullData.empty());
    INFO("response : {}", fullData);
}

/**
 * @brief Gemini 真实 API：流式消息测试
 *
 * 前置条件：
 *   export gemini_apikey="xxxxx"
 *
 * Gemini 使用 OpenAI 兼容端点 /v1beta/openai/chat/completions
 */
TEST(GeminiProviderTest, SendMessageStreamRealApi)
{
    auto provider = std::make_shared<ai_chat_sdk::GeminiProvider>();
    ASSERT_NE(provider, nullptr);

    const char* apiKey = std::getenv("gemini_apikey");
    if (!apiKey) {
        GTEST_SKIP() << "未设置环境变量 gemini_apikey，跳过真实 API 测试";
    }

    std::map<std::string, std::string> modelParam;
    modelParam["api_key"] = apiKey;
    modelParam["endpoint"] = "https://generativelanguage.googleapis.com";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"max_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    auto writeChunk = [&](const std::string& chunk, bool last) {
        INFO("chunk : {}", chunk);
        if (last) {
            INFO("[DONE]");
        }
    };
    std::string fullData = provider->sendMessageStream(messages, requestParam, writeChunk);
    ASSERT_FALSE(fullData.empty());
    INFO("response : {}", fullData);
}

/**
 * @brief Ollama 本地服务：流式消息测试
 *
 * 前置条件：
 *   - 本机已启动 Ollama 服务（默认 http://localhost:11434）
 *   - 已拉取模型：ollama pull qwen2.5:0.5b（或其它小模型）
 *
 * 无 Ollama 服务时自动 SKIP，不会失败
 */
TEST(OllamaLLMProviderTest, SendMessageStreamLocal)
{
    auto provider = std::make_shared<ai_chat_sdk::OllamaLLMProvider>();
    ASSERT_NE(provider, nullptr);

    std::map<std::string, std::string> modelParam;
    modelParam["model_name"] = "qwen2.5:0.5b";
    modelParam["model_desc"] = "Ollama local model";
    modelParam["endpoint"]  = "http://localhost:11434";

    provider->initModel(modelParam);
    ASSERT_TRUE(provider->isAvailable());

    std::map<std::string, std::string> requestParam = {
        {"temperature", "0.7"},
        {"max_tokens", "2048"}
    };
    std::vector<ai_chat_sdk::Message> messages;
    messages.push_back({"user", "你是谁？"});

    auto writeChunk = [&](const std::string& chunk, bool last) {
        INFO("chunk : {}", chunk);
        if (last) {
            INFO("[DONE]");
        }
    };
    std::string fullData = provider->sendMessageStream(messages, requestParam, writeChunk);
    if (fullData.empty()) {
        GTEST_SKIP() << "本地无 Ollama 服务或模型未拉取，跳过";
    }
    INFO("response : {}", fullData);
}

#endif  // 真实 API 测试块

// ============================================================================
// 四、ChatSDK 集成测试（多轮对话）
// ============================================================================

#if 0   // 改为 #if 1 启用真实 API 集成测试

/**
 * @brief ChatSDK 集成测试：DeepSeek + ChatGPT 多轮对话
 *
 * 通过 ChatSDK 统一入口，测试：
 *   - initModels 注册并初始化多个 Provider
 *   - createSession 创建会话
 *   - sendMessage 发送消息并获取回复
 *   - 会话历史持久化
 *
 * 前置条件：
 *   export deepseek_apikey="sk-xxxxx"
 *   export chatgpt_apikey="sk-xxxxx"
 */
TEST(ChatSDKTest, MultiModelConversation)
{
    auto sdk = std::make_shared<ai_chat_sdk::ChatSDK>();
    ASSERT_NE(sdk, nullptr);

    // ---- 配置 DeepSeek ----
    auto deepseekConfig = std::make_shared<ai_chat_sdk::APIConfig>();
    ASSERT_NE(deepseekConfig, nullptr);
    deepseekConfig->_modelName = "deepseek-chat";
    deepseekConfig->_apiKey = std::getenv("deepseek_apikey");
    ASSERT_FALSE(deepseekConfig->_apiKey.empty());
    deepseekConfig->_temperature = 0.7;
    deepseekConfig->_maxTokens = 2048;

    // ---- 配置 ChatGPT ----
    auto chatGPTConfig = std::make_shared<ai_chat_sdk::APIConfig>();
    ASSERT_NE(chatGPTConfig, nullptr);
    chatGPTConfig->_modelName = "gpt-4o-mini";
    chatGPTConfig->_apiKey = std::getenv("chatgpt_apikey");
    ASSERT_FALSE(chatGPTConfig->_apiKey.empty());
    chatGPTConfig->_temperature = 0.7;
    chatGPTConfig->_maxTokens = 2048;

    std::vector<std::shared_ptr<ai_chat_sdk::Config>> modelConfigs = {
        deepseekConfig, chatGPTConfig
    };

    bool initOk = sdk->initModels(modelConfigs);
    ASSERT_TRUE(initOk);

    // ---- 创建会话 ----
    auto sessionId = sdk->createSession(deepseekConfig->_modelName);
    ASSERT_FALSE(sessionId.empty());

    // ---- 多轮对话 ----
    std::string message;
    std::cout << ">>> ";
    std::getline(std::cin, message);
    auto response = sdk->sendMessage(sessionId, message);
    ASSERT_FALSE(response.empty());

    std::cout << ">>> ";
    std::getline(std::cin, message);
    response = sdk->sendMessage(sessionId, message);
    ASSERT_FALSE(response.empty());

    // ---- 获取会话历史 ----
    auto messages = sdk->_sessionManager.getHistroyMessages(sessionId);
    for (const auto& msg : messages) {
        std::cout << msg._role << ": " << msg._content << std::endl;
    }
    ASSERT_FALSE(messages.empty());
}

#endif  // ChatSDK 集成测试块

// ============================================================================
// 主函数
// ============================================================================
int main(int argc, char** argv)
{
    // 初始化 spdlog 日志库
    bite::Logger::initLogger("testLLM", "stdout", spdlog::level::debug);

    // 初始化 Google Test
    testing::InitGoogleTest(&argc, argv);

    // 执行所有测试用例
    return RUN_ALL_TESTS();
}
