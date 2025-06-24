cmake_minimum_required(VERSION 3.16)
project(MyRouterMonitor)

# 开启 compile_commands.json 的生成（clangd 需要）
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# 指定 C++ 标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address -g -O1")
set(CMAKE_BUILD_TYPE Debug)

add_executable(MyRouterMonitor main.c)