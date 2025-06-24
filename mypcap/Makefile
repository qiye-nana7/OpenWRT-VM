# === 路径配置 ===
TOOLCHAIN_DIR := ../openwrt-sdk-24.10.0-armsr-armv8_gcc-13.3.0_musl.Linux-x86_64/staging_dir/toolchain-aarch64_generic_gcc-13.3.0_musl
STAGING_DIR := ../openwrt-sdk-24.10.0-armsr-armv8_gcc-13.3.0_musl.Linux-x86_64/staging_dir
TARGET := target-aarch64_generic_musl
TARGET_CC := $(TOOLCHAIN_DIR)/bin/aarch64-openwrt-linux-musl-gcc

# === 编译选项 ===
INCLUDES := -I"$(STAGING_DIR)/$(TARGET)/usr/include"
LIBS     := -L"$(STAGING_DIR)/$(TARGET)/usr/lib" -lpcap
CFLAGS   := -Wall -O2

# === 源文件和目标文件 ===
SRC := src/main.c
OUT := traffic_monitor

all:
	@echo "[*] Compiling $(SRC) -> $(OUT)"
	$(TARGET_CC) $(CFLAGS) $(INCLUDES) $(SRC) -o $(OUT) $(LIBS)
	@echo "[✔] Build finished: $(OUT)"

clean:
	@rm -f $(OUT)
	@echo "[✔] Cleaned: $(OUT)"