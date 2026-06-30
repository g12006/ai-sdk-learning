#pragma once
#include <unordered_map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>
#include "common.h"

namespace ai_chat_sdk {

class DataManager;

/**
 * @brief SessionManager - 会话管理器
 *
 * 负责管理所有活跃会话的生命周期：
 *  - 内存缓存：以 unordered_map 维护会话对象，保证 O(1) 查找
 *  - 持久化协调：调用 DataManager 将会话/消息同步写入 SQLite 数据库
 *  - 线程安全：通过 std::mutex 保护共享状态
 *  - 启动恢复：构造时从数据库加载全部历史会话
 */
class SessionManager {
public:
    /**
     * @brief 构造函数
     * @param dataManager 指向 DataManager 的引用，负责持久化
     *
     * 构造时自动调用 DataManager::getAllSessions() 填充内存缓存，
     * 确保服务重启后能无缝恢复历史会话。
     */
    explicit SessionManager(DataManager& dataManager);

    /**
     * @brief 创建新会话
     * @param modelName 该会话绑定的模型名称
     * @return 新会话的唯一 session_id，格式：session_<timestamp>_<counter>
     *
     * 同时在内存缓存和数据库中创建会话记录。
     */
    std::string createSession(const std::string& modelName);

    /**
     * @brief 获取指定会话
     * @param sessionId 会话 ID
     * @return 指向 Session 的指针；不存在则返回 nullptr
     *
     * 先查内存缓存，未命中再从数据库加载并写入缓存（懒加载）。
     */
    std::shared_ptr<Session> getSession(const std::string& sessionId);

    /**
     * @brief 删除指定会话
     * @param sessionId 要删除的会话 ID
     * @return 删除成功返回 true
     *
     * 同步删除内存缓存和数据库记录（级联删除消息）。
     */
    bool deleteSession(const std::string& sessionId);

    /**
     * @brief 获取所有会话 ID 列表
     * @return 当前所有 session_id 的 vector
     */
    std::vector<std::string> getSessionLists() const;

    /**
     * @brief 向指定会话追加一条消息
     * @param sessionId 目标会话 ID
     * @param role      消息角色（"user" / "assistant" / "system"）
     * @param content   消息正文
     * @return 追加成功返回 true
     *
     * 同时更新内存中的 Session::_messages 并持久化到数据库。
     * 消息 ID 格式：msg_<timestamp>_<counter>
     */
    bool addMessage(const std::string& sessionId,
                    const std::string& role,
                    const std::string& content);

private:
    /**
     * @brief 生成全局唯一的会话 ID
     * @return 格式：session_<unix_timestamp>_<auto_increment_counter>
     */
    std::string generateSessionId();

    /**
     * @brief 生成全局唯一的消息 ID
     * @return 格式：msg_<unix_timestamp>_<auto_increment_counter>
     */
    std::string generateMessageId();

private:
    DataManager&                                              _dataManager;   ///< 持久化层引用
    std::unordered_map<std::string, std::shared_ptr<Session>> _sessions;      ///< 内存缓存
    mutable std::mutex                                        _mutex;         ///< 保护 _sessions 的互斥锁
    int                                                       _sessionCounter = 0; ///< 会话 ID 自增计数器
    int                                                       _messageCounter = 0; ///< 消息 ID 自增计数器
};

} // end ai_chat_sdk
