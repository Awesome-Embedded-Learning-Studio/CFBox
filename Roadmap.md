# CFBox 路线图：从玩具到真正可用的 BusyBox 替代品

## Context

CFBox 是一个 C++23 BusyBox 替代品，当前版本有 78 个 applet。项目使用注册表分发模式（`APPLET_REGISTRY`）、`std::expected` 错误处理、自定义参数解析器，CI 覆盖原生构建、交叉编译和 QEMU 测试。

**目标**：全面对齐 BusyBox，覆盖嵌入式、容器、救援和通用场景。Shell 是最关键的组件，必须最先实现。

**差异化**：C++23 现代特性、现代化 UX（彩色输出）、更好的可扩展性、严格 POSIX 兼容。

---

## 阶段总览

| 阶段 | 主题 | 新增 Applet | 核心基础设施 | 累计 |
|------|------|------------|-------------|------|
| 0 | 构建系统现代化 ✅ | 0 | CMake 配置、help 系统、UTF-8、彩色输出 | 17 |
| 1 | POSIX Shell + Coreutils I ✅ | ~17 | Shell 引擎、进程管理、信号处理 | ~34 |
| 2 | Coreutils II + findutils ✅ | ~44 | 流处理管线、校验和框架 | ~78 |
| 3 | 归档 + 压缩 + 文本处理 ✅ | ~15 | 终端抽象、压缩框架 | ~93 |
| 4 | vi 可视化编辑器 | 1 | TUI 框架、屏幕渲染、键盘映射 | ~94 |
| 5 | 进程/Init + util-linux | ~38 | /proc 解析器 | ~132 |
| 6 | 网络 + 登录 + 日志 | ~35 | Socket 抽象、HTTP 解析、shadow 密码 | ~167 |
| 7 | 剩余组件 + 集成验证 | ~40+ | POSIX 验证、容器替换测试 | ~200+ |

**当前状态**：Phase 0-3 已完成，93 个 applet，259 单元测试 + 54 集成测试全部通过。

---

## Phase 0：构建系统现代化 ✅

**目标**：为 200+ applet 做好构建基础，建立共享基础设施。

### 基础设施工作

1. **CMake 配置系统** ✅：`cmake/Config.cmake` + `include/cfbox/applet_config.hpp.in`，每个 applet 有 `CFBOX_ENABLE_<APPLET>` 选项（默认 ON），通过 `configure_file()` 生成 `applet_config.hpp`。`APPLET_REGISTRY` 中用 `#if CFBOX_ENABLE_ECHO` 守卫。提供预设 profile（`embedded`/`desktop`/`minimal`/`full`）。

2. **长选项支持** ✅：`args.hpp` 支持 `--recursive`、`--help`、`--version` 等 GNU 风格长选项，完全向后兼容现有短选项 API。

3. **内置 help 系统** ✅：`include/cfbox/help.hpp`，所有 17 个 applet 支持格式化的 `--help` 输出和 `--version`。

4. **彩色输出** ✅：`include/cfbox/term.hpp`，ANSI 颜色辅助（尊重 `NO_COLOR` 环境变量）。

5. **UTF-8 工具** ✅：`include/cfbox/utf8.hpp`，Unicode 感知的字符计数和宽度计算。

### 验证
- `cmake -DCFBOX_ENABLE_GREP=OFF` 编译成功且不包含 grep ✅
- `./cfbox echo --help` 输出格式化帮助 ✅
- 现有 17 个 applet 全部测试通过 ✅
- 149 个单元测试 + 17 个集成测试脚本全部通过 ✅

---

## Phase 1：POSIX Shell + Coreutils 第一批 ✅

**目标**：实现交互式 POSIX Shell——这是在真实 Linux 上使用的基础。同时实现简单的 coreutils 建立势头。

### Coreutils 第一批 ✅

16 个简单 coreutils 已完成，覆盖脚本基础所需：

`basename` ✅, `dirname` ✅, `true` ✅, `false` ✅, `yes` ✅, `sleep` ✅, `pwd` ✅, `tty` ✅, `uname` ✅, `whoami` ✅, `hostname` ✅, `id` ✅, `logname` ✅, `nproc` ✅, `test` ✅, `link` ✅

