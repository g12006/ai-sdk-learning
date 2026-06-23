# C++ AI 大模型接入 SDK — 项目总复习（面试版）

> 基于完整代码 + 9 个 PDF 课件 + 7 张板书图片整理
> 面试导向：架构 → 设计模式 → 核心代码 → 技术栈 → 高频追问

---

## 📐 一、项目架构总览

**一句话定位**：C++17 实现的 AI 大模型接入 SDK，统一封装 DeepSeek / ChatGPT / Gemini（云端）+ Ollama（本地）四大模型的访问接口，上层构建带 Web 前端的 HTTP 聊天服务器。

### 三层架构

```
┌─────────────────────────────────────────────┐
│  前端层 (build/www/)                          │
│  HTML + CSS + JS (marked.js + highlight.js)  │
│  SSE 流式接收 AI 回复                          │
└──────────────────┬──────────────────────────┘
                   │ HTTP / SSE
┌──────────────────┴──────────────────────────┐
│  应用层 (ChatServer/)                         │
│  httplib HTTP 服务器，RESTful API             │
│  路由：/sessions /models /chat /chat/stream   │
└──────────────────┬──────────────────────────┘
                   │ 调用 SDK
┌──────────────────┴──────────────────────────┐
│  SDK 层 (sdk/)  → libai_chat_sdk.a            │
│  ┌─────────────┐  ┌──────────────┐           │
│  │ ChatSDK     │  │ SessionMgr   │           │
│  │ (门面)      │  │ (会话管理)    │           │
│  └──────┬──────┘  └──────┬───────┘           │
│         │                │                    │
│  ┌──────┴──────┐  ┌──────┴───────┐           │
│  │ LLMManager  │  │ DataManager  │           │
│  │ (模型管理)   │  │ (SQLite持久化)│           │
│  └──────┬──────┘  └──────────────┘           │
│         │                                     │
│  ┌──────┴──────────────────────────┐         │
│  │      LLMProvider (抽象接口)       │         │
│  ├─────────┬─────────┬──────┬──────┤         │
│  │ChatGPT  │DeepSeek │Gemini│Ollama│         │
│  │Provider │Provider │Prov. │Prov. │         │
│  └─────────┴─────────┴──────┴──────┘         │
└─────────────────────────────────────────────┘
```

---

## 🎨 二、核心设计模式（面试必答）

### 2.1 策略模式 + 工厂模式（Provider 模式）

```cpp
// 抽象接口 — 6 个纯虚函数
class LLMProvider {
public:
    virtual bool initModel(const map<string,string>& config) = 0;
    virtual bool isAvailable() const = 0;
    virtual string getModelName() const = 0;
    virtual string getModelDesc() const = 0;
    virtual string sendMessage(const vector<Message>& msgs,
                               const map<string,string>& params) = 0;
    virtual string sendMessageStream(const vector<Message>& msgs,
                                     const map<string,string>& params,
                                     function<void(const string&, bool)> cb) = 0;
};

// 具体实现（4 个 Provider 各自处理 API 差异）
class ChatGPTProvider : public LLMProvider { ... };   // OpenAI API
class DeepSeekProvider : public LLMProvider { ... };  // DeepSeek API
class GeminiProvider  : public LLMProvider { ... };   // Google Gemini API
class OllamaLLMProvider : public LLMProvider { ... }; // 本地 Ollama

// 工厂管理器
class LLMManager {
    unordered_map<string, unique_ptr<LLMProvider>> _providers;
public:
    bool registerProvider(const string& name, unique_ptr<LLMProvider> p);
    string sendMessage(const string& model, const vector<Message>& msgs, ...);
};
```

**🎯 面试追问**：为什么用 `unique_ptr` 而不是 `shared_ptr` 管理 Provider？
Provider 的所有权唯一归属于 LLMManager，不需要共享。`unique_ptr` 零开销、语义明确，转移所有权用 `std::move`。

---

### 2.2 门面模式（Facade）

```cpp
class ChatSDK {
    LLMManager _llmManager;       // 组合模型管理
    SessionManager _sessionManager; // 组合会话管理
public:
    bool initModels(const vector<shared_ptr<Config>>& configs);
    string createSession(const string& modelName);
    string sendMessage(const string& sessionId, const string& message);
    string sendMessageStream(const string& sessionId, const string& message,
                              function<void(const string&, bool)> callback);
};
```

外部只需 `ChatSDK` 一个入口，不直接接触 LLMManager / SessionManager。

---

### 2.3 配置多态（向下转型安全）

