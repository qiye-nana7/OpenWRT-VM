# 2025 年 电子科技大学软件学院计算机网络实验

## 🧪 实验平台

- 主机架构：Intel X86_64
- 操作系统：Windows 11 24H2 26100.4351
- 虚拟环境：WSL + OpenWRT docker虚拟机（x86_64 架构）

## 📥 输入输出

- **输入**：使用 C 语言编写的网络流量监视程序源码
- **输出**：可在 OpenWRT-x86_64 虚拟机中运行的 ELF 可执行文件

## 🛠️ 使用工具链

- **WSL**：构建 x86_64 Linux 编译基本环境
- **Docker**：用于构建 OpenWRT 虚拟机环境
- **OpenWRT SDK**（包体较大，自行下载）：在 x86_64 Linux 中提供交叉编译 OpenWRT 程序环境（动态库）

## 使用方法
1. 交叉编译（这里用 cmake 工具）`
```bash
cmake -B build -DCMAKE_TOOLCHAIN_FILE=path/to/toolchain.cmake

cmake --build build
```
记得在toolchain.cmake里改成你的SDK路径

3. 构建虚拟机
```bash
// 加载docker
docker load -i ./sulinggg_openwrt.tar

// 构建docker
docker build -t my-openwrt-pcap -f dockerfile .

// 运行docker
docker run --privileged \
           --cap-add=NET_ADMIN \
           --net=host \
           -v $(pwd):/root \
           -it my-openwrt-pcap /bin/ash
```
```sh
// 容器内初始化
/root/docker_init.sh

// 运行可执行文件（自己改路径）
/path/to/TrafficMonitor
```