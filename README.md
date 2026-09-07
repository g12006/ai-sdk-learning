# C++ AI 多模型接入 SDK

用 C++17 从零实现的 LLM 统一接入 SDK 与聊天服务：以 **Provider 抽象层**屏蔽 DeepSeek / ChatGPT / Gemini / Ollama 四家模型的 API 差异，上层业务面向统一接口编程；配套 ChatServer（httplib HTTP + SSE 推流）与会话 SQLite 持久化，含 gtest 单元/集成测试。

**核心链路**：HTTP 请求进入 ChatServer → ChatSDK 门面经 SessionManager 装配历史消息 → LLMManager 按模型名查表路由到 Provider → Provider 组装各家 API 请求（全量 / 流式）→ 流式增量经 callback 逐帧上抛 → ChatServer 以 SSE 转推客户端，会话落库 SQLite 可恢复。

**架构亮点**：

- **Provider 抽象 + 查表转发（M×N → M+N）**：`LLMProvider` 基类定义 6 个纯虚接口，`LLMManager` 持有 `map<string, unique_ptr<LLMProvider>>` 查表转发；新增模型只需实现一个 Provider 子类，业务代码零改动
- **四种流式协议的统一分帧**：OpenAI 系 SSE（`\n\n` 分帧 + `[DONE]` 哨兵）、ChatGPT Responses API（`event:`/`data:` 双行、`response.completed` 收尾）、Ollama 行式 JSON（`\n` 分隔 + `done` 字段）——全部收敛为同一套 `callback(chunk, isLast)` 上层接口
- **Facade + 会话持久化**：`ChatSDK` 门面统一初始化/收发入口；`SessionManager` + `DataManager`（SQLite）支持会话恢复；真实 API 测试无 Key 时 `GTEST_SKIP` 自动跳过，CI 友好

## 架构

```
┌─────────────────────────────────────────────────┐
│                ChatServer (httplib)              │
│        7 个 REST 路由 + SSE 流式推流              │
├─────────────────────────────────────────────────┤
│              ChatSDK (Facade 统一入口)            │
│   SessionManager ── DataManager (SQLite 持久化)   │
├─────────────────────────────────────────────────┤
│                  LLMManager                      │
│      map<string, unique_ptr<LLMProvider>> 查表   │
├─────────┬──────────┬──────────┬─────────────────┤
│ DeepSeek │ ChatGPT  │ Gemini   │ Ollama(本地)     │
│ Provider │ Provider │ Provider │ Provider        │
├─────────┴──────────┴──────────┴─────────────────┤
│        httplib(HTTP 客户端/服务端) + jsoncpp      │
└─────────────────────────────────────────────────┘
```

## 目录结构

```
ai-sdk-learning/
├── CMakeLists.txt                 # 顶层构建配置
├── README.md
├── sdk/
│   ├── CMakeLists.txt             # SDK 静态库
│   ├── include/
│   │   ├── common.h              # 核心数据结构: Message/Config/APIConfig/OllamaConfig/Session/ModelInfo
│   │   ├── LLMProvider.h         # 抽象基类（6 个纯虚函数）
│   │   ├── DeepSeekProvider.h    # DeepSeek-chat
│   │   ├── ChatGPTProvider.h     # GPT-4o-mini (Responses API)
│   │   ├── GeminiProvider.h      # Gemini (OpenAI 兼容端点)
│   │   ├── OllamaLLMProvider.h   # Ollama 本地模型
│   │   ├── LLMManager.h          # 模型管理器
│   │   ├── ChatSDK.h             # 统一对外门面
│   │   ├── SessionManager.h      # 会话管理
│   │   ├── DataManager.h         # SQLite 持久化
│   │   └── util/myLog.h          # 日志工具
│   └── src/
│       ├── DeepSeekProvider.cpp   # 全量 + SSE 流式
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

## 流式协议差异（Provider 层已屏蔽）

OpenAI 的 Responses API（`/v1/responses`）与 Chat Completions API 有显著差异：

| 对比项 | Chat Completions (DeepSeek/Gemini) | Responses API (ChatGPT) | Ollama |
|--------|-----------------------------------|------------------------|--------|
| 端点 | `/v1/chat/completions` | `/v1/responses` | `/api/chat`（本地） |
| 消息字段 | `messages` | `input` | `messages` |
| 最大 Token | `max_tokens` | `max_output_tokens` | — |
| 全量响应路径 | `choices[0].message.content` | `output[0].content[0].text` | `message.content` |
| 流式分帧 | `data: {...}\n\n` | SSE: `event:` / `data:` 双行 | `\n` 分隔的 JSON 行 |
| 流式增量字段 | `choices[0].delta.content` | `response.output_text.delta` | `message.content`（逐行） |
| 流式结束标记 | `data: [DONE]` | `event: response.completed` | `done: true` 字段 |

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

## 依赖

- cpp-httplib、jsoncpp、sqlite3、gflags、googletest
- C++17 / CMake

## License

MIT
