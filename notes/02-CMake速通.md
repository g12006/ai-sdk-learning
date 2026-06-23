# CMake 速通：看懂 AI SDK 的 3 个 CMakeLists.txt

## sdk/CMakeLists.txt（编译静态库）

```cmake
cmake_minimum_required(VERSION 3.10)        # 最低 CMake 版本
set(SDK_NAME "ai_chat_sdk")                 # 定义变量
set(CMAKE_CXX_STANDARD 17)                  # C++17
set(CMAKE_CXX_STANDARD_REQUIRED ON)         # 编译器不支持就报错
set(CMAKE_BUILD_TYPE Debug)                 # Debug 模式

file(GLOB_RECURSE SDK_SOURCES "src/*.cpp")   # 通配符收集源文件
file(GLOB_RECURSE SDK_HEADERS "include/*.h")

add_library(${SDK_NAME} STATIC ${SDK_SOURCES} ${SDK_HEADERS})
#                ↑ 目标名   ↑ 静态库   ↑ 源文件        ↑ 头文件

target_compile_definitions(${SDK_NAME} PUBLIC CPPHTTPLIB_OPENSSL_SUPPORT)
target_include_directories(${SDK_NAME} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

find_package(OpenSSL REQUIRED)              # 查找 OpenSSL
target_link_libraries(${SDK_NAME} PUBLIC    # 链接依赖
    jsoncpp fmt spdlog sqlite3 OpenSSL::SSL OpenSSL::Crypto)

install(TARGETS ${SDK_NAME}                 # 安装到 /usr/local/lib
        ARCHIVE DESTINATION lib)
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/  # 安装头文件
        DESTINATION include/ai_chat_sdk
        FILES_MATCHING PATTERN "*.h")
```

## ChatServer/CMakeLists.txt（编译可执行文件）

```cmake
project(AIChatServer)                       # 项目名
add_executable(AIChatServer main.cpp ChatServer.cpp)
target_link_libraries(AIChatServer
    ai_chat_sdk          # 自己的 SDK（静态库）
    gflags               # 命令行参数
    jsoncpp fmt spdlog sqlite3 OpenSSL::SSL OpenSSL::Crypto)
```

## test/CMakeLists.txt（编译测试）

```cmake
project(testLLM)
add_executable(testLLM testLLM.cpp
    ../sdk/src/DeepSeekProvider.cpp        # 直接包含 SDK 源码
    ../sdk/src/ChatGPTProvider.cpp
    ../sdk/src/ChatSDK.cpp ...)            # 不用静态库，直接编译
target_link_libraries(testLLM jsoncpp fmt spdlog gtest OpenSSL::SSL OpenSSL::Crypto sqlite3)
```

## 核心命令速查

| 命令 | 作用 | 示例 |
|------|------|------|
| `cmake_minimum_required` | 版本检查 | `VERSION 3.10` |
| `project` | 项目名 + 编译器检测 | `project(AIChatServer)` |
| `set` | 变量赋值 | `set(VAR value)` |
| `add_executable` | 生成可执行文件 | `add_executable(app main.cpp)` |
| `add_library` | 生成库文件 | `add_library(lib STATIC ...)` |
| `target_include_directories` | 头文件搜索路径 | 支持 PUBLIC/PRIVATE |
| `target_link_libraries` | 链接依赖 | 支持 PUBLIC/PRIVATE |
| `target_compile_definitions` | 预处理器宏 | `-DCPPHTTPLIB_OPENSSL_SUPPORT` |
| `find_package` | 查找已安装库 | `find_package(OpenSSL REQUIRED)` |
| `file(GLOB_RECURSE)` | 通配符收集文件 | `file(GLOB_RECURSE SRC "src/*.cpp")` |
| `install` | 安装到系统目录 | `install(TARGETS ...)` |

## PUBLIC vs PRIVATE

```cmake
target_link_libraries(B PRIVATE A)   # B 能用 A，B 的消费者拿不到 A
target_link_libraries(C PUBLIC A)    # C 能用 A，C 的消费者也能用 A
```

```cmake
# sdk → PUBLIC jsoncpp → ChatServer 自动获得 jsoncpp
target_link_libraries(ai_chat_sdk PUBLIC jsoncpp)
```

## 三个文件的关系

```
sdk/CMakeLists.txt          → libai_chat_sdk.a（静态库）
ChatServer/CMakeLists.txt   → AIChatServer（可执行文件，链接 ai_chat_sdk）
test/CMakeLists.txt         → testLLM（可执行文件，直接编译 SDK 源码）
```

---

📅 最后更新：2026-06-23
