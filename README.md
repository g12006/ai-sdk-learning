# AI SDK 项目学习笔记

> 基于 `zhibite-edu/ai-model-acess-tech` 的完整学习记录

## 项目简介

AI 模型统一接入 SDK，支持 4 种大模型（ChatGPT / DeepSeek / Gemini / Ollama），提供同步和流式两种调用方式。

## 学习路线

```
第一阶段：前置知识速通
  ├── ① jsoncpp    → JSON 序列化/反序列化（AI API 的"语言"）
  ├── ② httplib    → HTTP Client + Server（数据传输层）
  ├── ③ SSE 协议    → 流式响应格式（字一个字蹦出来）
  ├── ④ fmt        → 字符串格式化引擎（spdlog 底层）
  ├── ⑤ spdlog     → 高性能异步日志
  ├── ⑥ gflags     → 命令行参数解析
  └── ⑦ gtest      → 单元测试框架

第二阶段：架构理解
  └── 三层架构：ChatServer → ChatSDK → Provider

第三阶段：源码逐层走读
  ├── common.h        → 5 个核心数据结构
  ├── LLMProvider     → 抽象基类（6 个纯虚函数）
  ├── 4 个 Provider    → ChatGPT / DeepSeek / Gemini / Ollama
  ├── LLMManager      → 模型管理器（查表 + 转发）
  ├── SessionManager  → 双重缓存（内存 + SQLite）
  ├── ChatSDK         → 门面模式（合并 Manager）
  └── ChatServer      → HTTP 路由 + 响应构建

第四阶段：CMake 构建系统
  └── 看懂 3 个 CMakeLists.txt
```

## 核心技术栈

| 类别 | 技术 | 作用 |
|------|------|------|
| 构建 | CMake 3.10+ | 编译管理 |
| HTTP | cpp-httplib (header-only) | Client + Server |
| JSON | jsoncpp | 数据序列化 |
| 日志 | spdlog + fmt | 异步日志 |
| 数据库 | SQLite3 | 会话持久化 |
| 加密 | OpenSSL | HTTPS 通信 |
| 测试 | Google Test | 单元测试 |

## 设计模式

- **Provider 模式**（策略 + 工厂）：4 个 Provider 实现统一接口
- **Facade 模式**：ChatSDK 封装 LLMManager + SessionManager
- **双重缓存**：内存 unordered_map + SQLite 持久化
- **Config 多态**：APIConfig / OllamaConfig 通过 dynamic_pointer_cast 区分

## 学完能回答的面试问题

1. 4 种 AI 模型的 API 差异（端点、鉴权、JSON格式、SSE格式）
2. 流式响应如何保证 TCP 分包不丢数据？（buffer + \n\n 边界检测）
3. 双重缓存如何保证一致性？（先锁内存读写 → 解锁 → 异步写 DB）
4. Config 多态为什么需要 virtual 析构函数？（dynamic_pointer_cast 依赖 RTTI）
5. ChatSDK::sendMessage 的 6 步完整流程

---

📅 最后更新：2026-06-23
