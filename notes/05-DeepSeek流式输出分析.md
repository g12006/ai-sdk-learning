# DeepSeek 流式输出源码分析

## 函数签名

```cpp
std::string DeepSeekProvider::sendMessageStream(
    const std::vector<Message>& messages,                    // 历史消息
    const std::map<std::string, std::string>& requestParam,  // temperature, max_tokens
    std::function<void(const std::string&, bool)> callback   // 每收到 delta 就回调
)
```

- `callback(delta, false)`：收到增量文本
- `callback("", true)`：流结束

---

## 完整流程（6 步）

### 第一步：构造请求体

```cpp
requestBody["model"] = "deepseek-chat";
requestBody["messages"] = messageArray;
requestBody["stream"] = true;  // ← 关键！和 sendMessage 的唯一区别
```

### 第二步：创建 HTTP 客户端

```cpp
httplib::Client client("https://api.deepseek.com");
client.set_read_timeout(300, 0);  // 5 分钟（AI 思考需要时间）
// ❌ 不需要代理（和 ChatGPT 不同！）
```

### 第三步：response_handler — 检查 HTTP 状态

```cpp
req.response_handler = [&](const httplib::Response& res) {
    if (res.status != 200) {
        gotError = true;
        return false;  // 停止接收
    }
    return true;
};
```

### 第四步（核心）：content_receiver — 流式解析

```cpp
std::string buffer;  // 缓冲区：解决 TCP 分包

req.content_receiver = [&](const char* data, size_t len, ...) {
    if (gotError) return false;
    buffer.append(data, len);

    // 循环查找 \n\n 分隔符
    size_t pos = 0;
    while ((pos = buffer.find("\n\n")) != std::string::npos) {
        std::string chunk = buffer.substr(0, pos);
        buffer.erase(0, pos + 2);

        // 跳过空行和注释（: 开头）
        if (chunk.empty() || chunk[0] == ':') continue;

        // 提取 data: 行
        if (chunk.compare(0, 6, "data: ") == 0) {
            std::string modelData = chunk.substr(6);

            // 检测结束标记
            if (modelData == "[DONE]") {
                callback("", true);
                streamFinish = true;
                return true;
            }

            // JSON 解析
            Json::Value json;
            Json::parseFromStream(reader, stream, &json, &errors);
            
            // 提取增量文本
            std::string content = json["choices"][0]["delta"]["content"].asString();
            fullResponse += content;
            callback(content, false);  // 推给上层
        }
    }
    return true;
};
```

### 第五步：发送请求

```cpp
auto result = client.send(req);  // 阻塞直到流结束
```

### 第六步：事后检查

```cpp
if (!streamFinish) {
    WARN("stream ended without [DONE] marker");
    callback("", true);  // 手动通知结束
}
return fullResponse;  // 返回完整回复
```

---

## DeepSeek SSE 格式

```
data: {"choices":[{"delta":{"content":"你"},"index":0}]}\n
\n
data: {"choices":[{"delta":{"content":"好"},"index":0}]}\n
\n
data: [DONE]\n
\n
```

**特点**：只有 `data:` 行，没有 `event:` 行（比 ChatGPT 简单）。

## 和 ChatGPT 流式的对比

| 维度 | ChatGPT | DeepSeek |
|------|---------|----------|
| 代理 | ✅ 需要 | ❌ 不需要 |
| 超时 | 60s | 300s |
| API 路径 | `/v1/responses` | `/v1/chat/completions` |
| SSE 事件类型 | `event:` + `data:` | 只有 `data:` |
| 结束标记 | `response.completed` 事件 | `data: [DONE]` |
| JSON 路径（流式） | `delta.isString()` | `choices[0].delta.content` |
| JSON 路径（全量） | `output[0].content[0].text` | `choices[0].message.content` |

## TCP 分包处理

```
TCP 第1包: "data: {\"choices\":[{\"delta\":{\"content\":\"你"
TCP 第2包: "好\"},\"index\":0}]}\n\n"
```

第 1 包到达时 `find("\n\n")` 找不到 → 留在 buffer。
第 2 包到达后 buffer 完整 → `find("\n\n")` 找到 → 解析 emoji → 成功。

---

📅 最后更新：2026-06-23
