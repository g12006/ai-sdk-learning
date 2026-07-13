# AI SDK 项目学习

> 基于 `zhibite-edu/ai-model-acess-tech`，逐层走读源码

## 学习进度

```
✅ common.h          → 5 个核心数据结构
                       Message, Config(虚拟析构), APIConfig, OllamaConfig, Session, ModelInfo
✅ LLMProvider.h     → 抽象基类（6 个纯虚函数）
                       initModel / isAvailable / getModelName / getModelDesc / sendMessage / sendMessageStream
✅ LLMManager        → 查表转发
                       map<string, unique_ptr<LLMProvider>>，6 个方法全部 ≤17 行
✅ DeepSeekProvider  → sendMessage + sendMessageStream 完整走读
                       全量：POST /v1/chat/completions → choices[0].message.content
                       流式：buffer + \n\n → [DONE] → choices[0].delta.content → callback
✅ ChatGPTProvider   → sendMessage + sendMessageStream 完整走读
                       全量：POST /v1/responses → output[0].content[0].text
                       流式：SSE event/data 解析 → response.output_text.delta → callback
✅ GeminiProvider    → sendMessage + sendMessageStream 完整走读
                       OpenAI 兼容端点 /v1beta/openai/chat/completions
                       流式：SSE data 解析 → choices[0].delta.content → callback
✅ OllamaLLMProvider → sendMessage + sendMessageStream 完整走读
                       本地端点 /api/chat，按 \n 分隔 JSON 行
                       流式：每行 JSON → message.content → callback，done 标记结束
✅ SessionManager    → 会话生命周期 + SQLite 持久化恢复
                       createSession / getSession / deleteSession / getHistroyMessages
✅ ChatSDK           → Facade 模式统一入口
                       initModels → registerAllProvider + initProviders
                       sendMessage / sendMessageStream 双通道
✅ ChatServer        → httplib HTTP 服务 + SSE 推流
                       7 个 REST 路由 + gflags 命令行 / 环境变量配置
✅ testLLM           → 12 个测试用例（8 单元 + 4 真实 API，无 Key 时 GTEST_SKIP）
```

## 目录结构

```
ai-sdk-learning/
├── CMakeLists.txt                 # 顶层构建配置
├── README.md
├── sdk/
│   ├── CMakeLists.txt             # SDK 静态库
│   ├── include/
│   │   ├── common.h              # 5 个数据结构
│   │   ├── LLMProvider.h         # 抽象基类
│   │   ├── DeepSeekProvider.h    # DeepSeek-chat
│   │   ├── ChatGPTProvider.h     # GPT-4o-mini (Responses API)
│   │   ├── GeminiProvider.h      # Gemini (OpenAI 兼容)
│   │   ├── OllamaLLMProvider.h   # Ollama 本地模型
│   │   ├── LLMManager.h          # 模型管理器
│   │   ├── ChatSDK.h             # 统一对外门面
│   │   ├── SessionManager.h      # 会话管理
│   │   ├── DataManager.h         # SQLite 持久化
│   │   └── util/myLog.h          # 日志工具
│   └── src/
│       ├── DeepSeekProvider.cpp   # 全量 + 流式
│       ├── ChatGPTProvider.cpp    # 全量 + SSE 流式
│       ├── GeminiProvider.cpp      # 全量 + 流式
│       ├── OllamaLLMProvider.cpp   # 全量 + 行式流式
│       ├── LLMManager.cpp         # 查表转发
│       ├── ChatSDK.cpp            # 门面实现
│       ├── SessionManager.cpp     # 会话管理
│       ├── DataManager.cpp        # 持久化
│       └── util/myLog.cpp         # 日志工具
├── ChatServer/
│   ├── CMakeLists.txt             # 服务器构建配置
│   ├── ChatServer.h              # HTTP 服务 + 路由
│   ├── ChatServer.cpp            # SSE 流式响应
│   └── main.cpp                  # gflags 入口
└── test/
    ├── CMakeLists.txt             # 测试构建配置
    ├── testLLM.cpp                # 单元测试 + 集成测试
    └── testSQLite/
        └── testSqlite3.cpp        # SQLite 持久化测试
```

## ChatGPTProvider 要点

OpenAI 的 Responses API（`/v1/responses`）与 Chat Completions API 有显著差异：

| 对比项 | Chat Completions (DeepSeek) | Responses API (ChatGPT) |
|--------|---------------------------|------------------------|
| 端点 | `/v1/chat/completions` | `/v1/responses` |
| 消息字段 | `messages` | `input` |
| 最大 Token | `max_tokens` | `max_output_tokens` |
| 响应路径 | `choices[0].message.content` | `output[0].content[0].text` |
| 流式协议 | `data: {...}\n\n` + `[DONE]` | SSE: `event:` / `data:` 分行 |
| 流式增量事件 | `choices[0].delta.content` | `response.output_text.delta` → `delta` |
| 流式结束事件 | `data: [DONE]` | `event: response.completed` |

## 构建 & 测试

```bash
# 仅构建 SDK 静态库
mkdir build && cd build
cmake ..
make

# 构建 SDK + 测试 + ChatServer
cmake .. -DBUILD_TESTS=ON -DBUILD_CHAT_SERVER=ON
make -j4

# 安装 SDK 头文件到 /usr/local/include/ai_chat_sdk/（ChatServer 依赖此路径）
make install

# 运行单元测试（无 API Key 时真实 API 测试自动 SKIP）
./test/testLLM

# 运行真实 API 测试（设置环境变量后启用）
export deepseek_apikey="sk-xxxxx"
export chatgpt_apikey="sk-xxxxx"
export gemini_apikey="xxxxx"

# 启动 ChatServer（至少提供一个 API Key）
export deepseek_apikey="sk-xxxxx"
./AIChatServer --port=8080
```

测试覆盖：
- 8 个单元测试（无网络依赖）：DeepSeekProvider / ChatGPTProvider 的 initModel / isAvailable / sendMessage
- 4 个真实 API 测试（无环境变量时 GTEST_SKIP 自动跳过，不失败）：
  - DeepSeekProviderTest.SendMessageStreamRealApi
  - ChatGPTProviderTest.SendMessageStreamRealApi
  - GeminiProviderTest.SendMessageStreamRealApi
  - OllamaLLMProviderTest.SendMessageStreamLocal（无本地 Ollama 服务时 SKIP）

---

📅 更新：2026-07-13
