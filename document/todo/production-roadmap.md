# CFBox 生产化路线图

本文档是 CFBox 从 v0.1.0 走向 v1.0 生产可用的**全局总纲**。定义阶段顺序、优先级裁决、质量工程策略和发布工程规范。各 Phase 的操作细节在对应的 `phases/` 子文档中。

## 阶段总览

| 阶段 | 文档 | 目标 |
|------|------|------|
| Phase 0 | [轻量基建](phases/phase-0a-baseline-inventory.md)（0A-0F 合并） | 文档漂移修复、differential test 骨架、编译零 warning；**与 Phase 1 并行推进** |
| Phase 1 | [核心系统能力补齐](phases/phase-1-core-system.md) | P0 系统命令（chmod/chown/dd/mount/stty 等 24 个新 applet） |
| Phase 1.5 | [代码质量审查](phases/phase-1.5-code-quality-review.md) | 错误处理一致性、代码风格、测试覆盖、体积检查、兼容性记录 |
| Phase 2 | [核心命令深化](phases/phase-1-core-system.md#part-3-现有-applet-深化) | tail -f、cp -a、grep -A/-B/-C、find 布尔表达式、sh 深化等 |
| Phase 3 | [网络最小闭环](phases/phase-2-network.md) | 基础网络配置、诊断、下载、连接调试 |
| Phase 4 | [生产质量门禁深化](phases/phase-3-quality.md) | fuzzing、benchmark、POSIX 子集、release 工程 |
| Phase 5 | [多用户与嵌入式运行时](phases/phase-4-multiuser.md) | login/getty/syslog/mdev/storage |
| Phase 6 | [长尾完备性](phases/phase-5-longtail.md) | `vi`、额外压缩格式、硬件工具、长尾 applet |

### 执行策略调整（v0.2.0 规划）

Phase 0 采用**轻量基建**策略，不作为硬门禁阻塞 Phase 1。核心变更：

- **Phase 0-lite**：仅保留文档漂移修复、differential test 骨架、编译零 warning 三项，与 Phase 1 并行推进。完整的性能基线、体积预算、fuzzing 等推迟到 Phase 4。
- **Phase 1.5（新增）**：在 Phase 1 Wave 3 完成（核心系统命令到位）后，进行代码质量审查，确保质量标准统一后再进入深化阶段。
- **Phase 2 独立**：核心命令深化从原 Phase 1 中独立出来，按运维频率排序逐步推进。

原有 Phase 0A-0F 的详细内容仍保留在各 Phase 文档中，作为参考和后续深化的输入。

---

## 核心 Applet 优先级

CFBox 的生产化路线不追求尽快覆盖 BusyBox 430+ applet，而是优先让关键场景闭环。命令优先级按"生产阻断程度、脚本频率、实现风险、测试可验证性"排序。

### 裁决原则

- **P0** 是 v1.0 支持 profile 的阻断项。
- **P1** 是生产环境高频项，应在 v1.0 前尽量完成。
- **P2** 是提升可用性的常用项，可进入 v1.x 或 profile 可选集。
- **P3** 是长尾或领域专用项，不阻塞 v1.0。

### P0：生产阻断项

| 类别 | 命令 | 理由 | 落地阶段 |
|------|------|------|----------|
| 权限 | `chmod`、`chown`、`chgrp` | 安装脚本、系统修复、权限恢复必需 | [Phase 1](phases/phase-1-core-system.md) |
| 文件系统 | `mount`、`umount`、`chroot` | initramfs、救援系统、容器构建必需 | [Phase 1](phases/phase-1-core-system.md) |
| 块设备 | `dd` | 镜像写入、磁盘复制、救援恢复必需 | [Phase 1](phases/phase-1-core-system.md) |
| 终端 | `stty` | 串口、交互 shell、脚本终端控制必需 | [Phase 1](phases/phase-1-core-system.md) |
| Shell | `sh` 深化 | 所有 profile 的脚本执行基础 | [Phase 1](phases/phase-1-core-system.md) |
| 归档 | `tar` 深化、`zcat` | rootfs、源码包、日志处理高频 | [Phase 1](phases/phase-1-core-system.md) |
| 查找文本 | `find`、`grep`、`sed` 深化 | 系统脚本和运维工作流核心 | [Phase 1](phases/phase-1-core-system.md) |
| 文件复制 | `cp`、`mv`、`rm` 深化 | 保留权限、链接、交互、递归语义必须可靠 | [Phase 1](phases/phase-1-core-system.md) |

### P1：生产高频项

| 类别 | 命令 | 落地阶段 |
|------|------|----------|
| 系统控制 | `halt`、`reboot`、`poweroff`、`killall` | [Phase 1](phases/phase-1-core-system.md) |
| 会话/并发 | `setsid`、`flock` | [Phase 1](phases/phase-1-core-system.md) |
| 基础便利 | `which`、`clear`、`mountpoint` | [Phase 1](phases/phase-1-core-system.md) |
| 网络 | `ip`、`ifconfig`、`route`、`netstat`、`ping`、`nslookup`、`wget`、`nc` | [Phase 2](phases/phase-2-network.md) |
| 编码校验 | `sha256sum`、`sha512sum`、`sha1sum`、`sha384sum`、`base64`、`base32` | [Phase 1](phases/phase-1-core-system.md) |
| 登录日志 | `getty`、`login`、`su`、`passwd`、`syslogd`、`logger`、`logread` | [Phase 4](phases/phase-4-multiuser.md) |

### P2：常用增强项

| 类别 | 命令 | 落地阶段 |
|------|------|----------|
| 存储 | `blkid`、`lsblk`、`losetup`、`mkswap`、`swapon`、`swapoff`、`fsck`、`mkfs.ext2` | [Phase 4](phases/phase-4-multiuser.md) |
| 设备 | `mdev`、`pivot_root`、`switch_root` | [Phase 4](phases/phase-4-multiuser.md) |
| 运维 | `crond`、`crontab`、`time`、`tree`、`strings`、`xxd`、`uuidgen` | [Phase 5](phases/phase-5-longtail.md) |
| 网络 | `traceroute`、`tftp`、`whois` | [Phase 2](phases/phase-2-network.md) |
| 文本 | `less`、`dos2unix`、`unix2dos` | [Phase 5](phases/phase-5-longtail.md) |

### P3：长尾与领域专用项

| 类别 | 命令 | 落地阶段 |
|------|------|----------|
| 编辑器 | `vi` | [Phase 5](phases/phase-5-longtail.md) |
| 压缩格式 | `bzip2`、`bunzip2`、`bzcat`、`xz`、`unxz`、`xzcat`、`lzma`、`unlzma`、`lzcat` | [Phase 5](phases/phase-5-longtail.md) |
| 模块管理 | `insmod`、`rmmod`、`lsmod`、`modprobe`、`depmod`、`modinfo` | [Phase 5](phases/phase-5-longtail.md) |
| 硬件 | `lspci`、`lsusb`、I2C 工具、flash/UBI 工具 | [Phase 5](phases/phase-5-longtail.md) |
| 服务管理 | runit 系列、`watchdog` | [Phase 5](phases/phase-5-longtail.md) |

`vi` 很重要，但不是 v1.0 生产可用的前置条件；网络、挂载、权限、日志和登录更早阻塞真实系统。

### 核心 50 命令深度目标

v1.0 应把以下命令作为核心兼容面：

`sh`、`test`、`echo`、`printf`、`cat`、`ls`、`cp`、`mv`、`rm`、`mkdir`、`rmdir`、`ln`、`readlink`、`realpath`、`touch`、`stat`、`chmod`、`chown`、`chgrp`、`find`、`xargs`、`grep`、`sed`、`awk`、`sort`、`uniq`、`cut`、`tr`、`head`、`tail`、`wc`、`tee`、`tar`、`cpio`、`gzip`、`gunzip`、`zcat`、`dd`、`mount`、`umount`、`chroot`、`stty`、`ps`、`kill`、`killall`、`pgrep`、`pidof`、`df`、`du`、`sync`

深度目标：

- 高频短选项必须与 BusyBox 兼容。
- GNU 长选项可作为增强，但不能破坏 BusyBox 行为。
- 多目标命令必须正确处理部分失败和最终退出码。
- 每个核心命令必须有 `--help`、示例、兼容性说明和 differential tests。

---

## Phase 0：轻量基建（与 Phase 1 并行）

Phase 0 采用轻量策略，不阻塞 Phase 1 的新功能开发。核心工作与 Phase 1 同步推进，边做边补。

### 必须完成（Phase 1 期间）

1. **修复已知文档漂移**：统一测试数量、体积口径、正则说明
2. **建立 differential test 骨架**：`tests/differential/run_diff.sh`，先覆盖 5 个核心命令
3. **编译零 warning**：每次提交 CI 绿（已做到，保持）

### 推迟到 Phase 4 的内容

以下内容在原路线图中属于 Phase 0B-0F，现调整为 Phase 4 的输入：

- 性能基线与 benchmark 框架（原 Phase 0B）
- 体积预算与 profile 裁剪系统（原 Phase 0C）
- IO 流式处理审计（原 Phase 0D）
- 安全与 fuzzing 入口（原 Phase 0E）
- CI 可复现构建（原 Phase 0F）

原有 Phase 0A-0F 的详细文档仍保留在 `phases/` 目录中，作为后续参考。

详见 [Phase 0A 文档](phases/phase-0a-baseline-inventory.md) 和 [执行策略调整说明](#执行策略调整v020-规划)。

---

## Phase 1：核心系统能力补齐

**进入条件**：v0.1.0 代码库，331 GTest + 56 集成测试全部通过，编译零 warning。Phase 0-lite 文档漂移修复已完成。

**退出条件**：8 个 P0 + 9 个 P1 + 7 个 P2 新 applet 实现并通过测试；测试总数达 500+ GTest + 80+ 集成测试；二进制体积保持在 550 KB 以内。

**执行起点**：从 `clear` 开始，验证 applet 注册流程（CMake → applets.hpp → 测试），然后按 Wave 1 → 4 推进。

**分波策略**：

| Wave | 命令 | 依赖 |
|------|------|------|
| Wave 1 | `clear`、`which`、`chgrp`、`chmod`、`chown`、`mountpoint` | 无 |
| Wave 2 | `killall`、`halt`/`reboot`/`poweroff`、`setsid`、`flock` | signal.hpp（从 kill.cpp 提取） |
| Wave 3 | `chroot`、`dd`、`stty`、`mount`、`umount` | mount.hpp、tty.hpp |
| Wave 4 | `sha1sum`/`sha256sum`/`sha384sum`/`sha512sum`、`base64`、`base32`、`zcat` | digest.hpp、base64.hpp |

**新增 P0 命令**：`chmod`、`chown`、`chgrp`、`chroot`、`dd`、`stty`、`mount`、`umount`

**新增 P1 命令**：`killall`、`halt`、`reboot`、`poweroff`、`flock`、`setsid`、`which`、`clear`、`mountpoint`

**新增 P2 命令**：`sha256sum`、`sha512sum`、`sha1sum`、`sha384sum`、`base64`、`base32`、`zcat`

**验收场景**：

- `chmod +x script.sh && ./script.sh` 可用。
- `dd if=/dev/zero of=test.img bs=1M count=10 status=progress` 正确。
- `mount -t proc proc /proc && umount /proc` 在 QEMU 中通过。
- `tar -czf rootfs.tar.gz rootfs && tar -xzf rootfs.tar.gz` 往返正确。

详见 [Phase 1 文档](phases/phase-1-core-system.md)。

---

## Phase 1.5：代码质量审查

**进入条件**：Phase 1 Wave 3 完成（核心系统命令 dd/stty/mount/umount 到位）。

**目标**：在深化现有命令之前，确保所有 P0 命令和核心代码达到统一质量标准。

**审查清单**：

1. **错误处理一致性**：所有 applet 使用 std::expected + CFBOX_TRY，错误信息格式统一
2. **代码风格统一**：clang-format 全量检查，命名一致性
3. **测试覆盖快照**：每个 P0 applet ≥1 GTest + ≥1 集成测试
4. **体积检查**：确认在 550 KB 以内
5. **兼容性记录**：每个 P0 applet 记录与 BusyBox 的已知差异

详见 [Phase 1.5 文档](phases/phase-1.5-code-quality-review.md)。

---

## Phase 2：核心命令深化

**进入条件**：Phase 1 完成 + Phase 1.5 质量审查通过。

**目标**：将现有命令功能深度从 ~30% 提升到 ~70%。

**优先排序**（按运维频率）：

| 批次 | 命令 | 关键补充 |
|------|------|---------|
| 第一批 | `tail`、`cp`、`test`、`ls` | tail -f、cp -a、全面 POSIX test、ls -R/--color |
| 第二批 | `grep`、`tar`、`sed`、`sort` | grep -A/-B/-C、tar -z/-v、sed -i、sort -k |
| 第三批 | `find`、`sh`、`ps`、`df`、`du` | find 布尔表达式、sh case/heredoc/函数 |

详见 [Phase 1 文档 Part 3](phases/phase-1-core-system.md#part-3-现有-applet-深化)。

---

## Phase 3：网络最小闭环

**进入条件**：Phase 2 核心深化完成。没有权限、挂载、`dd` 和深化后的 grep/sed/find 支持时，网络调试缺乏基础。

Phase 3 优先于 `vi`。没有网络配置和诊断能力，CFBox 无法成为生产系统中的 BusyBox 替代品。

**命令范围**：

- 基础配置：`ip`、`ifconfig`、`route`
- 状态查看：`netstat`
- 诊断：`ping`、`traceroute`、`nslookup`
- 传输与调试：`wget`、`nc`、`tftp`
- 主机名增强：`hostname -s`、`hostname -i`、`hostname -f`

**基础设施**：

- `socket.hpp`：TCP、UDP、Unix domain socket RAII 封装。
- `http.hpp`：最小 HTTP/1.1 client 解析。
- TLS 作为可选能力进入发布矩阵，不作为默认零依赖构建的一部分。

**验收场景**：

- QEMU 网络中 `ping -c 3` 可用。
- `wget http://example.com/` 可下载文件。
- `nc -l -p 8080` 与客户端互通。
- `netstat -tlnp` 可列出监听端口。

详见 [Phase 3 文档](phases/phase-2-network.md)。

---

## Phase 4：生产质量门禁深化

**进入条件**：Phase 3 网络闭环完成。Phase 0-lite 的基线入口运行稳定。

本阶段不以新增 applet 为主，而是把项目从"Phase 0 已建立基线"推进到"可发布、可回归、可审计"。Phase 0 已完成的 benchmark、differential、fuzz、size report 和 static analysis 入口应在本阶段扩大覆盖面、提高严格度并接入 release 工程。

**必交付内容**：

- differential test 框架：CFBox vs BusyBox vs GNU/Toybox。
- fuzzing：归档、压缩、shell/awk/sed 解析器、patch/diff。
- benchmark：吞吐、RSS、启动耗时、体积。
- static analysis：clang-tidy、cppcheck、sanitizers。
- replacement tests：initramfs、Alpine 容器、QEMU PID 1。
- release 工程：artifact、checksum、签名、SBOM、changelog、size report。

**验收场景**：

- `rescue` profile 在 QEMU initramfs 中完成挂载、文件恢复、网络诊断。
- `container` profile 可在 Alpine minirootfs 中运行常见 shell 脚本。
- 核心 parser fuzz target 无已知崩溃。
- 每个 release candidate 输出体积对比和兼容性差异报告。

详见 [Phase 4 文档](phases/phase-3-quality.md)。

---

## Phase 5：多用户与嵌入式运行时

**进入条件**：Phase 3 网络闭环和 Phase 4 基础质量门禁完成。

**命令范围**：

- 登录管理：`login`、`su`、`passwd`、`adduser`、`deluser`、`addgroup`、`delgroup`、`getty`、`sulogin`、`chpasswd`、`groups`
- 系统日志：`syslogd`、`klogd`、`logger`、`logread`
- 设备与存储：`mdev`、`blkid`、`lsblk`、`losetup`、`mkswap`、`swapon`、`swapoff`、`pivot_root`、`switch_root`

**基础设施**：

- `/etc/passwd`、`/etc/shadow`、`/etc/group` 解析库。
- syslog ring buffer 与日志持久化策略。
- mdev 规则解析和 hotplug 事件处理。

**验收场景**：

- 串口 getty 可启动 login。
- `logger` 写入 `syslogd`，`logread` 可读取。
- initramfs 可通过 `switch_root` 切换到真实 rootfs。

详见 [Phase 5 文档](phases/phase-4-multiuser.md)。

---

## Phase 6：长尾完备性

**进入条件**：Phase 5 完成。Phase 6 用于提升完整度，而不是定义 v1.0 的最低门槛。

**范围**：

- `vi`：可视化编辑器，至少支持打开、编辑、保存、搜索、替换和基础 ex 命令。
- 额外压缩格式：`bzip2`、`xz`、`lzma` 系列，默认可裁剪。
- 控制台与硬件：`reset`、`less`、`strings`、`tree`、`xxd`、`lspci`、`lsusb`、I2C 工具。
- 服务管理：`crond`、`crontab`、runit 相关命令。
- 模块管理：`insmod`、`rmmod`、`lsmod`、`modprobe`、`depmod`、`modinfo`。

**原则**：

- 长尾 applet 必须服从 profile 裁剪。
- 不为追求数量牺牲核心命令质量。
- 大型依赖或复杂格式必须提供体积预算和安全测试说明。

详见 [Phase 6 文档](phases/phase-5-longtail.md)。

---

## 质量工程总纲

CFBox 的生产化质量目标是：**行为可对比、回归可捕获、输入可抗攻击、发布可复现**。以下定义全局策略，各 Phase 文档包含具体实施细节。

### Differential Tests

建立 CFBox 与 BusyBox、GNU coreutils、Toybox 的行为对比测试。

优先覆盖：`sh/test/find/grep/sed/awk`、`cp/mv/rm/ln/chmod/chown/chgrp`、`tar/cpio/gzip/gunzip/zcat`、`dd/mount/umount/stty`、`ps/kill/df/du`。

测试输出区分三类结果：**完全一致**、**可接受差异**、**缺陷**。

差异分类和兼容性矩阵定义见 [Phase 0A](phases/phase-0a-baseline-inventory.md)，框架实施见 [Phase 3](phases/phase-3-quality.md)。裁决原则见 [兼容性策略](compatibility-policy.md)。

### POSIX 子集验证

v1.0 不以 Open Group VSX-PCTS 认证为阻塞项。推荐分层验证：

1. POSIX shell 和 coreutils 子集测试。
2. Open POSIX Test Suite 中可移植的相关子集。
3. 真实脚本语料：Alpine init scripts、Autoconf `configure`、常见安装脚本片段。

所有 POSIX 差异应进入兼容性矩阵。文档应使用"POSIX-like shell"、"POSIX-oriented behavior"等措辞，避免直接声称"POSIX compatible"。

### Fuzzing

P0 fuzz target：

- 压缩：inflate、deflate、gzip header/trailer。
- 归档：tar、cpio、ar、zip/unzip。
- 语言解析器：sh、awk、sed。
- patch/diff 输入。
- find 表达式解析。

框架策略：

- libFuzzer + ASan/UBSan 作为 CI 可运行目标。
- AFL++ 用于 nightly 或长期 corpus 演化。
- 所有崩溃样本进入回归 corpus。

Phase 0E 建立 fuzz smoke 入口，Phase 3 扩展为长期 corpus 和 coverage。详见 [Phase 0E](phases/phase-0e-safety-robustness-hardening.md) 和 [Phase 3](phases/phase-3-quality.md)。

### Coverage

覆盖率目标按风险分层，不追求单一全局数字：

| 类型 | 目标 |
|------|------|
| 基础库、parser、格式解析器 | 85% 行覆盖，关键分支 90%+ |
| P0 applet | 80%+ 行覆盖，并包含错误路径 |
| P1 applet | 70%+ 行覆盖 |
| 长尾 applet | 必须有基础 smoke test 和 help/version test |

覆盖率下降超过阈值时，PR 必须解释原因。

### Benchmark

建立稳定 benchmark 集，比较 CFBox、BusyBox、GNU/Toybox。

指标：吞吐（MB/s、lines/s、files/s）、峰值 RSS、启动耗时、二进制体积和每 applet 体积增量。

P0 benchmark：`cat`、`grep`、`sed`、`awk`、`sort`、`wc`、`find`、`tar`、`gzip`、`cp`、`dd`、`ls`。

具体场景、输出格式和回归阈值定义见 [Phase 0B](phases/phase-0b-performance-resource-baseline.md)。

### Static Analysis

门禁顺序：

1. ASan/UBSan：Debug 和 CI 必跑。
2. clang-tidy：先启用 `bugprone-*`、`performance-*` 中低噪声规则。
3. cppcheck：P1，覆盖 C++ 常见缺陷。
4. include-what-you-use：P2，用于长期头文件治理。

不应一次性开启高噪声规则导致大规模非功能改动。CI 集成细节见 [Phase 0F](phases/phase-0f-ci-repro-debuggability.md)。

### Replacement Tests

新增 `tests/replacement/` 类型测试，用于证明 profile 真实可替换。

场景：

- initramfs 只带 CFBox，在 QEMU 中启动并执行救援操作。
- Alpine minirootfs 注入 CFBox symlink，运行常见 shell 脚本。
- `container` profile 执行包安装脚本片段。
- `embedded` profile 启动 PID 1、挂载 proc/sys/dev、启动 syslog 和 getty。

实施见 [Phase 3](phases/phase-3-quality.md)。

### 特权命令治理

`mount`、`umount`、`chroot`、`dd`、`reboot`、`poweroff`、`mkfs`、`fsck`、`ip`、`login`、`su` 必须有额外要求：

- 单独错误路径测试。
- QEMU 或 namespace 隔离测试。
- 文档列出权限要求和容器限制。
- 错误信息包含操作、目标、errno 语义。
- 危险操作必须有清晰示例，避免误导用户。

详细审计和治理规范见 [Phase 0E](phases/phase-0e-safety-robustness-hardening.md)。

---

## 发布与分发

CFBox 的发布策略以嵌入式、容器和 rescue 场景为中心。musl static 是一等目标，glibc 构建用于桌面和发行版兼容。

### Release Artifact 矩阵

v1.0 必须发布：

| Artifact | 链接方式 | 用途 |
|----------|----------|------|
| `cfbox-x86_64-linux-musl` | static | 容器、initramfs、通用 Linux |
| `cfbox-aarch64-linux-musl` | static | ARM64 嵌入式、树莓派、边缘设备 |
| `cfbox-armv7-linux-musleabihf` | static | 32 位 ARM 嵌入式 |
| `cfbox-x86_64-linux-gnu` | dynamic 或 static | 桌面发行版和开发者测试 |

可选发布：`*-tls`（启用 mbedTLS 或等价 TLS 后端）、`*-debug`（带 debug symbols）、`*-rescue`/`*-container`/`*-embedded`（profile-specific artifact）。

### musl 优先级

musl static 为 P0：最适合 initramfs 和 scratch 容器；更接近 BusyBox 的常见部署方式；避免目标系统 libc 版本不匹配。glibc 仍保留，但不作为嵌入式主路径。

### 包管理优先级

| 优先级 | 包格式 | 理由 |
|--------|--------|------|
| P0 | APK / Alpine | 与 BusyBox 替换场景最贴近 |
| P1 | AUR / Arch | 开发者传播快，维护成本低 |
| P1/P2 | deb / Debian | 生态重要，但替换系统工具风险更高 |
| P3 | rpm | 可在 v1.x 后考虑 |

发行版包默认不应替换系统核心工具；应提供 opt-in symlink 安装方式。

### Release Checklist

每个 release candidate 必须包含：artifact 和 checksum、签名文件、changelog、SBOM、size report、测试矩阵、已知兼容性差异、applet 新增/删除/行为变化列表。

### Changelog

采用 Conventional Commits + `git-cliff`。Release notes 必须分组：Added applets、Changed behavior、Compatibility notes、Fixed bugs、Security、Tests、Size changes。

### Size Report

每个 release 输出 full/minimal/rescue/container/embedded profile 体积、与上一个 release 的增量、最大体积增长来源、与 BusyBox/Toybox 的参考对比。体积预算数值见 [v1.0 生产可用标准](v1-production-criteria.md)，口径定义见 [Phase 0C](phases/phase-0c-size-profile-budget.md)。

### TLS 策略

默认构建保持零运行时依赖。HTTPS 支持作为可选构建进入：`CFBOX_ENABLE_TLS=ON`、独立 `*-tls` artifact、文档明确 TLS 后端、证书路径、体积影响和安全支持边界。

`wget` 的 HTTP-only 能力可以先进入 Phase 5；HTTPS 不阻塞网络最小闭环，但阻塞"现代下载工具完整可用"的更高等级声明。

### 可复现与供应链

v1.0 前应建立：固定工具链版本记录、release 构建脚本、SBOM 生成、artifact checksum 和签名、第三方代码来源审计（避免 GPL 污染 MIT 项目）。构建元数据格式见 [Phase 0F](phases/phase-0f-ci-repro-debuggability.md)。
