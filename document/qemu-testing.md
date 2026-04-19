# QEMU 测试

CFBox 提供两层 QEMU 测试，验证在 ARM 嵌入式环境中的真实运行能力。

## 用户模式测试

在主机上通过 QEMU 用户模式模拟器直接运行交叉编译的静态二进制，执行完整集成测试。

### 使用便捷脚本

```bash
# 测试 AArch64 静态二进制
./scripts/qemu_user_test.sh --target aarch64 --link static

# 测试 ARMv7-A 静态二进制
./scripts/qemu_user_test.sh --target armhf --link static

# 测试所有架构
./scripts/qemu_user_test.sh --target all --link static
```

脚本 [qemu_user_test.sh](../scripts/qemu_user_test.sh) 自动：
1. 检测 QEMU 是否安装（`qemu-aarch64-static` / `qemu-arm-static`）
2. 运行冒烟测试（`cfbox --list`、`cfbox echo "Hello from <arch>"`）
3. 创建透明 wrapper 脚本，运行完整集成测试套件

### 前置依赖

```bash
# Debian/Ubuntu
sudo apt-get install qemu-user-static

# Arch Linux
sudo pacman -S qemu-user-static
```

## 系统模式测试

构建最小 Linux 内核 + initramfs，在 QEMU 中完整引导，CFBox 的 `init` applet 作为 PID 1 运行。

### 构建步骤

#### 1. 交叉编译 CFBox 静态二进制

```bash
cmake -B build-aarch64-static \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
  -DCFBOX_STATIC_LINK=ON
cmake --build build-aarch64-static
```

#### 2. 构建 initramfs

```bash
./scripts/build_initramfs.sh \
  --arch aarch64 \
  --cfbox build-aarch64-static/cfbox \
  --output build/aarch64-initramfs.cpio
```

[build_initramfs.sh](../scripts/build_initramfs.sh) 会：
- 创建最小目录结构（`bin/`、`dev/`、`proc/`、`sys/`、`tmp/`）
- 复制 CFBox 到 `bin/cfbox`
- 运行 `cfbox --list` 获取所有 applet 名称，创建符号链接
- 创建 `/init` → `bin/cfbox` 符号链接（内核入口点）
- 打包为 cpio 归档

对于交叉编译的二进制，脚本会自动使用 QEMU 来执行 `cfbox --list`。

#### 3. 构建 Linux 内核

```bash
cd third_party/linux

# AArch64（使用最小配置）
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- allnoconfig
# 启用必要选项（见下方配置说明）
scripts/config --enable CONFIG_SERIAL_AMBA_PL011
scripts/config --enable CONFIG_SERIAL_AMBA_PL011_CONSOLE
# ... 或使用项目提供的配置
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- -j$(nproc)

# ARMv7-A
make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- vexpress_defconfig
make ARCH=arm CROSS_COMPILE=arm-none-linux-gnueabihf- -j$(nproc)
```

#### 4. 引导测试

```bash
./scripts/qemu_system_test.sh \
  --arch aarch64 \
  --kernel third_party/linux/arch/arm64/boot/Image \
  --initramfs build/aarch64-initramfs.cpio \
  --timeout 120
```

[qemu_system_test.sh](../scripts/qemu_system_test.sh) 会：
- 启动 QEMU（`-nographic`，串口控制台）
- 实时流式输出内核日志
- 等待 `ALL TESTS COMPLETE` 标记
- 统计通过/失败数量

### init applet 工作原理

[init.cpp](../src/applets/init.cpp) 是专用的系统初始化 applet：

1. 检测是否为 PID 1（`getpid() == 1`）
2. 若是 PID 1，自动挂载：
   - `proc` → `/proc`
   - `sysfs` → `/sys`
   - `devtmpfs` → `/dev`
3. 打印内核版本和 PID 信息
4. 运行冒烟测试：echo、cat /proc/version、ls /、wc -l /proc/cpuinfo
5. 报告结果后调用 `reboot(RB_POWER_OFF)` 关机

### 内核最小配置

AArch64 的最小内核配置见 [configs/qemu-virt-aarch64.config](../configs/qemu-virt-aarch64.config)，关键选项：

```
CONFIG_ARM64=y                  # 架构
CONFIG_ARCH_VEXPRESS=y          # QEMU virt 平台
CONFIG_SERIAL_AMBA_PL011=y      # 串口控制台
CONFIG_BLK_DEV_INITRD=y         # initramfs 支持
CONFIG_PROC_FS=y                # /proc 文件系统
CONFIG_DEVTMPFS=y               # /dev 自动挂载
CONFIG_BINFMT_ELF=y             # ELF 执行
```
