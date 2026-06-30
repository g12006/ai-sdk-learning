#include "../include/util/myLog.h"

namespace bite {

// 静态成员初始化
std::shared_ptr<spdlog::logger> Logger::_logger = nullptr;
std::mutex                      Logger::_mutex;

// ----------------------------------------------------------------
// 初始化全局日志器
// ----------------------------------------------------------------
void Logger::initLogger(const std::string& loggerName,
                        const std::string& loggerFile,
                        spdlog::level::level_enum logLevel)
{
    // 双重检查锁定，保证多线程环境下只初始化一次
    if (_logger) return;

    std::lock_guard<std::mutex> lock(_mutex);
    if (_logger) return;

    // 1. 配置全局刷新策略：达到指定级别时立即刷盘
    spdlog::flush_on(logLevel);

    // 2. 初始化线程池（队列容量 32768，1 个后台 I/O 线程）
    spdlog::init_thread_pool(32768, 1);

    // 3. 根据 loggerFile 选择 sink 类型
    if (loggerFile == "stdout") {
        // 彩色终端输出（同步，色彩区分级别）
        _logger = spdlog::stdout_color_mt(loggerName);
    } else {
        // 异步文件输出（线程池 + basic_file_sink）
        _logger = spdlog::basic_logger_mt<spdlog::async_factory>(loggerName, loggerFile);
    }

    // 4. 设置输出格式：[时间][名称][级别] 消息
    _logger->set_pattern("[%H:%M:%S][%n][%-7l]%v");

    // 5. 设置最低输出级别
    _logger->set_level(logLevel);

    _logger->info("[{:>10s}:{:<4d}] Logger initialized: name={}, output={}, level={}",
                  "myLog.cpp", __LINE__, loggerName, loggerFile,
                  spdlog::level::to_string_view(logLevel));
}

// ----------------------------------------------------------------
// 获取 logger 实例
// ----------------------------------------------------------------
std::shared_ptr<spdlog::logger> Logger::getLogger()
{
    if (!_logger) {
        // 未初始化时回退到 spdlog 默认 logger（输出到 stderr）
        return spdlog::default_logger();
    }
    return _logger;
}

} // end bite
