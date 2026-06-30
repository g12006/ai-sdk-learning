#include "../include/DataManager.h"
#include "../include/util/myLog.h"
#include <sqlite3.h>
#include <sstream>
#include <ctime>

namespace ai_chat_sdk {

// ----------------------------------------------------------------
// 构造 / 析构
// ----------------------------------------------------------------
DataManager::DataManager(const std::string& dbPath)
    : _dbPath(dbPath), _db(nullptr)
{}

DataManager::~DataManager()
{
    if (_db) {
        sqlite3_close(_db);
        _db = nullptr;
        INFO("Database connection closed: {}", _dbPath);
    }
}

// ----------------------------------------------------------------
// 初始化数据库（建表）
// ----------------------------------------------------------------
bool DataManager::initDataBase()
{
    int rc = sqlite3_open(_dbPath.c_str(), &_db);
    if (rc != SQLITE_OK) {
        ERR("Cannot open database [{}]: {}", _dbPath, sqlite3_errmsg(_db));
        return false;
    }
    INFO("Database opened: {}", _dbPath);

    // 开启外键约束（SQLite 默认关闭）
    execSQL("PRAGMA foreign_keys = ON;");

    // 创建 sessions 表
    const std::string createSessions = R"(
        CREATE TABLE IF NOT EXISTS sessions (
            session_id  TEXT PRIMARY KEY,
            model_name  TEXT NOT NULL,
            create_time INTEGER NOT NULL,
            update_time INTEGER NOT NULL
        );
    )";

    // 创建 messages 表（级联删除）
    const std::string createMessages = R"(
        CREATE TABLE IF NOT EXISTS messages (
            message_id TEXT PRIMARY KEY,
            session_id TEXT NOT NULL,
            role       TEXT NOT NULL,
            content    TEXT NOT NULL,
            timestamp  INTEGER NOT NULL,
            FOREIGN KEY(session_id) REFERENCES sessions(session_id) ON DELETE CASCADE
        );
    )";

    if (!execSQL(createSessions)) {
        ERR("Failed to create sessions table.");
        return false;
    }
    if (!execSQL(createMessages)) {
        ERR("Failed to create messages table.");
        return false;
    }

    INFO("Database initialized successfully.");
    return true;
}

// ----------------------------------------------------------------
// 执行无返回的 SQL（DDL / 简单 DML）
// ----------------------------------------------------------------
bool DataManager::execSQL(const std::string& sql)
{
    char* errMsg = nullptr;
    int rc = sqlite3_exec(_db, sql.c_str(), nullptr, nullptr, &errMsg);
    if (rc != SQLITE_OK) {
        ERR("SQL exec error: {}", errMsg ? errMsg : "unknown");
        sqlite3_free(errMsg);
        return false;
    }
    return true;
}

// ----------------------------------------------------------------
// 插入会话
// ----------------------------------------------------------------
bool DataManager::insertSession(const Session& session)
{
    const char* sql = "INSERT INTO sessions (session_id, model_name, create_time, update_time) "
                      "VALUES (?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ERR("insertSession prepare failed: {}", sqlite3_errmsg(_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, session._sessionId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, session._modelName.c_str(),  -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, static_cast<sqlite3_int64>(session._createdAt));
    sqlite3_bind_int64(stmt, 4, static_cast<sqlite3_int64>(session._updatedAt));

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) ERR("insertSession step failed: {}", sqlite3_errmsg(_db));
    sqlite3_finalize(stmt);
    return ok;
}

// ----------------------------------------------------------------
// 删除会话（级联删除消息）
// ----------------------------------------------------------------
bool DataManager::deleteSession(const std::string& sessionId)
{
    const char* sql = "DELETE FROM sessions WHERE session_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ERR("deleteSession prepare failed: {}", sqlite3_errmsg(_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) ERR("deleteSession step failed: {}", sqlite3_errmsg(_db));
    sqlite3_finalize(stmt);
    return ok;
}

