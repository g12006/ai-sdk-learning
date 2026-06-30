#pragma once
#include <string>
#include <memory>
#include <mutex>
#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace bite {

/**
 * @brief Logger - 基于 spdlog 的线程安全日志工具（单例）
 *
 * 特性：
 *  - 单例模式：通过私有构造函数 + deleted 拷贝/赋值保证唯一实例
 *  - 双重检查锁定（DCLP）保护并发初始化
 *  - 异步日志：线程池队列 32768，1 个后台线程，降低主线程 I/O 开销
 *  - 两种 sink：
 *      * loggerFile == "stdout" → stdout_color_mt（彩色终端输出）
 *      * 其他值               → basic_logger_mt  （异步文件输出）
 *  - 日志格式：[HH:MM:SS][loggerName][LEVEL][   File:Line] Message
 *
 * 使用方式：
 *  @code
 *  // 1. 初始化（通常在 main.cpp 中调用一次）
 *  bite::Logger::initLogger("aiChatServer", "chat.log", spdlog::level::debug);
 *
 *  // 2. 在任意代码位置使用宏（自动注入文件名和行号）
 *  INFO("Server started on port {}", 8080);
 *  DBG("Request from {}", clientIp);
 *  WARN("High latency: {}ms", latency);
 *  ERR("DB connection failed: {}", errMsg);
 *  @endcode
 */
class Logger {
public:
    /**
     * @brief 初始化全局日志器
     * @param loggerName  日志器名称（显示在每条日志的 [name] 字段）
     * @param loggerFile  日志输出目标：传 "stdout" 输出到终端，否则视为文件路径
     * @param logLevel    最低输出级别，低于此级别的日志会被丢弃
     *
     * 线程安全：通过 std::mutex + double-checked locking 保证只初始化一次。
     */
    static void initLogger(const std::string& loggerName,
                           const std::string& loggerFile,
                           spdlog::level::level_enum logLevel);

    /**
     * @brief 获取全局 spdlog::logger 实例
     * @return 指向已初始化 logger 的 shared_ptr
     *
     * 若未调用 initLogger 直接使用，将使用 spdlog 默认 logger（仅输出到 stderr）。
     */
    static std::shared_ptr<spdlog::logger> getLogger();

private:
    Logger() = default;
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    static std::shared_ptr<spdlog::logger> _logger; ///< 全局 logger 实例
    static std::mutex                      _mutex;  ///< 保护初始化的互斥锁
};

} // end bite

// ----------------------------------------------------------------
// 日志宏定义
// 格式：[右对齐10字符文件名:左对齐4字符行号] <message>
// ----------------------------------------------------------------

/** @brief TRACE 级别：最细粒度的追踪信息 */
#define TRACE(fmt, ...) \
    bite::Logger::getLogger()->trace("[{:>10s}:{:<4d}] " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

/** @brief DBG 级别：调试信息 */
#define DBG(fmt, ...) \
    bite::Logger::getLogger()->debug("[{:>10s}:{:<4d}] " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

/** @brief INFO 级别：常规运行信息 */
#define INFO(fmt, ...) \
    bite::Logger::getLogger()->info("[{:>10s}:{:<4d}] " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

/** @brief WARN 级别：潜在问题警告 */
#define WARN(fmt, ...) \
    bite::Logger::getLogger()->warn("[{:>10s}:{:<4d}] " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

/** @brief ERR 级别：错误，功能受损 */
#define ERR(fmt, ...) \
    bite::Logger::getLogger()->error("[{:>10s}:{:<4d}] " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)

/** @brief CRIT 级别：严重错误，程序可能无法继续运行 */
#define CRIT(fmt, ...) \
    bite::Logger::getLogger()->critical("[{:>10s}:{:<4d}] " fmt, __FILE_NAME__, __LINE__, ##__VA_ARGS__)
