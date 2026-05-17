# VitePress 文档站规划

VitePress 文档站的目标是让用户完成真实任务，而不是只浏览命令列表。文档应围绕安装、裁剪、替换、调试和兼容性展开。

## 信息架构

### Getting Started

- 安装预编译 release（从 GitHub Releases 下载、APK/AUR 安装）。
- 从源码构建（CMake 配置、依赖说明、编译命令）。
- 子命令调用：`cfbox echo hello`、`cfbox --list`。
- symlink 分发：`./gen_links.sh`、PATH 设置。
- 选择 profile：minimal、rescue、container、embedded、full。

### Applet Reference

每个 applet 一页，统一结构：

| 小节 | 内容 |
|------|------|
| Synopsis | 用法一行摘要 |
| Options | 完整选项表（短选项 + GNU 长选项） |
| Examples | 2-5 个真实用例 |
| Exit status | 退出码含义 |
| BusyBox/GNU/POSIX compatibility | 差异列表和裁决说明 |
| Known limitations | 不支持项和有意差异 |
| Maturity level | 当前成熟度等级 |

Applet reference 应由 applet 元数据生成，避免手写文档和 `--help` 漂移。

### Cookbook

必须覆盖的场景（每个场景一个页面）：

| 场景 | 关键步骤 | 涉及命令 |
|------|----------|----------|
| 制作 CFBox initramfs | 编译 → strip → gen_links → cpio 打包 | `cpio`、`gzip`、`gen_links.sh` |
| QEMU 中以 CFBox 作为 PID 1 | 内核配置 → initramfs 启动 → init 脚本 | `init`、`mount`、`sh` |
| Alpine 容器替换 BusyBox | 注入 binary → 重建 symlink → 验证 | `cp`、`ln`、`--list` |
| 交叉编译到 aarch64/armv7 | toolchain 配置 → musl SDK → 编译 | CMake toolchain |
| 构建静态 musl binary | musl-gcc 配置 → static link → strip | `size`、`strip` |
| 裁剪 applet profile | CMake profile 选择 → 自定义裁剪 | `cmake/Config.cmake` |
| 系统恢复操作 | dd 写镜像 → mount → tar 解包 → chmod | `dd`、`mount`、`tar`、`chmod` |
| 串口 getty/login | stty 配置 → getty 启动 → login 流程 | `stty`、`getty`、`login` |
| syslog/logger/logread | syslogd 配置 → logger 写入 → logread 读取 | `syslogd`、`logger`、`logread` |

### Compatibility

包含以下对比矩阵页面：

- BusyBox 对比矩阵（逐 applet 的选项差异）。
- GNU coreutils 差异（输出格式、长选项、扩展行为）。
- POSIX 支持矩阵（每个 applet 声明的 POSIX 子集）。
- Shell 支持矩阵（内建命令、语法覆盖、不支持项）。
- 不支持项和有意差异汇总。

### Architecture

技术架构页面：

- multicall 分发机制。
- applet registry 和 `APPLET_REGISTRY`。
- CMake per-applet 裁剪和 `CFBOX_ENABLE_*` 宏。
- `std::expected` 错误处理链。
- 流式 I/O 管道（`for_each_line`、`read_chunks`）。
- 自定义 DEFLATE/INFLATE 压缩框架。
- `/proc` 解析和 `proc.hpp`。
- TUI 框架（ScreenBuffer、Key、TuiApp）。

### Build & Release

- CMake profile 配置和自定义。
- musl/glibc 构建差异。
- 静态链接注意事项。
- size optimization 开关和 `-Os` + LTO。
- release artifact 下载和 checksum 验证。
- 包管理安装方式（APK、AUR、deb）。

### Contributing

- 新增 applet 流程（`applets.hpp` + `Config.cmake` + `applet_config.hpp.in` 注册三件套）。
- 测试要求（GTest + 集成测试 + differential）。
- 帮助文档要求（`HelpEntry` 常量格式）。
- good first issue 类型。
- 兼容性裁决流程（参考 [兼容性策略](compatibility-policy.md)）。

### Security

- 漏洞报告流程（GitHub Security Advisory）。
- fuzzing 状态和已知覆盖率。
- 特权命令风险列表。
- TLS 策略和零依赖默认。
- 供应链和第三方代码来源。

### Roadmap

- 指向 `document/todo/` 文档集。
- 展示当前阶段、下一阶段和 v1.0 gate。
- 避免复制整份路线图，防止维护两套真相。

## 文档生成策略

建议扩展 applet help 元数据，形成单一来源：

```
applet 元数据 (源码) ──→ --help 输出
                    ──→ man page
                    ──→ VitePress applet page
                    ──→ compatibility matrix
```

实现方案：

1. 每个 applet 的 `HelpEntry` 常量包含结构化字段（name、synopsis、options、examples、exit_codes、compatibility_notes、maturity）。
2. 构建时导出为 JSON 或 YAML。
3. VitePress 构建时读取元数据，生成 applet reference 页面。
4. `--help` 和 man page 也从同一元数据生成。

不要让 `--help`、man page、文档站分别手写。

## 导航设计

顶部导航栏：

| 标签 | 指向 |
|------|------|
| Guide | Getting Started、Build & Release、Contributing |
| Applets | Applet Reference（按类别索引） |
| Cookbook | 场景化教程 |
| Compatibility | 对比矩阵 |
| Architecture | 技术架构 |
| Roadmap | 阶段进度 |

侧边栏按任务组织，不按源代码目录组织。

侧边栏结构示例：

```
Guide/
  Getting Started
  Build from Source
  Cross Compilation
  Profiles & Trimming
  Installation
Applets/
  Text Processing (echo, cat, grep, sed, awk, ...)
  File Operations (ls, cp, mv, rm, find, ...)
  Archive & Compression (tar, gzip, cpio, ...)
  Shell & Scripting (sh, xargs, test, ...)
  System Information (ps, df, du, top, ...)
  Process Management (kill, pgrep, nice, ...)
Cookbook/
  Initramfs
  Alpine Container
  System Recovery
  Cross Compile
  ...
```

## 语言策略

当前阶段中文为主。英文 README 提供入口说明。等 v1.0 生产边界稳定后，再考虑英文文档站完整翻译。
