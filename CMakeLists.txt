# AI Chat SDK 顶层 CMakeLists.txt
# 构建方式：
#   mkdir build && cd build
#   cmake .. -DBUILD_TESTS=ON -DBUILD_CHAT_SERVER=ON   # 构建测试 + ChatServer
#   cmake ..                                             # 仅构建 SDK 库

cmake_minimum_required(VERSION 3.10)
project(ai-chat-sdk VERSION 1.0.0 LANGUAGES CXX)

# C++17 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Debug 模式
set(CMAKE_BUILD_TYPE Debug)

# 添加子目录
add_subdirectory(sdk)

option(BUILD_CHAT_SERVER "构建 ChatServer 可执行文件" OFF)
if(BUILD_CHAT_SERVER)
    add_subdirectory(ChatServer)
endif()

option(BUILD_TESTS "构建测试项目" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()