```cpp
struct Config {            // 基类
    string _modelName;
    double _temperature = 0.7;
    int _maxTokens = 2048;
    virtual ~Config() = default;  // ⚠️ 虚析构：确保 dynamic_pointer_cast 安全
};

struct APIConfig : public Config {     // 云端模型配置
    string _apiKey;
};

struct OllamaConfig : public Config {  // 本地模型配置
    string _endpoint;
    string _modelDesc;
};

// ChatSDK 内部用 dynamic_pointer_cast 判断配置类型
void ChatSDK::registerAllProvider(const vector<shared_ptr<Config>>& configs) {
    for (auto& config : configs) {
        auto apiConfig = dynamic_pointer_cast<APIConfig>(config);
        if (apiConfig) {
            initAPIModelProviders(config->_modelName, apiConfig);
            continue;
        }
        auto ollamaConfig = dynamic_pointer_cast<OllamaConfig>(config);
        if (ollamaConfig) {
            initOllamaModelProviders(config->_modelName, ollamaConfig);
        }
    }
}
```

**🎯 面试追问**：为什么基类必须有虚析构？
`dynamic_pointer_cast` 依赖 RTTI（运行时类型信息），RTTI 只对有虚函数的类生效。虚析构使 Config 成为多态类型，向下转型才安全。

---

### 2.4 流式响应回调模式

```cpp
// SDK 层：Provider 通过回调推送增量数据
string ChatGPTProvider::sendMessageStream(..., function<void(const string&, bool)> callback) {
    httplib::Request request;
    request.content_receiver = [&](const char* data, size_t len, ...) {
        // 解析 SSE 事件流：data: {...}\n\n
        // 每解析出一段 delta，调用 callback(delta, false)
        // 流结束时调用 callback("", true)
        return true;
    };
    client.send(request);
}

// 服务层：ChatServer 用 httplib 的 chunked 推送给浏览器
void ChatServer::handleSendMessageStreamRequest(...) {
    response.set_chunked_content_provider(
        "text/event-stream",
        [&](size_t offset, httplib::DataSink& sink) {
            _chatSDK->sendMessageStream(sessionId, message,
                [&](const string& chunk, bool isLast) {
                    sink.write(chunk.data(), chunk.size());
                    if (isLast) sink.done();
                });
            return true;
        });
}
```

---

### 2.5 双缓存策略（SessionManager）

```
内存缓存: unordered_map<string, shared_ptr<Session>>  ← 快速查找
    ↕ 同步
SQLite 数据库: sessions 表 + messages 表              ← 持久化

启动时: 从数据库加载所有会话到内存
操作时: 先改内存，再写数据库（保证一致性）
```

---

## 💻 三、核心代码走读

### 3.1 ChatGPT Provider — 全量返回

```cpp
string ChatGPTProvider::sendMessage(const vector<Message>& messages,
                                     const map<string,string>& requestParam) {
    // 1. 构造 JSON 请求体
    Json::Value requestBody;
    requestBody["model"] = getModelName();        // "gpt-4o-mini"
    requestBody["input"] = messagesArray;          // 消息数组
    requestBody["temperature"] = temperature;
    requestBody["max_output_tokens"] = maxTokens;

    // 2. 序列化为字符串
    string bodyStr = Json::writeString(writerBuilder, requestBody);

    // 3. 创建 HTTP 客户端，设置代理（国内访问 OpenAI 需要代理）
    httplib::Client client(_endpoint.c_str());
    client.set_proxy("127.0.0.1", 7890);

    // 4. 发送 POST 请求
    httplib::Headers headers = { {"Authorization", "Bearer " + _apiKey} };
    auto response = client.Post("/v1/responses", headers, bodyStr, "application/json");

    // 5. 解析响应 JSON，提取回复文本
    Json::Value responseJson;
    Json::parseFromStream(reader, responseStream, &responseJson, &errs);
    string reply = responseJson["output"][0]["content"][0]["text"].asString();
    return reply;
}
```

### 3.2 ChatGPT Provider — 流式返回（SSE 解析）

```cpp
string ChatGPTProvider::sendMessageStream(..., callback) {
    // 关键：httplib 的 content_receiver 逐块接收数据
    request.content_receiver = [&](const char* data, size_t dataLength, ...) {
        buffer.append(data, dataLength);

        // SSE 协议：事件之间用 \n\n 分隔
        size_t pos = 0;
        while ((pos = buffer.find("\n\n", pos)) != string::npos) {
            string event = buffer.substr(0, pos);
            buffer.erase(0, pos + 2);

            // 解析 event: 和 data: 行
            // event: response.output_text.delta → 增量文本
            // event: response.completed → 流结束
            if (eventType == "response.output_text.delta") {
                string delta = chunk["delta"].asString();
                callback(delta, false);           // 推送增量给上层
            } else if (eventType == "response.completed") {
                callback("", true);               // 通知流结束
            }
        }
        return true;
    };
    client.send(request);
}
```

