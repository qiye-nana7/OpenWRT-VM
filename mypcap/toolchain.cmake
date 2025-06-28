# toolchain.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)  # 根据你目标平台改

# 记得改路径
set(tools /path/to/your/openwrt/sdk/staging_dir/toolchain-x86_64_gcc-11.3.0_musl)

set(CMAKE_C_COMPILER ${tools}/bin/x86_64-openwrt-linux-gcc)
set(CMAKE_CXX_COMPILER ${tools}/bin/x86_64-openwrt-linux-g++)

set(CMAKE_FIND_ROOT_PATH ${tools}/x86_64-openwrt-linux)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
