# AI SDK 项目学习

> 基于 `zhibite-edu/ai-model-acess-tech`，逐层走读源码

## 学习进度

```
✅ common.h          → 5 个核心数据结构 (Message, Config, APIConfig, OllamaConfig, Session)
✅ LLMProvider.h     → 抽象基类（6 个纯虚函数）
✅ ChatGPTProvider   → GPT-4o-mini 全量 + 流式（SSE: event+data, response.completed）
✅ DeepSeekProvider  → DeepSeek-chat 全量 + 流式（SSE: 仅 data, [DONE]）
✅ GeminiProvider    → Gemini-2.0-flash 全量 + 流式
✅ OllamaLLMProvider → 本地模型 全量 + 流式（SSE: 单\n, {"done":true}）
✅ LLMManager        → 查表转发（map<string, unique_ptr<LLMProvider>>）
⏳ SessionManager    → 双重缓存（内存 + SQLite）
⏳ ChatSDK           → 门面模式（合并 Manager）
⏳ ChatServer        → HTTP 路由 + SSE 推送
```

## 目录结构

```
sdk/
├── include/
│   ├── common.h              # Message, Config, APIConfig, OllamaConfig, Session
│   ├── LLMProvider.h         # 抽象基类
│   ├── ChatGPTProvider.h     # GPT-4o-mini
│   ├── DeepSeekProvider.h    # DeepSeek-chat
│   ├── GeminiProvider.h      # Gemini-2.0-flash
│   ├── OllamaLLMProvider.h   # 本地 Ollama
│   ├── LLMManager.h          # 模型管理器
│   ├── SessionManager.h      # 会话管理器（双重缓存）
│   └── ChatSDK.h             # SDK 门面
└── src/
    ├── ChatGPTProvider.cpp    # 309 行
    ├── DeepSeekProvider.cpp   # 299 行
    ├── GeminiProvider.cpp
    ├── OllamaLLMProvider.cpp
    ├── LLMManager.cpp
    ├── SessionManager.cpp     # 224 行
    └── ChatSDK.cpp
```

---

📅 更新：2026-06-23
