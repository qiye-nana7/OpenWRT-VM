# 2025 年 电子科技大学软件学院计算机网络实验

## 🧪 实验平台

- 主机架构：ARM64（Apple Silicon）
- 操作系统：macOS 15.5
- 虚拟环境：OpenWRT 虚拟机（ARM64 架构）

## 📥 输入输出

- **输入**：使用 C 语言编写的网络流量监视程序源码
- **输出**：可在 OpenWRT-ARM64 虚拟机中运行的 ELF 可执行文件

## 🛠️ 使用工具链

- **Docker**：用于构建 x86_64 Linux 编译环境（适配 OpenWRT SDK 要求）
- **OpenWRT SDK**：
  - 名称：`openwrt-sdk-24.10.0-armsr-armv8_gcc-13.3.0_musl.Linux-x86_64`
  - 用途：在 x86_64 Linux 容器中交叉编译 OpenWRT-ARM64 程序