# AI SDK 项目学习

> 基于 `zhibite-edu/ai-model-acess-tech`，逐层走读源码

## 学习进度

```
✅ common.h          → 5 个核心数据结构 (Message, Config, APIConfig, OllamaConfig, Session)
✅ LLMProvider.h     → 抽象基类（6 个纯虚函数：initModel, isAvailable, getModelName, getModelDesc, sendMessage, sendMessageStream）
✅ ChatGPTProvider   → GPT-4o-mini sendMessage 全量返回（88 行逐行走读）
                       JSON构造 → httplib POST → 解析 responses[0].content[0].text
✅ DeepSeekProvider  → DeepSeek-chat sendMessageStream 流式返回（完整走读）
                       buffer + \n\n 边界检测 → [DONE] 结束标记 → choices[0].delta.content
✅ LLMManager        → 查表转发（map<string, unique_ptr<LLMProvider>>）
                       6 个方法全部 ≤17 行，纯转发无逻辑
⏳ GeminiProvider    → 待学
⏳ OllamaProvider    → 待学
⏳ SessionManager    → 双重缓存（内存 + SQLite），待学
⏳ ChatSDK           → 门面模式，待学
⏳ ChatServer        → HTTP 路由 + SSE 推送，待学
```

## 目录结构

```
sdk/
├── include/
│   ├── common.h              # Message, Config, APIConfig, OllamaConfig, Session, ModelInfo
│   ├── LLMProvider.h         # 抽象基类（6 个纯虚函数）
│   ├── ChatGPTProvider.h     # GPT-4o-mini
│   ├── DeepSeekProvider.h    # DeepSeek-chat
│   └── LLMManager.h          # 模型管理器
└── src/
    ├── ChatGPTProvider.cpp    # 309 行
    ├── DeepSeekProvider.cpp   # 299 行
    └── LLMManager.cpp         # 查表转发
```

## 关键收获

| 知识点 | 在哪里学的 |
|--------|----------|
| jsoncpp 序列化/反序列化 | ChatGPTProvider::sendMessage 构造请求体 + 解析响应 |
| httplib Client POST | ChatGPTProvider 发 HTTPS 到 api.openai.com |
| httplib 流式 content_receiver | DeepSeekProvider::sendMessageStream |
| SSE \n\n 边界检测 + TCP 分包 | DeepSeek 的 buffer.find("\n\n") |
| 4 种 AI 模型的端点/鉴权/SSE格式差异 | ChatGPT vs DeepSeek 对比 |
| CMake 14 个命令 | 3 个 CMakeLists.txt 反讲 |
| Config 多态 + virtual 析构 | common.h 的 virtual ~Config() = default |
| unique_ptr 所有权转移 | LLMManager::registerProvider |

---

📅 更新：2026-06-23