### 3.3 SessionManager — 会话管理

```cpp
class SessionManager {
    unordered_map<string, shared_ptr<Session>> _sessions;  // 内存缓存
    DataManager _dataManager;                               // SQLite 持久化
public:
    string createSession(const string& modelName) {
        string sessionId = generateUUID();
        auto session = make_shared<Session>(modelName);
        session->_sessionId = sessionId;
        _sessions[sessionId] = session;
        _dataManager.saveSession(*session);     // 同步写数据库
        return sessionId;
    }

    void addMessage(const string& sessionId, const Message& msg) {
        _sessions[sessionId]->_messages.push_back(msg);
        _dataManager.saveMessage(sessionId, msg);
    }
};
```

### 3.4 ChatServer — HTTP 路由

```cpp
void ChatServer::setHttpRoutes() {
    _chatServer->Get("/api/models",    [&](req, res){ handleGetModelListsRequest(req,res); });
    _chatServer->Post("/api/sessions", [&](req, res){ handleCreateSessionRequest(req,res); });
    _chatServer->Get("/api/sessions",  [&](req, res){ handleGetSessionListsRequest(req,res); });
    _chatServer->Delete("/api/sessions/([a-f0-9-]+)", [&](req, res){
        handleDeleteSessionRequest(req, res); });
    _chatServer->Post("/api/chat",      [&](req, res){ handleSendMessageRequest(req,res); });
    _chatServer->Post("/api/chat/stream",[&](req, res){ handleSendMessageStreamRequest(req,res); });

    // 静态资源挂载
    _chatServer->set_mount_point("/", "./www");
}
```

---

## 📚 四、PDF 课件知识点摘要

### 4.1 项目简介
- **背景**：AI 大模型爆火，程序员不能只停留在使用层面，要掌握底层接入技术
- **目标**：掌握大模型 API 接口、C++ 远程/本地接入、开发聊天应用
- **核心名词**：Model / LLM / Prompt / Context / Token / Temperature

### 4.2 AI 知识科普
- **大模型分类**：语言模型（LLM）、视觉模型（VLM）、多模态模型
- **主流模型**：GPT-5、Gemini-2.5-Pro、Claude-3.7、DeepSeek-V3、Qwen3
- **接入方式**：
  - 云端 API（HTTP POST + JSON + Bearer Token 认证）
  - 本地部署（Ollama / vLLM / LM Studio）
- **核心参数**：
  - `temperature`：0~2，越高越随机，越低越确定
  - `max_tokens`：最大生成令牌数
  - `stream`：是否流式返回

### 4.3 环境搭建
- **开发工具**：Trae（VSCode 内核）+ clangd + CMake Tools
- **依赖库**：httplib、jsoncpp、OpenSSL、spdlog、fmt、sqlite3、gflags、gtest
- **构建系统**：CMake（sdk 生成静态库，ChatServer 链接）

### 4.4 ChatSDK 核心
- **Provider 模式设计**：抽象基类 → 具体实现 → 工厂管理
- **配置体系**：Config 基类 + APIConfig / OllamaConfig 子类 + dynamic_pointer_cast
- **会话管理**：SessionManager 内存缓存 + SQLite 持久化
- **流式响应**：SSE 协议解析 + 回调机制

### 4.5 智能聊天助手
- **ChatServer**：httplib HTTP 服务器，7 个 RESTful API
- **前端**：单页应用，Markdown 渲染（marked.js）+ 代码高亮（highlight.js）
- **SSE 推送**：`set_chunked_content_provider` 实现 Server-Sent Events

### 4.6 项目扩展
- **多模态接入**：图片/语音/视频
- **RAG 检索增强**：向量数据库 + 文档嵌入
- **Agent 智能体**：工具调用 + 多步推理
- **Function Calling**：让 AI 调用外部函数

### 4.7 附录
- **SQLite**：嵌入式数据库，无需服务端，单文件存储，C API（sqlite3_open/exec/prepare/step）
- **ChatSDK 使用手册**：initModels → createSession → sendMessage 完整流程

---

## 🖼️ 五、板书图片内容

| 编号 | 内容描述 |
|:--:|------|
| 1.png | 项目整体架构图：前端↔HTTP服务器↔SDK↔四大模型 Provider |
| 2.png | 前端页面设计：聊天界面布局、消息气泡、Markdown 渲染 |
| 3.png | SDK 类图：LLMProvider 继承体系 + LLMManager + ChatSDK + SessionManager |
| 4.png | 四大 Provider 对比：API 端点、认证方式、请求格式差异 |
| 5.png | 会话管理流程：创建→发消息→流式返回→持久化 |
| 6.png | HTTP 服务路由设计：7 个 API 端点 + 静态资源挂载 |
| 7.png | 运行流程：浏览器→ChatServer→ChatSDK→Provider→AI API→流式返回 |

