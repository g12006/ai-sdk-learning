#include "../../include/util/ConfigLoader.h"
#include "../../include/util/myLog.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <cctype>

namespace ai_chat_sdk {

// 去除字符串首尾空白
std::string ConfigLoader::trim(const std::string& s){
    size_t start = 0;
    while(start < s.size() && std::isspace(static_cast<unsigned char>(s[start]))){
        ++start;
    }
    size_t end = s.size();
    while(end > start && std::isspace(static_cast<unsigned char>(s[end - 1]))){
        --end;
    }
    return s.substr(start, end - start);
}

// 去除值两侧的引号（支持单引号与双引号）
std::string ConfigLoader::stripQuotes(const std::string& s){
    if(s.size() >= 2){
        if((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')){
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

// 加载 .env 文件：KEY=VALUE 行，# 开头注释，空行跳过，支持引号包裹
bool ConfigLoader::loadEnvFile(const std::string& path){
    std::ifstream ifs(path);
    if(!ifs.is_open()){
        ERR("ConfigLoader::loadEnvFile open failed: {}", path);
        return false;
    }
    std::string line;
    int count = 0;
    while(std::getline(ifs, line)){
        std::string t = trim(line);
        if(t.empty() || t[0] == '#'){
            continue;  // 空行或注释
        }
        size_t pos = t.find('=');
        if(pos == std::string::npos){
            continue;  // 非法行，跳过
        }
        std::string key = trim(t.substr(0, pos));
        std::string value = stripQuotes(trim(t.substr(pos + 1)));
        if(key.empty()){
            continue;
        }
        _values[key] = value;
        ++count;
    }
    INFO("ConfigLoader::loadEnvFile loaded {} entries from {}", count, path);
    return true;
}

// 加载简单 YAML 风格配置：key: value 行，# 注释，支持缩进嵌套（点号连接）
bool ConfigLoader::loadYamlFile(const std::string& path){
    std::ifstream ifs(path);
    if(!ifs.is_open()){
        ERR("ConfigLoader::loadYamlFile open failed: {}", path);
        return false;
    }
    // 路径栈：记录 (缩进层数, 键) 用于嵌套拼接
    std::vector<std::pair<int, std::string>> pathStack;
    std::string line;
    int count = 0;
    while(std::getline(ifs, line)){
        // 计算行首缩进空格数
        int indent = 0;
        while(indent < static_cast<int>(line.size()) && line[indent] == ' '){
            ++indent;
        }
        std::string t = trim(line);
        if(t.empty() || t[0] == '#'){
            continue;  // 空行或注释
        }
        // 去除行内注释（# 之前的内容视为有效，前提是 # 前有空白，避免误伤值中的 #）
        {
            size_t hashPos = t.find(" #");
            if(hashPos != std::string::npos){
                t = trim(t.substr(0, hashPos));
            }
        }

        size_t colon = t.find(':');
        if(colon == std::string::npos){
            continue;  // 非法行，跳过
        }
        std::string key = trim(t.substr(0, colon));
        std::string value = stripQuotes(trim(t.substr(colon + 1)));
        if(key.empty()){
            continue;
        }

        // 弹出栈顶缩进 >= 当前的项，保证父子层级正确
        while(!pathStack.empty() && pathStack.back().first >= indent){
            pathStack.pop_back();
        }

        if(value.empty()){
            // 无值：作为父节点压栈，供后续子行拼接
            pathStack.push_back({indent, key});
        }else{
            // 有值：拼接完整 key（父路径 + 当前 key）
            std::string fullKey;
            for(const auto& p : pathStack){
                fullKey += p.second + ".";
            }
            fullKey += key;
            _values[fullKey] = value;
            ++count;
        }
    }
    INFO("ConfigLoader::loadYamlFile loaded {} entries from {}", count, path);
    return true;
}

// 从环境变量加载：按 keys 列表逐个读取 std::getenv
void ConfigLoader::loadFromEnv(const std::vector<std::string>& keys){
    int count = 0;
    for(const auto& key : keys){
        const char* val = std::getenv(key.c_str());
        if(val != nullptr){
            _values[key] = std::string(val);
            ++count;
        }
    }
    INFO("ConfigLoader::loadFromEnv loaded {} entries from environment", count);
}

// 从命令行参数加载：支持 --key=value 与 --key value 两种形式
void ConfigLoader::loadFromArgs(int argc, char** argv){
    int count = 0;
    for(int i = 1; i < argc; ++i){
        std::string arg(argv[i]);
        if(arg.size() > 2 && arg[0] == '-' && arg[1] == '-'){
            std::string body = arg.substr(2);
            size_t eq = body.find('=');
            if(eq != std::string::npos){
                // --key=value 形式
                std::string key = trim(body.substr(0, eq));
                std::string value = stripQuotes(trim(body.substr(eq + 1)));
                if(!key.empty()){
                    _values[key] = value;
                    ++count;
                }
            }else{
                // --key value 形式（取下一个参数作为值）
                std::string key = trim(body);
                if(!key.empty() && i + 1 < argc){
                    std::string value = stripQuotes(trim(std::string(argv[i + 1])));
                    _values[key] = value;
                    ++count;
                    ++i;  // 跳过已消费的值参数
                }
            }
        }
    }
    INFO("ConfigLoader::loadFromArgs loaded {} entries from argv", count);
}

std::string ConfigLoader::getString(const std::string& key, const std::string& defaultValue) const{
    auto it = _values.find(key);
    if(it != _values.end()){
        return it->second;
    }
    return defaultValue;
}

int ConfigLoader::getInt(const std::string& key, int defaultValue) const{
    auto it = _values.find(key);
    if(it == _values.end()){
        return defaultValue;
    }
    try{
        return std::stoi(it->second);
    }catch(...){
        return defaultValue;  // 解析失败回退默认值
    }
}

double ConfigLoader::getDouble(const std::string& key, double defaultValue) const{
    auto it = _values.find(key);
    if(it == _values.end()){
        return defaultValue;
    }
    try{
        return std::stod(it->second);
    }catch(...){
        return defaultValue;  // 解析失败回退默认值
    }
}

bool ConfigLoader::getBool(const std::string& key, bool defaultValue) const{
    auto it = _values.find(key);
    if(it == _values.end()){
        return defaultValue;
    }
    std::string v = it->second;
    // 转小写比较
    std::transform(v.begin(), v.end(), v.begin(),
                   [](unsigned char c){ return std::tolower(c); });
    if(v == "true" || v == "1" || v == "yes" || v == "on"){
        return true;
    }
    if(v == "false" || v == "0" || v == "no" || v == "off"){
        return false;
    }
    return defaultValue;
}

bool ConfigLoader::hasKey(const std::string& key) const{
    return _values.find(key) != _values.end();
}

} // namespace ai_chat_sdk
