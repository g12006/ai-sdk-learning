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
⏳ ChatGPTProvider   → 待学
⏳ GeminiProvider    → 待学
⏳ OllamaProvider    → 待学
⏳ SessionManager    → 待学
⏳ ChatSDK           → 待学
```

## 目录结构

```
sdk/
├── include/
│   ├── common.h              # 5 个数据结构
│   ├── LLMProvider.h         # 抽象基类
│   ├── DeepSeekProvider.h    # DeepSeek-chat
│   └── LLMManager.h          # 模型管理器
└── src/
    ├── DeepSeekProvider.cpp   # 全量 + 流式
    └── LLMManager.cpp         # 查表转发
```

---

📅 更新：2026-06-23
