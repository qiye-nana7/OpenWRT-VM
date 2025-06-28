#!/bin/sh
RUN mkdir -p /var/lock

# 配置DNS
echo "nameserver 8.8.8.8" > /etc/resolv.conf

# 禁用签名验证（改变系统文件，不安全）
sed -i '$s/^/# /' /etc/opkg.config # 注释掉最后一行
# sed -i '$d' /etc/opkg.config # 删除最后一行

# 重写 distfeeds.conf
cat > /etc/opkg/distfeeds.conf <<EOF
src/gz openwrt_core https://downloads.openwrt.org/releases/22.03.5/targets/x86/64/packages
src/gz openwrt_base https://downloads.openwrt.org/releases/22.03.5/packages/x86_64/base
src/gz openwrt_luci https://downloads.openwrt.org/releases/22.03.5/packages/x86_64/luci
src/gz openwrt_packages https://downloads.openwrt.org/releases/22.03.5/packages/x86_64/packages
src/gz openwrt_routing https://downloads.openwrt.org/releases/22.03.5/packages/x86_64/routing
src/gz openwrt_telephony https://downloads.openwrt.org/releases/22.03.5/packages/x86_64/telephony
EOF

# 更新索引
opkg update

# 安装 libpcap
opkg install libpcap