# CFBox

**[中文](README.md)** | [English](README.en.md)

用现代 C++23 实现的极简 BusyBox 替代品。

[![CI](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml/badge.svg)](https://github.com/Awesome-Embedded-Learning-Studio/CFBox/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++23-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.26+-064F8C?logo=cmake)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-149_passing-brightgreen)](tests/)
[![Applets](https://img.shields.io/badge/Applets-17-brightgreen)](src/applets/)

## 概述

CFBox 是一个单一可执行文件的 Unix 工具集，通过符号链接分发。17 个 applet 已实现并通过测试，CI 流水线覆盖原生构建、交叉编译、QEMU 用户/系统模式 5 种测试场景。支持 CMake 配置化构建（per-applet 开关）、GNU 风格长选项、彩色帮助输出。

**设计理念：** 简洁优先 — 现代C++（`std::expected`） — 嵌入式友好（交叉编译、静态链接）

## 快速开始

```bash
# 构建
cmake -B build
cmake --build build

# 测试
ctest --test-dir build --output-on-failure   # 149 个 GTest 单元测试
bash tests/integration/run_all.sh            # 17 套集成测试脚本

# 通过子命令运行
./build/cfbox echo "Hello, World!"

# 或安装符号链接
./scripts/gen_links.sh /usr/local/bin
echo "Hello, World!"   # 通过符号链接调用 cfbox
```

## 支持的命令

### 文本处理

| 命令 | 支持的标志 / 功能 |
|------|-------------------|
| `echo` | `-n`（不换行），`-e`（解释转义序列），所有 applet 支持 `--help` / `--version` |
| `printf` | 格式字符串（`%s` `%d` `%f` `%c` `%%`），格式重用 |
| `cat` | `-n`（显示行号），`-b`（非空行编号），`-A`（显示不可打印字符），stdin 透传 |
| `head` | `-n N`（前 N 行），`-c N`（前 N 字节），多文件头部 |
| `tail` | `-n N`（后 N 行），`-c N`（后 N 字节），多文件尾部 |
| `wc` | `-l`（行数），`-w`（词数），`-c`（字节数），`-m`（字符数），多文件合计 |
| `sort` | `-r`（逆序），`-n`（数值排序），`-u`（去重），`-k N`（按字段排序），多文件合并 |
| `uniq` | `-c`（计数），`-d`（仅重复行），`-u`（仅唯一行），stdin 支持 |
| `grep` | `-E`（扩展正则），`-i`（忽略大小写），`-v`（反转匹配），`-n`（行号），`-r`（递归搜索），`-c`（计数），`-l`（匹配文件名），`-q`（静默） |
| `sed` | `-n`（禁止自动输出），`-e 脚本`；替换 `s/模式/替换/[g\|p\|d]`，行地址，范围，`$` |

### 文件操作

| 命令 | 支持的标志 / 功能 |
|------|-------------------|
| `mkdir` | `-p`/`--parents`（递归创建父目录），`-m`/`--mode MODE`（设置权限） |
| `rm` | `-r`/`--recursive`（递归删除），`-f`/`--force`（强制），`-i`（交互确认），`/` 安全检查 |
| `cp` | `-r`/`--recursive`（递归复制），`-p`/`--preserve`（保留权限），多文件到目录 |
| `mv` | `-f`（强制覆盖），跨文件系统回退（复制 + 删除） |

### 目录与搜索

| 命令 | 支持的标志 / 功能 |
|------|-------------------|
| `ls` | `-a`/`--all`（显示隐藏文件），`-l`/`--long`（长格式），`-h`/`--human-readable`（人类可读大小） |
| `find` | `-name 模式`（glob 匹配），`-type [f\|d\|l]`，`-maxdepth N`，`-exec 命令 {} ;` |

### 系统

| 命令 | 说明 |
|------|------|
| `init` | 系统初始化 — PID 1 时自动挂载 proc/sysfs/devtmpfs，运行冒烟测试后关机 |

## 系统要求

- **编译器：** GCC 13+ / Clang 17+（需 C++23 支持）
- **CMake：** 3.26+
- **平台：** Linux（x86_64 / aarch64）

## 文档

| 文档 | 说明 |
|------|------|
| [架构与设计](document/architecture.md) | 分发机制、核心基础设施、错误处理、测试体系 |
| [交叉编译与嵌入式](document/cross-compilation.md) | 工具链、CMake 选项、构建示例、二进制大小对比 |
| [QEMU 测试](document/qemu-testing.md) | 用户模式 / 系统模式测试、init applet、内核配置 |
| [持续集成](document/ci.md) | CI 流水线 5 个阶段说明 |
| [贡献指南](CONTRIBUTING.md) | 构建、测试、编码规范、提交方式 |

## 项目结构

```
cfbox/
├── CMakeLists.txt
├── cmake/
│   ├── Config.cmake                  # Per-applet 配置（CFBOX_ENABLE_xxx 选项）
│   ├── compile/CompilerFlag.cmake    # 编译器警告与优化标志
│   ├── third_party/CPM.cmake        # CPM 依赖管理
│   └── toolchain/                   # 交叉编译工具链
├── configs/
│   └── qemu-virt-aarch64.config     # QEMU aarch64 最小内核配置
├── document/                         # 详细文档
├── include/cfbox/
│   ├── applet_config.hpp.in         # CMake 生成的配置（版本号 + 启用开关）
│   ├── applet.hpp / applets.hpp     # 注册表与分发
│   ├── args.hpp                     # 短选项 + 长选项参数解析器
│   ├── help.hpp                     # --help / --version 帮助系统
│   ├── term.hpp                     # ANSI 彩色输出（NO_COLOR 支持）
│   ├── utf8.hpp                     # Unicode 感知的宽度/计数工具
│   └── ...                          # error.hpp, io.hpp, fs_util.hpp, escape.hpp
├── src/
│   ├── main.cpp                     # 分发入口
│   └── applets/                     # 17 个命令实现
├── tests/
│   ├── unit/                        # GTest 单元测试（149 个用例）
│   └── integration/                 # Shell 集成测试（17 个脚本）
├── scripts/                         # 构建、测试、安装脚本
├── .github/workflows/ci.yml         # CI 流水线
└── CONTRIBUTING.md                   # 贡献指南
```

## 贡献

见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

MIT 许可证 — 详见 [LICENSE](LICENSE)。
