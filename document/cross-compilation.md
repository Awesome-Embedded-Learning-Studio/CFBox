# 交叉编译与嵌入式

CFBox 支持三种架构的交叉编译，并提供体积优化和静态链接选项。

## 支持架构

| 架构 | 说明 |
|------|------|
| x86_64 | 原生开发与测试 |
| AArch64 | 嵌入式 64 位 ARM（如 Raspberry Pi 4、QEMU virt） |
| ARMv7-A | 嵌入式 32 位 ARM hard-float（如 STM32MP、QEMU vexpress） |

## 工具链文件

| 文件 | 目标架构 | 编译器 |
|------|----------|--------|
| [Toolchain-aarch64.cmake](../cmake/toolchain/Toolchain-aarch64.cmake) | AArch64 Linux | `aarch64-linux-gnu-g++` |
| [Toolchain-armhf.cmake](../cmake/toolchain/Toolchain-armhf.cmake) | ARMv7-A hard-float | `arm-none-linux-gnueabihf-g++`（Arm GNU Toolchain 15.2） |

## CMake 选项

| 选项 | 说明 |
|------|------|
| `CFBOX_OPTIMIZE_FOR_SIZE=ON` | Release 构建使用 `-Os` 替代 `-O2` |
| `CFBOX_STATIC_LINK=ON` | 链接时添加 `-static`，消除运行时依赖 |

## 构建示例

### AArch64

```bash
# 动态链接
cmake -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCFBOX_OPTIMIZE_FOR_SIZE=ON
cmake --build build-aarch64

# 静态链接
cmake -B build-aarch64-static \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-aarch64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
  -DCFBOX_STATIC_LINK=ON
cmake --build build-aarch64-static
```

前置依赖：`sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu`

### ARMv7-A

```bash
# 静态链接（需要 Arm GNU Toolchain）
cmake -B build-armhf-static \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain/Toolchain-armhf.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCFBOX_OPTIMIZE_FOR_SIZE=ON \
  -DCFBOX_STATIC_LINK=ON
cmake --build build-armhf-static
```

前置依赖：[Arm GNU Toolchain](https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain) 15.2+（`arm-none-linux-gnueabihf-g++`）

### 验证目标二进制

```bash
file build-aarch64/cfbox          # 确认目标架构
size build-aarch64/cfbox          # 检查各 section 大小
scripts/measure_size.sh build-aarch64/cfbox  # 详细大小分析（含 Top-10 大符号）
```

## 二进制大小对比（`-Os` + LTO，stripped）

| 配置 | text | data | bss | 说明 |
|------|------|------|-----|------|
| x86-64 dynamic | 139 KB | 4 KB | 144 B | 原生基准 |
| aarch64 dynamic | 127 KB | 4 KB | 72 B | |
| armhf dynamic | 77 KB | 1.6 KB | 56 B | 最小配置 |
| aarch64 static | 1.7 MB | 57 KB | 30 KB | 含 libstdc++ + libc |
| armhf static | 1.1 MB | 30 KB | 17 KB | 含 libstdc++ + libc |

## 体积说明

`std::regex`（用于 [grep.cpp](../src/applets/grep.cpp) 和 [sed.cpp](../src/applets/sed.cpp)）贡献约 30-40% 的 text section。若需进一步压缩体积，可考虑替换为手写匹配或轻量 regex 库。

### 已知限制

| 风险 | 说明 |
|------|------|
| `std::regex` + musl-libc 静态链接 | 存在已知 bug，当前仅验证 glibc 场景 |
| 静态链接体积膨胀 | 已度量，暂保留 `std::regex`，后续视需求替换 |
