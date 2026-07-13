#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace ai_chat_sdk {

// 配置加载器
// 支持多来源加载，优先级（高 -> 低）：
//   命令行参数 > 环境变量 > .env 文件 > config.yaml 文件 > 代码默认值
// 实现方式：按上述顺序依次调用各 load 方法，后加载的会覆盖先前的。
class ConfigLoader {
public:
    ConfigLoader() = default;

    // 加载 .env 文件（KEY=VALUE 行，# 开头注释，支持引号包裹）
    bool loadEnvFile(const std::string& path);

    // 加载简单 YAML 风格 config（key: value 行，# 注释，支持嵌套用点号 . 表示，如 server.port）
    bool loadYamlFile(const std::string& path);

    // 从环境变量加载（按 keys 列表拷贝到内部存储）
    void loadFromEnv(const std::vector<std::string>& keys);

    // 从命令行参数加载（期望形如 --key=value 或 --key value 的 argv）
    void loadFromArgs(int argc, char** argv);

    // 读取（按内部 map 返回；调用方应按上述 load 顺序：先 loadYamlFile，再 loadEnvFile，
    // 再 loadFromEnv，再 loadFromArgs；后加载的会覆盖先前的——这就是优先级实现）
    std::string getString(const std::string& key, const std::string& defaultValue = "") const;
    int getInt(const std::string& key, int defaultValue = 0) const;
    double getDouble(const std::string& key, double defaultValue = 0.0) const;
    bool getBool(const std::string& key, bool defaultValue = false) const;
    bool hasKey(const std::string& key) const;

    // 调试用：返回内部所有键值对
    const std::unordered_map<std::string, std::string>& all() const { return _values; }

private:
    std::unordered_map<std::string, std::string> _values;

    static std::string trim(const std::string& s);
    static std::string stripQuotes(const std::string& s);
};

} // namespace ai_chat_sdk
