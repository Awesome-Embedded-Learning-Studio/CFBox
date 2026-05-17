# CFBox

**[中文](README.md)** | [English](README.en.md)

用现代 C++23 实现的极简 BusyBox 替代品。

[![CI](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml/badge.svg)](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++23-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.26+-064F8C?logo=cmake)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-331_passing-brightgreen)](tests/)
[![Applets](https://img.shields.io/badge/Applets-109-brightgreen)](src/applets/)

## 概述

CFBox 是一个单一可执行文件的 Unix 工具集，通过符号链接分发。109 个 applet 已实现并通过测试，CI 流水线覆盖原生构建、交叉编译、QEMU 用户/系统模式测试。支持 CMake 配置化构建（per-applet 开关）、GNU 风格长选项、彩色帮助输出。

**设计理念：** 简洁优先 — 现代C++（`std::expected`） — 嵌入式友好（交叉编译、静态链接）

## 体积对比

| 项目 | 语言 | 体积 | Applets | 体积/Applet |
|------|------|------|---------|-------------|
| **CFBox (size-opt)** | **C++23** | **446 KB** | **109** | **~4.1 KB** |
| Toybox | C | ~500 KB | 238 | ~2.1 KB |
| BusyBox (full) | C | ~1.7 MB | 274 | ~9 KB |
| uutils/coreutils | Rust | ~11 MB | ~100 | ~110 KB |

> CFBox 比 BusyBox 小 **3-4x**，在相似体积下提供了完整 awk 解释器、归档工具集（tar/cpio/ar/unzip/gzip）、diff/patch（Myers O(ND) 算法）、进程工具集（ps/top/pstree/pgrep/pmap）以及内置 TUI 框架。

## 性能

| 操作 | 数据规模 | 耗时 |
|------|---------|------|
| grep -c | 10 MB | 54 ms |
| cat | 10 MB | 63 ms |
| wc | 10 MB | 17 ms |
| sort | 100K 行 | 32 ms |
| diff | 100K 行（相似文件） | 79 ms |

- grep/cat/wc 均为流式处理，读取 `/dev/urandom` 不会内存爆炸
- diff 使用 Myers O(ND) 算法，sort 预计算排序 key 避免重复分配
- 零外部依赖：手写轻量 deflate/inflate 替代 zlib

## 快速开始

```bash
# 构建
cmake -B build
cmake --build build

# 测试
ctest --test-dir build --output-on-failure   # 331 个 GTest 单元测试
bash tests/integration/run_all.sh            # 54 套集成测试脚本

# 通过子命令运行
./build/cfbox echo "Hello, World!"

# 或安装符号链接
./scripts/gen_links.sh /usr/local/bin
echo "Hello, World!"   # 通过符号链接调用 cfbox
```

## 支持的命令（109 个）

### 文本处理（31 个）

`echo`, `printf`, `cat`, `head`, `tail`, `wc`, `sort`, `uniq`, `grep`, `sed`, `fold`, `expand`, `cut`, `paste`, `nl`, `comm`, `tr`, `tac`, `rev`, `shuf`, `factor`, `od`, `split`, `seq`, `tsort`, `expr`, `awk`, `diff`, `patch`, `cmp`, `ed`

### 文件操作（19 个）

`mkdir`, `rm`, `cp`, `mv`, `ls`, `find`, `ln`, `touch`, `stat`, `install`, `mktemp`, `truncate`, `du`, `df`, `readlink`, `realpath`, `rmdir`, `link`, `unlink`

### 归档与压缩（6 个）

`tar`（ustar 格式）, `cpio`（newc 格式）, `ar`（静态库）, `unzip`, `gzip`, `gunzip`

### Shell 与脚本（2 个）

`sh`（POSIX shell：管道、重定向、变量展开、命令替换、if/while/for、15 个内置命令）, `xargs`

### 系统信息（20 个）

`pwd`, `basename`, `dirname`, `uname`, `hostname`, `whoami`, `id`, `tty`, `date`, `nproc`, `logname`, `hostid`, `printenv`, `env`, `uptime`, `free`, `cal`, `dmesg`, `who`, `test`

### 进程管理（15 个）

`ps`, `top`, `kill`, `pgrep`/`pkill`, `pidof`, `pstree`, `pmap`, `fuser`, `pwdx`, `sysctl`, `iostat`, `watch`, `nice`, `renice`, `timeout`

### 其他（16 个）

`true`, `false`, `yes`, `sleep`, `usleep`, `sync`, `nohup`, `cksum`, `md5sum`, `sum`, `hexdump`, `more`, `tee`, `init`（PID 1 initramfs init 系统）, `mkfifo`, `mknod`

> 所有 applet 均支持 `--help` / `--version`

## 系统要求

- **编译器：** GCC 13+ / Clang 17+（需 C++23 支持）
- **CMake：** 3.26+
- **平台：** Linux（x86_64 / aarch64）

## 文档

| 文档 | 说明 |
|------|------|
| [架构与设计](document/architecture.md) | 分发机制、核心基础设施、错误处理、测试体系 |
| [生产化升级路线图](document/todo/README.md) | Phase 4.5 到 v1.0 的生产化治理文档集、优先级、发布标准 |
| [交叉编译与嵌入式](document/cross-compilation.md) | 工具链、CMake 选项、构建示例、二进制大小对比 |
| [QEMU 测试](document/qemu-testing.md) | 用户模式 / 系统模式测试、init applet、内核配置 |
| [持续集成](document/ci.md) | CI 流水线阶段说明 |
| [贡献指南](CONTRIBUTING.md) | 构建、测试、编码规范、提交方式 |

## 项目结构

```
cfbox/
├── CMakeLists.txt
├── cmake/
│   ├── Config.cmake                  # Per-applet 配置（CFBOX_ENABLE_xxx 选项）
│   ├── compile/CompilerFlag.cmake    # 编译器警告与优化标志
│   ├── third_party/CPM.cmake        # CPM 依赖管理（仅 GTest）
│   └── toolchain/                   # 交叉编译工具链
├── include/cfbox/
│   ├── applet.hpp / applets.hpp     # 注册表与分发
│   ├── args.hpp                     # 短选项 + 长选项参数解析器
│   ├── error.hpp                    # std::expected 错误处理 + CFBOX_TRY
│   ├── io.hpp                       # 流式 I/O（for_each_line、read_all、write_all）
│   ├── stream.hpp                   # 逐行处理管线、LineProcessor
│   ├── deflate.hpp / inflate.hpp    # 手写轻量 DEFLATE（零外部依赖）
│   ├── compress.hpp                 # gzip 封装
│   ├── utf8.hpp                     # Unicode 宽度/计数（constexpr + static_assert）
│   ├── term.hpp                     # ANSI 彩色输出（NO_COLOR 支持）
│   ├── terminal.hpp                 # 终端控制（RawMode RAII、光标、双缓冲）
│   ├── tui.hpp                      # TUI 框架（ScreenBuffer、Key、TuiApp）
│   ├── proc.hpp                     # /proc 解析器（进程、内存、CPU、磁盘）
│   ├── regex.hpp                    # POSIX regex RAII（scoped_regex）
│   └── ...                          # help.hpp, fs_util.hpp, escape.hpp, checksum.hpp
├── src/
│   ├── main.cpp                     # 分发入口
│   └── applets/                     # 109 个命令实现
├── tests/
│   ├── unit/                        # GTest 单元测试（331 个用例）
│   └── integration/                 # Shell 集成测试（54 个脚本）
└── scripts/                         # 构建、测试、安装脚本
```

## 下一步计划

当前版本 v0.1.0，下一阶段工作按优先级排列：

### Phase 0：生产化前置门禁（进行中）

在新增 applet 之前，必须完成以下质量地基：

| 阶段 | 内容 | 状态 |
|------|------|------|
| **0A** 基线盘点 | 109 个 applet 清单、成熟度标注、profile 归属、文档漂移修复 | 待开始 |
| **0B** 性能基线 | 核心命令 benchmark（cat/grep/sed/sort/find/tar/gzip/cp/tail）、RSS 回归阈值 | 待开始 |
| **0C** 体积预算 | 按 profile 设定体积上限、新增 rescue/container profile、每 applet 增量追踪 | 待开始 |
| **0D** IO 策略 | 流式处理审计（head/tail/sed/tr/md5sum 等整改为流式）、大文件/管道/broken pipe 测试 | 待开始 |
| **0E** 安全加固 | 数值解析统一 helper、parser fuzz smoke、特权命令隔离测试 | 待开始 |
| **0F** CI 与发布 | 分层 CI（sanitizer/benchmark/differential/cross/QEMU）、可复现构建、错误格式规范 | 待开始 |

### Phase 1：核心系统（Phase 0 完成后）

新增 `chmod`、`chown`、`chgrp`、`mount`、`umount`、`chroot`、`dd`、`stty` 等系统级 applet，深化核心命令功能。

> 详细路线图见 [document/todo/README.md](document/todo/README.md)。

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

MIT 许可证 — 详见 [LICENSE](LICENSE)。
