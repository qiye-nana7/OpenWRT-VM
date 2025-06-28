cmake_minimum_required(VERSION 3.10)
project(TrafficMonitor C)

set(CMAKE_C_STANDARD 99)

# 指定交叉编译工具链前缀，例如 x86_64-openwrt-linux-
# 记得在调用cmake时加 -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake

# 查找 libpcap
find_package(PkgConfig REQUIRED)
pkg_check_modules(PCAP REQUIRED libpcap)

include_directories(${PCAP_INCLUDE_DIRS})

set(SOURCES src/main.c)

add_executable(${PROJECT_NAME} ${SOURCES})

# 链接 libpcap 和 pthread
target_link_libraries(${PROJECT_NAME} ${PCAP_LIBRARIES} pthread)

target_compile_options(${PROJECT_NAME} PRIVATE -Wall -O2)