### POSIX Shell ✅

Shell 已实现为第一个多文件 applet（`src/applets/sh/`，8 个模块，~2400 行）：

| 模块 | 文件 | 状态 |
|------|------|------|
| 共享类型 | `sh.hpp` | ✅ AST 节点、Token、ShellState |
| 词法分析 | `sh_lexer.cpp` | ✅ 分词、引号、$展开、运算符 |
| 语法解析 | `sh_parser.cpp` | ✅ 递归下降：管道、列表、if/while/for、子shell、花括号组 |
| 执行器 | `sh_executor.cpp` | ✅ fork/exec/pipe、重定向 < > >>、内置命令 |
| 内建命令 | `sh_builtins.cpp` | ✅ echo/cd/pwd/export/exit/set/unset/shift/read/eval/source |
| 词展开 | `sh_expand.cpp` | ✅ $VAR、${VAR}、$?、$$、$#/$@/$*、命令替换、波浪号、glob |
| 变量系统 | `sh_vars.cpp` | ✅ 变量存储、环境同步、位置参数、特殊参数 |

**已实现功能**：管道 `|`、重定向 `< > >>`、`&&` `||`、`if/elif/else/fi`、`while/until/do/done`、`for/in/do/done`、子shell `()`、花括号组 `{}`、变量赋值、变量展开、命令替换 `$(...)`、tilde 展开、glob 展开、15 个内置命令。

**待后续迭代**：here-document `<<`、函数定义、算术展开 `$(( ))`、行编辑（readline 风格）、作业控制（jobs/fg/bg）、`case/esac`。

### 验证
- `./cfbox sh -c "echo hello | wc -l"` 管道工作
- `./cfbox sh -c "for i in 1 2 3; do echo $i; done"` 循环工作
- 交互模式：行编辑、历史、Tab 补全
- 作业控制：后台进程、fg/bg、Ctrl+Z
- 所有新 coreutils 通过单元 + 集成测试

---

## Phase 2：Coreutils 第二批 + findutils/xargs ✅

**目标**：完成剩余 coreutils，添加 xargs，建立流处理管线基础设施。

### 基础设施 ✅

1. **流管线** `include/cfbox/stream.hpp` ✅：`for_each_line()`、`split_fields()`、`split_whitespace()`、`LineProcessor` 虚基类 + `run_processor()`。

2. **校验和** `include/cfbox/checksum.hpp` ✅：CRC-32（POSIX）、MD5（RFC 1321 完整实现）、BSD/SysV `sum`。

3. **fs_util.hpp 扩展** ✅：`create_symlink()`、`read_symlink()`、`canonical()`、`resize_file()`、`space()`。

### Coreutils 第二批 ✅（44 个 applet）

**文本处理** ✅：`date`, `env`, `printenv`, `seq`, `tee`, `touch`, `tr`, `fold`, `expand`, `nl`, `paste`, `cut`, `comm`, `cksum`, `md5sum`, `sum`, `shuf`, `factor`, `tac`, `od`, `split`

**系统/文件** ✅：`timeout`, `nice`, `nohup`, `expr`, `hostid`, `install`, `readlink`, `realpath`, `rmdir`, `unlink`, `truncate`, `tsort`, `ln`, `mktemp`, `mkfifo`, `mknod`, `du`, `df`, `stat`, `sync`, `usleep`, `who`

### findutils ✅
- **xargs** ✅：从 stdin 构建参数列表执行命令，支持 `-n`, `-I`, `-0`, `-r`, `-t`

### 验证 ✅
- `echo -e "main.cpp\nCMakeLists.txt" | cfbox xargs -n1 echo` 端到端工作 ✅
- `cfbox date +%Y`、`cfbox seq 3`、`echo hello | cfbox tr a-z A-Z` 行为正确 ✅
- `cfbox factor 42` → `42: 2 3 7` ✅
- `cfbox expr 2 + 3` → `5` ✅
- `echo "a:b:c" | cfbox cut -d: -f2` → `b` ✅
- `echo -n "test" | cfbox md5sum -` → `098f6bcd4621d373cade4e832627b4f6` ✅
- 所有新 applet 通过 --help / --version ✅
- **246 单元测试** 全部通过 ✅
- **49 集成测试** 全部通过 ✅