// ----------------------------------------------------------------
// 获取所有会话元数据（不含消息列表，用于启动恢复）
// ----------------------------------------------------------------
std::vector<Session> DataManager::getAllSessions()
{
    std::vector<Session> sessions;
    const char* sql = "SELECT session_id, model_name, create_time, update_time FROM sessions;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ERR("getAllSessions prepare failed: {}", sqlite3_errmsg(_db));
        return sessions;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Session s;
        s._sessionId  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        s._modelName  = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        s._createdAt  = static_cast<std::time_t>(sqlite3_column_int64(stmt, 2));
        s._updatedAt  = static_cast<std::time_t>(sqlite3_column_int64(stmt, 3));
        sessions.push_back(std::move(s));
    }
    sqlite3_finalize(stmt);
    return sessions;
}

// ----------------------------------------------------------------
// 获取指定会话完整数据（含消息列表）
// ----------------------------------------------------------------
std::shared_ptr<Session> DataManager::getSession(const std::string& sessionId)
{
    // 1. 查询会话元数据
    const char* sqlSession = "SELECT session_id, model_name, create_time, update_time "
                             "FROM sessions WHERE session_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_db, sqlSession, -1, &stmt, nullptr) != SQLITE_OK) {
        ERR("getSession prepare failed: {}", sqlite3_errmsg(_db));
        return nullptr;
    }
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);

    if (sqlite3_step(stmt) != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return nullptr;
    }

    auto session = std::make_shared<Session>();
    session->_sessionId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    session->_modelName = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
    session->_createdAt = static_cast<std::time_t>(sqlite3_column_int64(stmt, 2));
    session->_updatedAt = static_cast<std::time_t>(sqlite3_column_int64(stmt, 3));
    sqlite3_finalize(stmt);

    // 2. 加载消息列表
    session->_messages = getMessages(sessionId);
    return session;
}

// ----------------------------------------------------------------
// 更新会话的 update_time
// ----------------------------------------------------------------
bool DataManager::updateSessionTime(const std::string& sessionId, std::time_t updateTime)
{
    const char* sql = "UPDATE sessions SET update_time = ? WHERE session_id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ERR("updateSessionTime prepare failed: {}", sqlite3_errmsg(_db));
        return false;
    }
    sqlite3_bind_int64(stmt, 1, static_cast<sqlite3_int64>(updateTime));
    sqlite3_bind_text(stmt, 2, sessionId.c_str(), -1, SQLITE_STATIC);

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) ERR("updateSessionTime step failed: {}", sqlite3_errmsg(_db));
    sqlite3_finalize(stmt);
    return ok;
}

// ----------------------------------------------------------------
// 插入消息
// ----------------------------------------------------------------
bool DataManager::insertMessage(const std::string& sessionId, const Message& message)
{
    const char* sql = "INSERT INTO messages (message_id, session_id, role, content, timestamp) "
                      "VALUES (?, ?, ?, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ERR("insertMessage prepare failed: {}", sqlite3_errmsg(_db));
        return false;
    }

    sqlite3_bind_text(stmt, 1, message._messageId.c_str(), -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, sessionId.c_str(),           -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, message._role.c_str(),       -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, message._content.c_str(),    -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 5, static_cast<sqlite3_int64>(message._timestamp));

    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    if (!ok) ERR("insertMessage step failed: {}", sqlite3_errmsg(_db));
    sqlite3_finalize(stmt);
    return ok;
}

// ----------------------------------------------------------------
// 获取指定会话的所有消息（按 timestamp 升序）
// ----------------------------------------------------------------
std::vector<Message> DataManager::getMessages(const std::string& sessionId)
{
    std::vector<Message> messages;
    const char* sql = "SELECT message_id, role, content, timestamp "
                      "FROM messages WHERE session_id = ? ORDER BY timestamp ASC;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        ERR("getMessages prepare failed: {}", sqlite3_errmsg(_db));
        return messages;
    }
    sqlite3_bind_text(stmt, 1, sessionId.c_str(), -1, SQLITE_STATIC);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        Message msg;
        msg._messageId = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
        msg._role      = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
        msg._content   = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
        msg._timestamp = static_cast<std::time_t>(sqlite3_column_int64(stmt, 3));
        messages.push_back(std::move(msg));
    }
    sqlite3_finalize(stmt);
    return messages;
}

} // end ai_chat_sdk
