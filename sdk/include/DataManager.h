#pragma once
#include <string>
#include <vector>
#include <memory>
#include "common.h"

// 前向声明 sqlite3 结构体，避免直接引入 sqlite3.h
struct sqlite3;

namespace ai_chat_sdk {

/**
 * @brief DataManager - SQLite3 持久化层
 *
 * 封装所有数据库操作，为上层的 SessionManager 提供 CRUD 接口：
 *  - 数据库文件默认为 chatDB.db
 *  - 使用 prepared statement 防止 SQL 注入
 *  - messages 表通过 ON DELETE CASCADE 联动删除
 *
 * 数据库 Schema：
 *
 *   sessions(
 *       session_id  TEXT PRIMARY KEY,
 *       model_name  TEXT NOT NULL,
 *       create_time INTEGER,   -- Unix timestamp
 *       update_time INTEGER    -- Unix timestamp
 *   )
 *
 *   messages(
 *       message_id  TEXT PRIMARY KEY,
 *       session_id  TEXT NOT NULL,
 *       role        TEXT NOT NULL,
 *       content     TEXT NOT NULL,
 *       timestamp   INTEGER,   -- Unix timestamp
 *       FOREIGN KEY(session_id) REFERENCES sessions(session_id) ON DELETE CASCADE
 *   )
 */
class DataManager {
public:
    /**
     * @brief 构造函数
     * @param dbPath SQLite 数据库文件路径，默认 "chatDB.db"
     */
    explicit DataManager(const std::string& dbPath = "chatDB.db");

    /** @brief 析构函数，关闭数据库连接 */
    ~DataManager();

    // ----------------------------------------------------------------
    // 数据库初始化
    // ----------------------------------------------------------------

    /**
     * @brief 初始化数据库
     * @return 成功返回 true
     *
     * 打开数据库连接，若 sessions / messages 表不存在则自动创建。
     * 必须在调用任何其他方法前调用。
     */
    bool initDataBase();

    // ----------------------------------------------------------------
    // 会话操作
    // ----------------------------------------------------------------

    /**
     * @brief 插入一条新会话记录
     * @param session 待插入的 Session 对象（_sessionId、_modelName、_createdAt 已填充）
     * @return 插入成功返回 true
     */
    bool insertSession(const Session& session);

    /**
     * @brief 删除指定会话（级联删除其所有消息）
     * @param sessionId 目标会话 ID
     * @return 删除成功返回 true
     */
    bool deleteSession(const std::string& sessionId);

    /**
     * @brief 获取所有会话元数据（不含消息列表）
     * @return Session 对象列表，_messages 为空
     *
     * 供 SessionManager 构造时批量加载，实现重启恢复。
     */
    std::vector<Session> getAllSessions();

    /**
     * @brief 获取指定会话的完整数据（含消息列表）
     * @param sessionId 目标会话 ID
     * @return 指向 Session 的 shared_ptr；不存在返回 nullptr
     */
    std::shared_ptr<Session> getSession(const std::string& sessionId);

    /**
     * @brief 更新会话的 update_time 字段
     * @param sessionId  目标会话 ID
     * @param updateTime 新的更新时间戳
     * @return 更新成功返回 true
     */
    bool updateSessionTime(const std::string& sessionId, std::time_t updateTime);

    // ----------------------------------------------------------------
    // 消息操作
    // ----------------------------------------------------------------

    /**
     * @brief 插入一条消息
     * @param sessionId 所属会话 ID
     * @param message   待插入的 Message 对象
     * @return 插入成功返回 true
     */
    bool insertMessage(const std::string& sessionId, const Message& message);

    /**
     * @brief 获取指定会话的所有消息（按时间戳升序）
     * @param sessionId 目标会话 ID
     * @return Message 列表
     */
    std::vector<Message> getMessages(const std::string& sessionId);

private:
    /**
     * @brief 执行无返回结果的 SQL 语句（DDL / DML）
     * @param sql 待执行的 SQL 字符串
     * @return 成功返回 true，失败时记录错误日志
     */
    bool execSQL(const std::string& sql);

private:
    std::string _dbPath;    ///< 数据库文件路径
    sqlite3*    _db = nullptr; ///< sqlite3 连接句柄
};

} // end ai_chat_sdk
