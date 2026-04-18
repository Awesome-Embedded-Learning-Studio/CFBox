# CFBox — RoadMap

> Phase 0-4 已全部完成：16 个 applet，108 个 GTest 单元测试 + 16 套集成测试全部通过。详见 [README.md](README.md)。

---

## Phase 4 — 嵌入式交叉编译验证

**目标：交叉编译通过，二进制尽量小**

- [x] 验证 `Toolchain-aarch64.cmake` 能编译（aarch64 + armhf 双工具链）
- [x] 开启 `-Os` / LTO，测量 stripped binary 大小
- [x] 验证静态链接二进制（`-static`）无运行时依赖
- [x] 用 `size` 命令检查各 section 大小，识别膨胀来源
- [x] CI 中增加交叉编译 job

### 工具链文件

| 文件 | 目标架构 | 编译器 |
|------|----------|--------|
| [Toolchain-aarch64.cmake](cmake/toolchain/Toolchain-aarch64.cmake) | AArch64 Linux | `aarch64-linux-gnu-g++` |
| [Toolchain-armhf.cmake](cmake/toolchain/Toolchain-armhf.cmake) | ARMv7-A hard-float | `arm-none-linux-gnueabihf-g++` (Arm GNU Toolchain 15.2) |

### CMake 选项

| 选项 | 说明 |
|------|------|
| `CFBOX_OPTIMIZE_FOR_SIZE=ON` | Release 构建使用 `-Os` 替代 `-O2` |
| `CFBOX_STATIC_LINK=ON` | 链接时添加 `-static`，消除运行时依赖 |

### 二进制大小对比（`-Os` + LTO）

| 配置 | text | data | bss | 说明 |
|------|------|------|-----|------|
| x86-64 dynamic | 139 KB | 4 KB | 144 B | 原生基准 |
| aarch64 dynamic | 127 KB | 4 KB | 72 B | |
| armhf dynamic | 77 KB | 1.6 KB | 56 B | 最小配置 |
| aarch64 static | 1.7 MB | 57 KB | 30 KB | 含 libstdc++ + libc |
| armhf static | 1.1 MB | 30 KB | 17 KB | 含 libstdc++ + libc |

### 膨胀来源

`std::regex`（grep.cpp / sed.cpp）贡献约 30-40% 的 text section。若需进一步压缩体积，可考虑替换为手写匹配或轻量 regex 库。

### 风险备忘

| 风险 | 决策 |
|------|------|
| `std::regex` 在 musl-libc 静态链接下有 bug | 当前仅验证 glibc；musl 场景需后续验证 |
| 静态链接 + `std::regex` 体积膨胀 | 已度量，暂保留 `std::regex`，后续视需求替换 |

---

*更新时间：2026-04-19*