---

## 🛠️ 六、技术栈清单

| 类别 | 技术 | 用途 |
|------|------|------|
| **语言标准** | C++17 | structured bindings / optional / string_view |
| **HTTP 库** | cpp-httplib | HTTP 客户端（调 AI API）+ HTTP 服务器（ChatServer） |
| **JSON 库** | jsoncpp | 请求体序列化 + 响应体解析 |
| **加密库** | OpenSSL | HTTPS 支持（httplib 依赖） |
| **日志库** | spdlog | 异步日志，6 级日志（TRACE→CRIT） |
| **格式化** | fmt | spdlog 依赖，`fmt::format` |
| **命令行** | gflags | 服务器配置参数解析 |
| **数据库** | SQLite3 | 会话和消息持久化 |
| **测试** | Google Test | Provider 和 SDK 单元测试 |
| **前端** | HTML/CSS/JS | 单页聊天应用 |
| **Markdown** | marked.js | AI 回复 Markdown → HTML |
| **代码高亮** | highlight.js | 代码块语法高亮 |
| **流式协议** | SSE | text/event-stream，AI 流式响应传输 |

---

## 🎯 七、面试高频追问

### Q1：为什么用 Provider 模式而不是 if-else 判断模型类型？

**开放封闭原则**。新增模型只需新建一个 Provider 类继承 LLMProvider，在 ChatSDK 注册即可，不需要修改现有代码。if-else 方式每加一个模型就要改 sendMessage 的分支逻辑，违反 OCP。

### Q2：流式响应为什么要用回调而不是直接返回？

AI 生成文本是逐步的，全量返回需要等几十秒用户看到空白。回调机制让每个 token 生成后立即推送给前端，用户体验是"打字机效果"。SSE 协议天然适合这种服务端→客户端的单向流推送。

### Q3：为什么 SessionManager 要双缓存（内存+SQLite）？

- **内存**：快速查找，O(1) 访问会话数据
- **SQLite**：程序重启不丢失会话历史
- 启动时从数据库恢复到内存，运行时双写保证一致性

### Q4：dynamic_pointer_cast 和 static_cast 的区别？

| | static_cast | dynamic_cast |
|------|------|------|
| 检查时机 | 编译期 | 运行期（RTTI） |
| 安全性 | 不安全（不检查） | 安全（类型不匹配返回 nullptr） |
| 要求 | 无 | 基类必须有多态（至少一个虚函数） |
| 开销 | 零 | 略有（查虚表 + type_info） |

本项目用 `dynamic_pointer_cast` 判断 Config 子类类型，因为配置可能来自用户输入，必须运行期安全检查。

### Q5：httplib 的 content_receiver 是什么？

httplib 提供的回调机制，HTTP 响应体每收到一块数据就调一次。用于流式响应（SSE），每收到一个 `data:` 行就解析并回调上层。返回 `false` 可以终止接收。

### Q6：项目中有哪些 RAII 的应用？

- `unique_ptr<LLMProvider>`：Provider 生命周期自动管理
- `shared_ptr<Session>`：会话引用计数共享
- httplib::Client / Server：构造即连接，析构即释放
- SQLite 的 `sqlite3*` 虽然是 C API，但 DataManager 在析构函数中 `sqlite3_close`

### Q7：如果让你改进这个项目，你会怎么做？

1. **线程安全**：SessionManager 的 `_sessions` map 没有加锁，多线程并发访问会崩溃 → 加读写锁
2. **连接池**：每次请求都创建 httplib::Client，可以复用连接池
3. **错误重试**：AI API 可能超时/限流，应加指数退避重试
4. **配置热加载**：当前配置通过 gflags 启动时传入，应该支持运行时修改
5. **限流**：防止用户滥用 API Key，应加速率限制

---

## ✅ 掌握标准

- [x] 能画出三层架构图
- [x] 能解释 Provider 模式 + 门面模式 + 配置多态
- [x] 能走读 ChatGPTProvider 的全量和流式实现
- [x] 能解释 SSE 协议和 content_receiver 机制
- [x] 能回答 dynamic_pointer_cast 的原理和必要性
- [x] 能说出 5 个以上改进方向

---

> 资料来源：
> - 代码：`ai-model-access/AIModelAcessTech/`（34 个文件）
> - 课件：9 个 PDF（项目简介/AI科普/环境搭建/ChatSDK/聊天助手/扩展/SQLite/使用手册）
> - 板书：7 张 PNG 架构图
