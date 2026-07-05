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
⏳ GeminiProvider    → 待学
⏳ OllamaProvider    → 待学
⏳ SessionManager    → 待学
⏳ ChatSDK           → 待学
```

## 目录结构

```
ai-sdk-learning/
├── CMakeLists.txt                 # 顶层构建配置
├── README.md
├── sdk/
│   ├── include/
│   │   ├── common.h              # 5 个数据结构
│   │   ├── LLMProvider.h         # 抽象基类
│   │   ├── DeepSeekProvider.h    # DeepSeek-chat
│   │   ├── ChatGPTProvider.h     # GPT-4o-mini (Responses API)
│   │   ├── LLMManager.h          # 模型管理器
│   │   ├── ChatSDK.h             # 统一对外门面
│   │   ├── SessionManager.h      # 会话管理
│   │   ├── DataManager.h         # SQLite 持久化
│   │   └── util/myLog.h          # 日志工具
│   └── src/
│       ├── DeepSeekProvider.cpp   # 全量 + 流式
│       ├── ChatGPTProvider.cpp    # 全量 + SSE 流式
│       ├── LLMManager.cpp         # 查表转发
│       ├── ChatSDK.cpp            # 门面实现
│       ├── SessionManager.cpp     # 会话管理
│       ├── DataManager.cpp        # 持久化
│       └── util/myLog.cpp         # 日志工具
└── test/
    ├── CMakeLists.txt             # 测试构建配置
    └── testLLM.cpp                # 单元测试 + 集成测试
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
# 构建测试
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make

# 运行测试
./test/testLLM

# 运行真实 API 测试（需设置环境变量）
export deepseek_apikey="sk-xxxxx"
export chatgpt_apikey="sk-xxxxx"
# 然后将 test/testLLM.cpp 中的 #if 0 改为 #if 1
```

---
📅 更新：2026-07-05
