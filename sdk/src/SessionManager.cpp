#include "../include/SessionManager.h"
#include "../include/DataManager.h"
#include "../include/util/myLog.h"
#include <sstream>
#include <ctime>

namespace ai_chat_sdk {

// ----------------------------------------------------------------
// 构造函数：从数据库恢复所有历史会话
// ----------------------------------------------------------------
SessionManager::SessionManager(DataManager& dataManager)
    : _dataManager(dataManager)
{
    // 启动时批量加载历史会话元数据（不含消息，节省内存）
    std::vector<Session> sessions = _dataManager.getAllSessions();
    for (auto& session : sessions) {
        _sessions[session._sessionId] = std::make_shared<Session>(std::move(session));
    }
    INFO("SessionManager initialized, loaded {} session(s) from database.", _sessions.size());
}

// ----------------------------------------------------------------
// 生成唯一 session ID：session_<timestamp>_<counter>
// ----------------------------------------------------------------
std::string SessionManager::generateSessionId()
{
    std::ostringstream oss;
    oss << "session_" << std::time(nullptr) << "_" << (++_sessionCounter);
    return oss.str();
}

// ----------------------------------------------------------------
// 生成唯一 message ID：msg_<timestamp>_<counter>
// ----------------------------------------------------------------
std::string SessionManager::generateMessageId()
{
    std::ostringstream oss;
    oss << "msg_" << std::time(nullptr) << "_" << (++_messageCounter);
    return oss.str();
}

// ----------------------------------------------------------------
// 创建新会话
// ----------------------------------------------------------------
std::string SessionManager::createSession(const std::string& modelName)
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::string sessionId = generateSessionId();

    // 构建 Session 对象
    auto session = std::make_shared<Session>(modelName);
    session->_sessionId = sessionId;
    session->_createdAt = std::time(nullptr);
    session->_updatedAt = session->_createdAt;

    // 持久化到数据库
    if (!_dataManager.insertSession(*session)) {
        ERR("Failed to insert session into database, sessionId = {}", sessionId);
        return "";
    }

    // 写入内存缓存
    _sessions[sessionId] = session;

    INFO("Created new session: sessionId = {}, model = {}", sessionId, modelName);
    return sessionId;
}

// ----------------------------------------------------------------
// 获取会话（优先内存缓存，缓存未命中时从数据库加载）
// ----------------------------------------------------------------
std::shared_ptr<Session> SessionManager::getSession(const std::string& sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);

    // 1. 先查内存缓存
    auto it = _sessions.find(sessionId);
    if (it != _sessions.end()) {
        return it->second;
    }

    // 2. 缓存未命中：从数据库加载完整会话（含消息列表）
    auto session = _dataManager.getSession(sessionId);
    if (!session) {
        WARN("Session not found: sessionId = {}", sessionId);
        return nullptr;
    }

    // 3. 写入内存缓存（懒加载）
    _sessions[sessionId] = session;
    DBG("Loaded session from database: sessionId = {}", sessionId);
    return session;
}

// ----------------------------------------------------------------
// 删除会话
// ----------------------------------------------------------------
bool SessionManager::deleteSession(const std::string& sessionId)
{
    std::lock_guard<std::mutex> lock(_mutex);

    // 先从数据库删除（级联删除消息）
    if (!_dataManager.deleteSession(sessionId)) {
        ERR("Failed to delete session from database, sessionId = {}", sessionId);
        return false;
    }

    // 再从内存缓存移除
    _sessions.erase(sessionId);

    INFO("Deleted session: sessionId = {}", sessionId);
    return true;
}

// ----------------------------------------------------------------
// 获取所有会话 ID
// ----------------------------------------------------------------
std::vector<std::string> SessionManager::getSessionLists() const
{
    std::lock_guard<std::mutex> lock(_mutex);

    std::vector<std::string> ids;
    ids.reserve(_sessions.size());
    for (const auto& pair : _sessions) {
        ids.push_back(pair.first);
    }
    return ids;
}

// ----------------------------------------------------------------
// 向指定会话追加消息（双写：内存 + 数据库）
// ----------------------------------------------------------------
bool SessionManager::addMessage(const std::string& sessionId,
                                const std::string& role,
                                const std::string& content)
{
    std::lock_guard<std::mutex> lock(_mutex);

    // 1. 查找会话（内存缓存）
    auto it = _sessions.find(sessionId);
    if (it == _sessions.end()) {
        ERR("addMessage failed: session not found, sessionId = {}", sessionId);
        return false;
    }

    // 2. 构建消息对象
    Message msg(role, content);
    msg._messageId = generateMessageId();
    msg._timestamp = std::time(nullptr);

    // 3. 写入数据库
    if (!_dataManager.insertMessage(sessionId, msg)) {
        ERR("Failed to persist message, sessionId = {}, role = {}", sessionId, role);
        return false;
    }

    // 4. 更新内存中的会话
    auto session = it->second;
    session->_messages.push_back(msg);
    session->_updatedAt = msg._timestamp;
    _dataManager.updateSessionTime(sessionId, session->_updatedAt);

    DBG("Added message to session: sessionId = {}, role = {}, msgId = {}",
        sessionId, role, msg._messageId);
    return true;
}

} // end ai_chat_sdk