---

## Phase 3：归档 + 压缩 + 文本处理 ✅

**目标**：实现让 CFBox 能够独立进行系统管理的重量级组件。

### 文本处理
- **awk**（`src/applets/awk/`，~1266 行）✅：完整 AWK 实现——词法/语法/解释器，模式-动作、BEGIN/END、字段、关联数组、printf、用户函数、正则匹配、内置函数（length/substr/split/gsub/match/sprintf）。
- **diff** ✅ + **cmp** ✅：LCS 文件比较，支持 normal 和 unified (-u) 格式
- **patch** ✅：支持 normal 和 unified diff 输入，diff | patch 往返验证通过
- **ed** ✅：行编辑器，支持 a/i/d/p/n/w/q/$ 及带文件名的 w 命令

### 归档
- **tar** ✅（`src/applets/tar.cpp`，~180 行）：创建/提取/列出，ustar 格式，支持 `-C` 目录切换、目录递归
- **cpio** ✅：newc 格式，initramfs 就绪
- **ar** ✅：静态库管理，创建/列出/提取
- **unzip** ✅：ZIP 提取，支持 stored 和 deflated 方法

### 压缩
- **gzip/gunzip** ✅：基于 zlib 的 DEFLATE 压缩/解压，文件和 stdin/stdout 双模式

### 基础设施 ✅
- **终端抽象** `include/cfbox/terminal.hpp` ✅：RawMode RAII、终端大小检测、光标控制、备用屏幕缓冲区、视频属性
- **压缩框架** `include/cfbox/compress.hpp` ✅：gzip 压缩/解压接口

### 验证 ✅
- `echo "hello" | gzip | gunzip` 往返正确 ✅
- `tar -cf archive.tar -C DIR files && tar -xf archive.tar` 创建/提取/列出正确 ✅
- `awk '{print $1}' file` / `awk '{s+=$1}END{print s}'` 输出正确 ✅
- `diff file1 file2 | patch file1` 往返正确（normal 和 unified 格式） ✅
- 259 单元测试 + 54 集成测试全部通过 ✅

---

## Phase 4：vi 可视化编辑器

**目标**：实现完整的 vi 可视化编辑器——CFBox 中最复杂的单一组件，需要独立的终端交互框架。

### 核心功能
- **命令模式**：光标移动（h/j/k/l/w/b/e/G/gg/0/$）、删除（x/dd/dw/d$）、复制粘贴（yy/p/P）、撤销/重做（u/Ctrl-R）
- **插入模式**：i/a/I/A/o/O 进入，Esc 退出，文本输入
- **ex 命令行**：:w/:q/:wq/:q!/:e/:r/:%s/:%g/:Number/:%!（外部命令过滤）
- **搜索替换**：/pattern 向前搜索、?pattern 向后搜索、n/N 重复、:%s/old/new/g 替换
- **可视化模式**：v 字符选择、V 行选择、基于选择的操作

### 屏幕渲染
- **双缓冲区渲染**：维护逻辑屏幕和物理屏幕，diff 驱动最小更新
- **状态栏**：底行显示文件名、光标位置、模式指示、修改标记
- **滚动**：半页/整页滚动（Ctrl-D/Ctrl-U/Ctrl-F/Ctrl-B），长行折行显示

### 基础设施
- **TUI 框架** `include/cfbox/tui.hpp`：全屏终端应用抽象，vi/top/less 共用
  - 屏幕缓冲区管理、增量渲染
  - 键盘事件映射（普通键 + 转义序列解析）
  - 信号处理（SIGWINCH 终端大小变化）

### 文件结构
```
src/applets/vi/
├── vi.hpp           # 共享类型、ViState、模式枚举
├── vi_buffer.cpp    # 文本缓冲区管理（行存储、插入/删除、撤销栈）
├── vi_render.cpp    # 屏幕渲染、diff 驱动更新
├── vi_normal.cpp    # 命令模式处理
├── vi_insert.cpp    # 插入模式处理
├── vi_ex.cpp        # ex 命令行解析和执行
├── vi_search.cpp    # 搜索替换引擎
└── vi_main.cpp      # 入口、事件循环
```

