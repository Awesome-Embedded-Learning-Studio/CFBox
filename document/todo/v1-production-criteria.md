# v1.0 生产可用标准

CFBox v1.0 的发布标准是"明确场景内生产可用"，不是"BusyBox 全量功能兼容"。v1.0 必须公开支持边界，并且每个支持边界都能被测试复现。

## Profile 标准

| Profile | 目标 | v1.0 要求 |
|---------|------|-----------|
| `rescue` | initramfs / 救援系统 | shell、权限、挂载、`dd`、归档、压缩、网络诊断可用 |
| `container` | 最小容器基础工具集 | 高频 shell 脚本、文件操作、文本处理、网络下载可用 |
| `embedded` | 嵌入式 runtime | PID 1、挂载、日志、getty/login、网络配置、设备管理基础可用 |
| `minimal` | 极小构建 | 明确 applet 集合、体积报告、无隐式功能承诺 |
| `full` | 开发者完整体验 | 默认启用稳定 applet，实验性 applet 必须标注 |

### 当前 CMake 状态 vs v1.0 目标

当前 `cmake/Config.cmake` 定义了 `minimal`、`embedded`、`desktop`、`full` 四个 profile。v1.0 需要：

- 将 `desktop` 重命名或拆分为 `rescue` 和 `container`（详见 [Phase 0C](phases/phase-0c-size-profile-budget.md) Profile 迁移计划）。
- `minimal` 保持最小文件/文本操作集合。
- `embedded` 增加 init、挂载、日志等 PID 1 相关能力。
- `full` 保持所有 applet 启用。

## Applet 成熟度

每个 applet 应标注成熟度：

| 等级 | 含义 | 测试要求 |
|------|------|----------|
| `experimental` | 可运行，但行为覆盖有限，不进入生产 profile | 基础 smoke test |
| `basic` | 覆盖常见用法，有基础单元和集成测试 | GTest + 1 个集成测试 |
| `compatible` | BusyBox 高频行为基本兼容，有对比测试 | GTest + 集成测试 + differential smoke |
| `production` | 有场景测试、错误处理审计、文档和兼容性说明 | GTest + 集成 + differential + 错误路径测试 |

### 成熟度升级条件

从 `experimental` → `production` 的升级路径：

1. **experimental → basic**：补齐 `--help`/`--version`；至少覆盖主要选项的 GTest；至少 1 个集成测试。
2. **basic → compatible**：补齐 BusyBox 高频短选项；建立 differential smoke；输出格式与 BusyBox 对齐。
3. **compatible → production**：完成错误路径审计；补齐文档（示例、退出码、已知限制）；通过场景测试（initramfs/容器/嵌入式之一）。

v1.0 要求：`rescue`、`container`、`embedded` profile 中的 P0 applet 必须达到 `production`，P1 applet 至少达到 `compatible`。

## 核心功能门槛

- `sh`：通过 POSIX shell 子集测试；支持 `case`、here-doc、函数、算术展开；明确不支持项。详见 [Phase 1](phases/phase-1-core-system.md) sh 深化。
- 文件系统：`cp/mv/rm/ln/install/chmod/chown/chgrp/mkdir/find` 高频选项可用。
- 系统管理：`mount/umount/chroot/dd/stty/halt/reboot/poweroff` 在 QEMU 场景中通过。详见 [Phase 1](phases/phase-1-core-system.md) Wave 3。
- 归档压缩：`tar/cpio/gzip/gunzip/zcat` 支持 initramfs 和 rootfs 打包工作流。详见 [Phase 1](phases/phase-1-core-system.md) tar 深化。
- 网络：`ip/ifconfig/route/netstat/ping/nslookup/wget/nc` 支撑基础诊断和下载。详见 [Phase 2](phases/phase-2-network.md)。
- 日志与登录：`getty/login/su/passwd/syslogd/logger/logread` 支撑嵌入式多用户基础场景。详见 [Phase 4](phases/phase-4-multiuser.md)。

## 测试门槛

- 所有单元测试、集成测试、QEMU system-mode 测试通过。
- ASan/UBSan clean；核心 parser 和文件格式解析器 fuzz target 无已知崩溃。详见 [Phase 0E](phases/phase-0e-safety-robustness-hardening.md)。
- P0 applet 有 BusyBox 或 GNU differential tests。详见 [生产化路线图 — Differential Tests](production-roadmap.md)。
- `rescue` profile 通过 initramfs 替换测试。
- `container` profile 通过 Alpine minirootfs 替换测试。
- `embedded` profile 通过 QEMU PID 1、挂载、日志、登录 smoke test。

性能基线要求见 [Phase 0B](phases/phase-0b-performance-resource-baseline.md)，CI 集成见 [Phase 0F](phases/phase-0f-ci-repro-debuggability.md)。

## 发布矩阵

v1.0 必须发布：

| Artifact | 链接方式 | 用途 |
|----------|----------|------|
| `cfbox-x86_64-linux-musl` | static | 容器、initramfs、通用 Linux |
| `cfbox-aarch64-linux-musl` | static | ARM64 嵌入式、树莓派、边缘设备 |
| `cfbox-armv7-linux-musleabihf` | static | 32 位 ARM 嵌入式 |
| `cfbox-x86_64-linux-gnu` | dynamic 或 static | 桌面发行版和开发者测试 |

可选发布：TLS-enabled variants、debug symbols、profile-specific artifacts。

发布规范和 release checklist 详见 [生产化路线图 — 发布与分发](production-roadmap.md)。

## 体积预算

| 构建 | 目标上限 |
|------|----------|
| `minimal` static musl | <= 350 KB |
| `rescue` static musl | <= 500 KB |
| `container` static musl | <= 650 KB |
| `embedded` static musl | <= 800 KB |
| `full` static musl no-TLS | <= 900 KB |
| `full` static musl TLS | <= 1.4 MB |

每个 release candidate 必须附带 size diff，解释超过预算的新增能力。体积口径定义（profile + target + link mode + strip state）和每 applet 增量测量方法详见 [Phase 0C](phases/phase-0c-size-profile-budget.md)。

## 文档门槛

- 每个 production applet 有 `--help`、示例、退出码、已知限制。
- 文档站包含 initramfs、容器替换、交叉编译、profile 裁剪 cookbook。
- 兼容性矩阵公开列出 BusyBox/GNU/POSIX 差异。
- release notes 列出新增 applet、行为变化、破坏性变化、测试矩阵和体积变化。

文档站信息架构详见 [VitePress 文档站](documentation-site.md)。兼容性差异记录规范详见 [兼容性策略](compatibility-policy.md)。

## 不宣称的内容

v1.0 不宣称：

- BusyBox 430+ applet 全量替代。
- 完整 POSIX 认证。
- 完整 GNU coreutils 兼容。
- 默认 HTTPS/TLS 支持，除非使用 TLS 构建变体。
