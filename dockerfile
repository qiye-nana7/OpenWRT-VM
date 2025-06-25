# 使用 x86_64 架构的 Debian 作为基础镜像（适配 OpenWRT SDK）
FROM debian:bullseye

# 安装编译环境和基本工具
RUN apt update && \
    apt install -y \
        build-essential \
        wget \
        git \
        unzip \
        vim \
        libpcap-dev \
        ca-certificates \
        python3 \
        python3-pip \
        file \
        rsync \
        gawk \
        libncurses-dev \
        pkg-config

# 设置默认工作目录（和挂载目录保持一致）
WORKDIR /sdk

# 默认执行 bash，方便手动进入编译流程
CMD ["/bin/bash"]