### 验证
- vi 能打开文件、编辑文本、保存退出
- 自动化测试：通过管道注入按键序列 + 验证输出文件
- 多文件编辑、大文件（10MB+）性能可接受
- 终端大小变化（SIGWINCH）正确响应

---

## Phase 5：进程管理 + Init 系统 + util-linux

**目标**：构建让 CFBox 适合作为完整 init 环境的系统级工具。

### Init 系统（完整）
- **inittab 解析器**：解析 `/etc/inittab`，支持运行级别、respawn、once 条目
- **运行级别管理**：sysinit, boot, single-user, multi-user
- **服务监控**：进程监控 + respawn 能力
- **关机/重启**：SIGTERM → 等待 → SIGKILL → sync → 卸载 → 重启
- **getty 集成**：在 TTY 上生成登录提示

### procps（解析 `/proc` 文件系统）
`ps`, `top`, `kill`, `free`, `uptime`, `pgrep`/`pkill`, `pidof`, `pmap`, `iostat`, `lsof`, `watch`, `sysctl`, `pstree`, `fuser`, `pwdx`

### util-linux
**存储/块设备**：`mount`/`umount`, `blkid`, `blockdev`, `dmesg`, `fdisk`, `mkfs`, `fsck`, `losetup`, `pivot_root`, `switch_root`, `swapon`/`swapoff`

**系统工具**：`hexdump`, `more`, `flock`, `getopt`, `cal`, `rev`, `setsid`, `nsenter`, `unshare`, `mdev`, `lspci`, `lsusb`, `hwclock`, `rtcwake`, `taskset`, `chrt`, `ionice`, `renice`, `last`, `mesg`, `wall`, `script`

### 基础设施
- **`/proc` 解析器** `include/cfbox/proc.hpp`：集中解析 /proc/meminfo, /proc/stat, /proc/[pid]/stat 等
- TUI 框架已在 Phase 4 实现，top/less 可直接复用

### 验证
- CFBox 作为 PID 1 在 QEMU 中启动，运行 inittab，生成 getty，处理关机
- `ps aux` 输出与 procps 格式匹配
- 容器测试：CFBox 替换 Alpine 容器中的 BusyBox，`docker run` 成功

---

## Phase 6：网络 + 登录管理 + 系统日志

**目标**：添加网络工具和用户/会话管理，使 CFBox 适用于网络启动系统和多用户环境。

### 网络工具
**基础网络**：`ifconfig`, `ip`, `route`, `hostname`, `ping`, `traceroute`

**文件传输**：`wget`, `nc` (netcat), `tftp`

**网络服务器**：`httpd`, `ftpd`, `telnetd`, `inetd`

**DHCP/DNS**：DHCP 客户端/服务端 (`udhcpc`/`udhcpd`), `nslookup`, `dnsd`, `whois`

### 登录管理
`login`, `su`, `passwd`, `adduser`, `deluser`, `addgroup`, `delgroup`, `getty`, `sulogin`, `vlock`, `chpasswd`

### 系统日志
`syslogd`, `klogd`, `logger`, `logread`

### 基础设施
- **Socket 抽象** `include/cfbox/socket.hpp`：TCP/UDP/Unix domain socket RAII 封装
- **HTTP 解析器** `include/cfbox/http.hpp`：最小 HTTP/1.1 解析
- **shadow 密码** `include/cfbox/shadow.hpp`：安全解析 /etc/passwd, /etc/shadow, /etc/group

### 验证
- `ping -c 3 8.8.8.8` 工作（QEMU 网络桥接）
- `wget http://example.com/` 下载成功
- `nc -l -p 8080` 与 `nc localhost 8080` 通信成功
- `login` 通过 `/etc/shadow` 认证
- `syslogd` 收集 `logger` 发送的日志

---

## Phase 7：剩余组件 + 全面集成验证

**目标**：完成所有剩余 BusyBox applet 类别，进行 POSIX 合规验证和集成测试。

