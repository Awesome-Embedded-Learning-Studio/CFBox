# CFBox

**[中文](README.md)** | [English](README.en.md)

用现代 C++23 实现的极简 BusyBox 替代品。

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![C++23](https://img.shields.io/badge/C++23-00599C?logo=cplusplus)](https://en.cppreference.com/w/cpp/23)
[![CMake](https://img.shields.io/badge/CMake-3.26+-064F8C?logo=cmake)](https://cmake.org/)
[![Tests](https://img.shields.io/badge/Tests-124_passing-brightgreen)](tests/)
[![Applets](https://img.shields.io/badge/Applets-16%2F16-brightgreen)](src/applets/)

## 概述

CFBox 是一个单一可执行文件的 Unix 工具集，通过符号链接分发。每个子命令可以通过 `argv[0]` 检测或显式子命令语法调用。

**设计理念：**
- **简洁优先** — 先跑通核心功能，再追求完整 POSIX 兼容
- **现代 C++** — 利用 C++23 特性，使用 `std::expected` 做错误处理
- **嵌入式友好** — 支持交叉编译，最小运行时依赖

全部 16 个 applet 已实现并通过测试。

## 快速开始

```bash
# 构建
cmake -B build
cmake --build build

# 测试
ctest --test-dir build --output-on-failure   # 108 个 GTest 单元测试
bash tests/integration/run_all.sh            # 16 套集成测试脚本

# 通过子命令运行
./build/cfbox echo "Hello, World!"

# 或安装符号链接
./scripts/gen_links.sh /usr/local/bin
echo "Hello, World!"   # 通过符号链接调用 cfbox
```

## 支持的命令

| 命令 | 支持的标志 / 功能 |
|------|-------------------|
| `echo` | `-n`（不换行），`-e`（解释转义序列） |
| `printf` | 格式字符串（`%s` `%d` `%f` `%c` `%%`），格式重用 |
| `cat` | `-n`（显示行号），`-b`（非空行编号），`-A`（显示不可打印字符），stdin 透传 |
| `head` | `-n N`（前 N 行），`-c N`（前 N 字节），多文件头部 |
| `tail` | `-n N`（后 N 行），`-c N`（后 N 字节），多文件头部 |
| `wc` | `-l`（行数），`-w`（词数），`-c`（字节数），`-m`（字符数），多文件合计 |
| `sort` | `-r`（逆序），`-n`（数值排序），`-u`（去重），`-k N`（按字段排序），多文件合并 |
| `uniq` | `-c`（计数），`-d`（仅重复行），`-u`（仅唯一行），stdin 支持 |
| `mkdir` | `-p`（递归创建父目录），`-m MODE`（设置权限） |
| `rm` | `-r`（递归删除），`-f`（强制），`-i`（交互确认），`/` 安全检查 |
| `cp` | `-r`（递归复制），`-p`（保留权限），多文件到目录 |
| `mv` | `-f`（强制覆盖），跨文件系统回退（复制 + 删除） |
| `ls` | `-a`（显示隐藏文件），`-l`（长格式），`-h`（人类可读大小） |
| `grep` | `-E`（扩展正则），`-i`（忽略大小写），`-v`（反转匹配），`-n`（行号），`-r`（递归搜索），`-c`（计数），`-l`（匹配文件名），`-q`（静默） |
| `find` | `-name 模式`（glob 匹配），`-type [f\|d\|l]`，`-maxdepth N`，`-exec 命令 {} ;` |
| `sed` | `-n`（禁止自动输出），`-e 脚本`；替换 `s/模式/替换/[g\|p\|d]`，行地址，范围，`$` |

## 系统要求

- **编译器：** GCC 13+ / Clang 17+（需 C++23 支持）
- **CMake：** 3.26+
- **平台：** Linux（x86_64 / aarch64）

## 架构

**分发机制：** `main.cpp` 通过 `argv[0]` 检测调用名（符号链接检测），并回退到子命令语法（`cfbox echo ...`）。[applets.hpp](include/cfbox/applets.hpp) 中的 `APPLET_REGISTRY` 是 `constexpr std::array`，将名称映射到入口函数。

**核心基础设施：**

| 头文件 | 用途 |
|--------|------|
| [error.hpp](include/cfbox/error.hpp) | `std::expected<T, Error>` 及 `CFBOX_TRY` 宏用于错误传播 |
| [applet.hpp](include/cfbox/applet.hpp) | `AppEntry` 结构体与 `find_applet()` 查找 |
| [args.hpp](include/cfbox/args.hpp) | 命令行参数解析器 — 短标志、带值标志、`--` 分隔符、位置参数 |
| [io.hpp](include/cfbox/io.hpp) | 文件 I/O 工具 — `read_all`、`read_lines`、`read_all_stdin`、`write_all`、`split_lines` |
| [fs_util.hpp](include/cfbox/fs_util.hpp) | 返回 `Result<T>` 的文件系统封装 — `exists`、`mkdir_recursive`、`copy_recursive`、`rename` 等 |
| [escape.hpp](include/cfbox/escape.hpp) | `echo` / `printf` 的转义序列处理 |

**测试体系：** 通过 CPM 获取 GTest 进行单元测试（108 个用例）；基于 Shell 的集成测试（16 个脚本），与 GNU coreutils 行为对比。

## 项目结构

```
cfbox/
├── CMakeLists.txt
├── cmake/
│   ├── compile/CompilerFlag.cmake
│   ├── third_party/CPM.cmake
│   └── toolchain/               # 交叉编译工具链（Phase 4）
├── include/cfbox/
│   ├── applet.hpp               # AppEntry 注册表 & 查找
│   ├── applets.hpp              # 16 个 applet 声明 & APPLET_REGISTRY
│   ├── args.hpp                 # 命令行参数解析器
│   ├── error.hpp                # std::expected<T, Error> & CFBOX_TRY
│   ├── escape.hpp               # 转义序列处理器
│   ├── fs_util.hpp              # 文件系统工具封装
│   └── io.hpp                   # 文件 I/O 工具
├── src/
│   ├── main.cpp                 # 分发入口（argv[0] + 子命令）
│   └── applets/                 # 16 个命令实现
│       ├── echo.cpp   printf.cpp  cat.cpp    head.cpp
│       ├── tail.cpp   wc.cpp      sort.cpp   uniq.cpp
│       ├── mkdir.cpp  rm.cpp      cp.cpp     mv.cpp
│       ├── ls.cpp     grep.cpp    find.cpp   sed.cpp
├── tests/
│   ├── unit/                    # GTest 单元测试（108 个用例）
│   └── integration/             # Shell 集成测试（16 个脚本）
├── scripts/
│   └── gen_links.sh             # 符号链接安装脚本
├── .clang-format
├── CONTRIBUTING.md
├── LICENSE
├── README.md
└── ROADMAP.md
```

## 开发状态

全部核心 applet 已实现（Phase 0-3 完成）。剩余的 Phase 4（交叉编译）工作见 [ROADMAP.md](ROADMAP.md)。

## 贡献

构建说明、编码规范和提交方式见 [CONTRIBUTING.md](CONTRIBUTING.md)。

## 许可证

MIT 许可证 — 详见 [LICENSE](LICENSE)。