### 剩余类别
- **modutils**：`insmod`, `rmmod`, `lsmod`, `modprobe`, `depmod`, `modinfo`
- **runit**：`runsv`, `sv`, `svlogd`, `runsvdir`, `chpst`
- **console-tools**：`chvt`, `clear`, `deallocvt`, `reset`, `setconsole`, `loadfont`, `loadkmap`, `openvt`, `fgconsole`, `showkey`
- **debianutils**：`which`, `run_parts`, `start_stop_daemon`, `pipe_progress`
- **e2fsprogs**：`chattr`, `lsattr`, `tune2fs`
- **miscutils**：`strings`, `less`, `man`, `time`, `tree`, `bc`, `dc`, `crond`, `crontab`, `watchdog`, `hdparm`, `beep`, `devmem`, `inotifyd`, `microcom`, `chat`

### 最终集成验证

1. **POSIX 合规测试**：运行 Open Group POSIX 测试套件（或子集）验证所有 applet
2. **容器替换测试**：在 Alpine/Ubuntu 最小容器中用 CFBox 替换 BusyBox，运行完整冒烟测试
3. **initramfs 救援测试**：构建仅包含 CFBox 的 initramfs，在 QEMU 中启动执行救援操作
4. **真实硬件测试**：在树莓派（aarch64）上启动
5. **体积报告**：对比不同配置（full/minimal/shell-only）与 BusyBox 的体积
6. **musl 支持**：添加 musl libc 交叉编译支持，CI 同时测试 glibc 和 musl
7. **文档完善**：所有 applet 的 `--help`，更新 README 和架构文档

### 验证
- 所有 ~200 applet 编译通过且测试通过
- 容器替换测试通过
- initramfs 救援测试通过
- POSIX 合规文档化
- 二进制体积满足嵌入式使用

---

## 风险与缓解

| 风险 | 影响 | 缓解措施 |
|------|------|---------|
| **Shell 复杂度** | 最高——5000+ 行代码 | 增量构建：先非交互 → 行编辑 → 作业控制，从第一天开始测试 POSIX shell 测试套件 |
| **AWK 复杂度** | 高——第二复杂组件 | 从 POSIX awk 子集开始，实现解释器而非编译器 |
| **vi 复杂度** | 高——终端处理微妙、独立 Phase 4 | 使用 Phase 3 的终端抽象 + 自建 TUI 框架，自动化按键注入测试，双缓冲 diff 驱动渲染 |
| **二进制体积膨胀** | 中——200+ applet | Phase 0 的 CMake 配置允许裁剪，LTO 和死代码消除，每阶段监控体积 |
| **跨平台边界情况** | 中——ioctl、/proc 格式差异 | 平台特性抽象到 `include/cfbox/` 头文件，每阶段三平台测试 |
| **网络安全** | 中——wget/httpd 需要 TLS | 先做 HTTP-only，HTTPS 通过可选 mbedTLS 依赖，作为 CMake 选项 |
| **大规模测试覆盖** | 高——200+ applet | 每个 applet 添加时即包含单元+集成测试，阶段完成需全部通过 |

---

## 关键架构决策

1. **简单 applet 单文件**（`src/applets/`），复杂 applet（shell, awk, vi, tar, fdisk）用子目录
2. **共享基础设施放头文件**（`include/cfbox/`），遵循现有 io.hpp/fs_util.hpp 模式
3. **`std::expected` 错误处理**：继续使用 CFBOX_TRY 模式
4. **注册表分发**：`APPLET_REGISTRY` 通过 `#if` 配置守卫增长
5. **无外部运行时依赖**：静态链接目标，可选编译时依赖（lzma, zstd, mbedtls）

## 关键文件

- `include/cfbox/applets.hpp`——注册表从 17 增长到 78（目标 200+）
- `include/cfbox/args.hpp`——扩展长选项支持
- `include/cfbox/error.hpp`——所有 applet 的错误处理基础
- `include/cfbox/stream.hpp`——流处理管线（逐行处理、字段分割）
- `include/cfbox/checksum.hpp`——校验和框架（CRC-32、MD5、BSD/SysV sum）
- `include/cfbox/fs_util.hpp`——文件系统工具（symlink、canonical、resize 等）
- `CMakeLists.txt`——从简单构建演进为配置化选择
- `src/applets/init.cpp`——从简单 init 扩展为完整 init 系统
