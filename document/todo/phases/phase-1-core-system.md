# Phase 1: 核心系统能力补齐

## 概述

**目标**：补齐 rescue/embedded profile 的最低生产门槛。`chmod/chown/chgrp/chroot/dd/stty/mount/umount` 是 P0 阻断项；13 个核心命令需要从"基础可用"深化到"compatible"成熟度；`killall/halt/reboot/poweroff/flock/setsid/which/clear/mountpoint` 关闭系统控制缺口；`sha*/base64/base32/zcat` 补齐完整性校验和编码。

**进入条件**：
- Phase 0A-0F 全部完成，或每个未完成项都有明确豁免记录。
- v0.1.0 代码库，109 个 applet，331 GTest + 56 集成测试全部通过。
- 已建立 P0 benchmark/RSS/启动耗时基线，并能检测超过阈值的回归。
- 已建立 profile 体积预算和 size diff 口径，Phase 1 新增 applet 均有体积预算。
- 已完成全量读入、数值解析、执行边界和危险命令的初始审计。
- 兼容性裁决原则已建立，见 [兼容性策略](../compatibility-policy.md)。

**退出条件**：
- 8 个 P0 新 applet 实现并通过测试
- 9 个 P1 新 applet 实现并通过测试
- 7 个 P2 新 applet 实现并通过测试
- 13 个核心 applet 深化到 `compatible` 成熟度
- 测试总数达到 500+ GTest + 80+ 集成测试
- 二进制体积保持在 550 KB 以内（size-opt, LTO + strip）

**体积预算**：full profile static musl ≤ 550 KB（当前 ~446 KB，预算新增 ~104 KB）

---

## Part 1: 新增基础设施

### 1.1 `include/cfbox/signal.hpp` — 信号工具库

**动机**：`kill.cpp` 已有本地 `SIGNALS` 数组和 `signal_from_name()`。`killall`、`halt`、`reboot`、`poweroff`、`flock` 和 init shutdown 模块都需要相同的信号名-编号映射和信号发送原语。提取为共享头文件消除重复。

**接口设计**：

```cpp
namespace cfbox::signal {
    struct signal_entry {
        const char* name;  // "TERM", "KILL", "USR1" 等
        int num;
    };

    // 信号表，覆盖 SIGABRT~SIGUSR2 及 SIGWINCH 等
    constexpr auto signal_table() -> std::span<const signal_entry>;

    auto name_to_num(std::string_view name) -> int;       // 处理 SIG 前缀
    auto num_to_name(int num) -> std::string_view;
    auto send_signal(pid_t pid, int sig) -> int;           // 包装 kill()
    auto send_signal_all(int sig) -> int;                  // kill(0, sig)
    auto parse_signal_spec(std::string_view spec) -> int;  // -SIGTERM, -15, SIGTERM
    auto list_signals() -> void;                           // 格式化输出
}
```

**依赖**：无外部依赖。可 header-only 实现。从 `src/applets/kill.cpp` 中提取现有代码。

**测试要点**：`name_to_num("TERM") == 15`、`name_to_num("SIGTERM") == 15`、`name_to_num("15") == 15`、无效信号名返回 -1。

### 1.2 `include/cfbox/digest.hpp` — 流式摘要框架

**动机**：现有 `checksum.hpp` 提供 `crc32()`、`md5()` 但只处理完整内存输入。SHA-1/256/384/512 需要流式/分块接口，因为输入文件可能数 GB。`-c` 校验模式需要处理 N 个文件。

**接口设计**：

```cpp
namespace cfbox::digest {
    enum class Algorithm { SHA1, SHA256, SHA384, SHA512 };

    struct HashResult {
        std::array<uint8_t, 64> bytes{};  // 最大 SHA-512 = 64 bytes
        size_t hash_len = 0;
        auto hex_string() const -> std::string;
    };

    class StreamingHash {
    public:
        explicit StreamingHash(Algorithm algo);
        auto update(const uint8_t* data, size_t len) -> void;
        auto finalize() -> HashResult;
        auto reset() -> void;
    private:
        // 使用 variant 存储不同 SHA 算法的内部状态
        // SHA-256: 8×uint32_t + 64字节块缓冲 + 计数
        // SHA-512: 8×uint64_t + 128字节块缓冲 + 计数
    };

    // 便利函数
    auto hash_file(std::string_view path, Algorithm algo) -> base::Result<HashResult>;
    auto hash_bytes(std::string_view data, Algorithm algo) -> HashResult;
    auto check_digests(const std::string& checkfile, Algorithm algo) -> int;
}
```

**实现要点**：
- 纯 C++23 实现 FIPS 180-4，无外部依赖
- SHA-256 是首要目标，SHA-1/384/512 可复用相同框架
- `check_digests()` 解析 `sha256sum` 输出格式：`<hash>  <filename>` 或 `<hash> *<filename>`
- 四个 SHA applet 共享模板函数：

```cpp
template<digest::Algorithm Algo>
auto sha_sum_main(int argc, char* argv[]) -> int;

// 四个入口点
auto sha1sum_main(int argc, char* argv[]) -> int;
auto sha256sum_main(int argc, char* argv[]) -> int;
auto sha384sum_main(int argc, char* argv[]) -> int;
auto sha512sum_main(int argc, char* argv[]) -> int;
```

**测试要点**：NIST 已知测试向量、空输入、大数据分块、流式与一次性结果一致性。

### 1.3 `include/cfbox/base64.hpp` — Base 编码/解码

**动机**：`base64`/`base32` applet 需要编解码例程。Phase 2 的 HTTP Basic Auth 也需要。

**接口设计**：

```cpp
namespace cfbox::encoding {
    auto base64_encode(std::string_view input) -> std::string;
    auto base64_decode(std::string_view input) -> base::Result<std::string>;
    auto base32_encode(std::string_view input) -> std::string;
    auto base32_decode(std::string_view input) -> base::Result<std::string>;
    auto hex_encode(const uint8_t* data, size_t len) -> std::string;
    auto hex_decode(std::string_view hex) -> base::Result<std::vector<uint8_t>>;
}
```

**测试要点**：RFC 4648 测试向量、填充处理、无效输入拒绝、行换行、编解码往返。

### 1.4 `include/cfbox/mount.hpp` — 挂载/卸载工具

**动机**：`mount` 和 `umount` 共享选项解析、挂载标志构造和错误报告逻辑。`proc.hpp` 已有 `read_mounts()` 返回 `MountEntry`。本头文件提供挂载操作原语。

**接口设计**：

```cpp
namespace cfbox::mount {
    auto mount_fs(std::string_view device, std::string_view mountpoint,
                  std::string_view fstype, unsigned long flags,
                  std::string_view options) -> base::Result<void>;
    auto umount_one(std::string_view target) -> base::Result<void>;
    auto umount_lazy(std::string_view target) -> base::Result<void>;
    auto umount_force(std::string_view target) -> base::Result<void>;
    auto parse_mount_flags(std::string_view options) -> unsigned long;
    auto find_fstype(std::string_view device) -> base::Result<std::string>;
}
```

**系统头文件**：`<sys/mount.h>`、`<linux/fs.h>`

**测试要点**：`parse_mount_flags("ro,noexec")` 返回正确 `MS_RDONLY|MS_NOEXEC` 位组合。

### 1.5 `include/cfbox/tty.hpp` — 终端控制抽象

**动机**：`stty` 需要全面的 termios 操控。现有 `terminal.hpp` 提供 raw 模式和光标控制，但不暴露单个 termios 字段（波特率、奇偶校验、字符大小、流控等）。

**接口设计**：

```cpp
namespace cfbox::tty {
    struct TermiosSettings {
        speed_t input_speed = B0;
        speed_t output_speed = B0;
        unsigned csize = CS8;
        bool parity = false;
        bool parity_odd = false;
        bool stop_bits_two = false;
        bool hardware_flow = false;
        bool software_flow = false;
        cc_t intr = CTRL('C');
        cc_t quit = CTRL('\\');
        cc_t erase = 0177;
        cc_t kill = CTRL('U');
        cc_t eof = CTRL('D');
        // ... 其他 c_cc 条目
        tcflag_t input_flags = 0;
        tcflag_t output_flags = 0;
        tcflag_t local_flags = 0;
    };

    auto read_settings(int fd) -> base::Result<TermiosSettings>;
    auto write_settings(int fd, const TermiosSettings& s) -> base::Result<void>;
    auto print_settings(const TermiosSettings& s, bool all) -> void;
    auto speed_to_baud(speed_t s) -> unsigned long;
    auto baud_to_speed(unsigned long baud) -> speed_t;
}
```

---

## Part 2: 新增 Applet（按依赖排序分波）

每个 applet 的注册步骤一致：在 `applets.hpp` 添加 `CFBOX_ENABLE_*` 条件声明、在 `APPLET_REGISTRY` 添加条目、在 `cmake/Config.cmake` 的 `CFBOX_APPLETS` 列表添加、在 `applet_config.hpp.in` 添加 `#cmakedefine01`。

### Wave 1: 基础命令（无新基础设施依赖）

#### 2.1 `chmod` — `src/applets/chmod.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-R` | 递归修改 |
| `-v` | 详细输出 |
| `--reference=RFILE` | 使用 RFILE 的权限模式 |
| 符号模式 | `u+x`、`go-w`、`a=rX` |
| 八进制模式 | `755`、`4755` |

**实现要点**：
- 解析符号模式字符串为 `std::filesystem::perms` 操作
- 递归遍历使用 `std::filesystem::recursive_directory_iterator`
- 权限修改复用 `fs_util.hpp` 的 `permissions()`

**测试场景**：`chmod +x script.sh && ./script.sh` 可用；`chmod -R 755 dir/` 递归正确。

#### 2.2 `chown` — `src/applets/chown.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-R` | 递归修改 |
| `-v` | 详细输出 |
| `--reference=RFILE` | 使用 RFILE 的属主 |
| `OWNER:GROUP` | 同时修改属主和组 |
| `OWNER` | 只改属主 |
| `:GROUP` | 只改组 |

**实现要点**：
- 解析 `owner:group` 规范
- 通过 `<pwd.h>` `getpwnam()` 和 `<grp.h>` `getgrnam()` 解析 UID/GID
- 需要扩展 `fs_util.hpp`：添加 `chown(path, uid_t, gid_t)` 包装

#### 2.3 `chgrp` — `src/applets/chgrp.cpp`

**选项**：`-R`（递归）、`-v`（详细）

**实现要点**：薄包装 chown 逻辑，只修改组。`chgrp GROUP FILE` 等效于 `chown :GROUP FILE`。

#### 2.4 `clear` — `src/applets/clear.cpp`

**实现**：5 行 applet，调用 `terminal::clear_screen()` + `std::fflush(stdout)`。无选项。

#### 2.5 `which` — `src/applets/which.cpp`

**选项**：`-a`（打印所有匹配）

**实现**：拆分 `PATH`，遍历目录，通过 `access()` 检查 `X_OK`。

#### 2.6 `mountpoint` — `src/applets/mountpoint.cpp`

**选项**：`-q`（静默）、`-d`（打印设备 major:minor）

**实现**：`stat()` 参数，比较 `st_dev` 与 `proc.hpp` 的 `read_mounts()` 返回的挂载条目。

### Wave 2: 系统控制（依赖 `signal.hpp`）

#### 2.7 `killall` — `src/applets/killall.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-s SIG` | 指定信号 |
| `-q` | 静默 |
| `-e` | 精确匹配进程名 |
| `-I` | 忽略大小写 |

**依赖**：`signal.hpp`、`proc.hpp`（`read_all_processes()`）

**实现**：扫描所有进程，匹配 `comm` 或 `cmdline[0]`，发送信号。

#### 2.8 `halt` / 2.9 `reboot` / 2.10 `poweroff` — `src/applets/shutdown.cpp`

**推荐实现**：单文件 `shutdown.cpp` 导出三个入口点 `halt_main`、`reboot_main`、`poweroff_main`。

**共享选项**：
| 选项 | 说明 |
|------|------|
| `-f` | 强制（直接调用 reboot() 系统调用） |
| `-n` | 不 sync |
| `-w` | 只写 wtmp 记录 |

**实现要点**：
- 必须与现有 `init_shutdown.cpp` 的 `do_shutdown()` 逻辑协调
- halt: `reboot(RB_HALT_SYSTEM)`
- reboot: `reboot(RB_AUTOBOOT)`
- poweroff: `reboot(RB_POWER_OFF)`
- 非 `-f` 时：向 init 发送 SIGTERM

#### 2.11 `flock` — `src/applets/flock.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-s` | 共享锁 |
| `-x` | 排他锁（默认） |
| `-n` | 非阻塞 |
| `-w SECS` | 超时等待 |
| `-o` | exec 前关闭文件描述符 |

**两种模式**：
1. `flock FILE CMD...` — 获取锁，运行命令，释放
2. `flock FILE` — 获取锁，保持到 stdin 关闭

**系统调用**：`<sys/file.h>` 的 `flock()`

#### 2.12 `setsid` — `src/applets/setsid.cpp`

**选项**：`-c`（设置控制终端）

**实现**：调用 `setsid()`，然后 `execvp()` 命令。

### Wave 3: P0 阻断项（依赖新基础设施）

#### 2.13 `dd` — `src/applets/dd.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `if=FILE` | 输入文件（默认 stdin） |
| `of=FILE` | 输出文件（默认 stdout） |
| `bs=N` | 块大小 |
| `count=N` | 复制块数 |
| `skip=N` | 跳过输入块数 |
| `seek=N` | 跳过输出块数 |
| `status=progress\|noxfer\|none` | 进度显示 |
| `conv=notrunc\|noerror\|sync\|fsync` | 转换标志 |

**实现要点**：
- 使用 POSIX I/O（非 stdio）以获得性能
- 进度报告通过 SIGUSR1 信号处理器
- 单位解析：K、M、G 后缀
- 预估 ~200 行

**测试场景**：`dd if=/dev/zero of=test.img bs=1M count=10 status=progress` 正确。

#### 2.14 `stty` — `src/applets/stty.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-a` | 显示所有设置 |
| `-g` | 机器可读格式 |
| `-F DEVICE` | 指定设备 |
| termios 标志设置 | `ispeed`、`ospeed`、`csN`、`parity`、`raw`、`cooked`、`echo`、`-echo`、`clocal` 等 |

**依赖**：`tty.hpp`

**实现要点**：解析 termios 标志名，通过 `tty.hpp` 应用到 `tcgetattr`/`tcsetattr`。预估 ~300 行。

#### 2.15 `mount` — `src/applets/mount.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-t TYPE` | 文件系统类型 |
| `-o OPTIONS` | 挂载选项 |
| `-r` | 只读 |
| `-w` | 读写 |
| `-a` | 挂载 fstab 中所有 |
| `-n` | 不写 mtab |
| `-v` | 详细 |
| `--bind` | 绑定挂载 |
| `--move` | 移动挂载 |
| `--rbind` | 递归绑定 |

**依赖**：`mount.hpp`、`proc.hpp`

**实现要点**：
- 解析挂载选项为 `MS_*` 标志
- 至少支持 `-t proc/sysfs/tmpfs/devtmpfs/vfat/ext4`
- `-o loop`（回环挂载）可推迟到 Phase 4

**测试场景**：`mount -t proc proc /proc && umount /proc` 在 QEMU 中通过。

#### 2.16 `umount` — `src/applets/umount.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-a` | 卸载所有 |
| `-f` | 强制 |
| `-l` | 懒卸载 |
| `-n` | 不写 mtab |
| `-R` | 递归 |
| `-v` | 详细 |
| `-t TYPE` | 只卸载指定类型 |

**依赖**：`mount.hpp`、`proc.hpp`

**实现要点**：解析目标，尝试 `umount2()` 加标志，EBUSY 时回退到 `-l`。

#### 2.17 `chroot` — `src/applets/chroot.cpp`

**选项**：`USER`（以指定用户运行）、`GROUP`、`COMMAND`（可选参数）

**实现**：`chroot()` + `chdir("/")` + `execvp()`。省略命令时默认 `/bin/sh`。

### Wave 4: 完整性校验与编码（依赖 `digest.hpp` 和 `base64.hpp`）

#### 2.18-2.21 SHA 系列 — `src/applets/sha_sum.cpp`

**推荐**：单文件实现，四个入口点。

**选项**（所有 SHA 命令共享）：
| 选项 | 说明 |
|------|------|
| `-c FILE` | 校验模式 |
| `-b` | 二进制模式 |
| `-t` | 文本模式 |
| `--tag` | BSD 输出格式 |

**实现**：使用模板函数 `sha_sum_main<Algorithm>()`，每个入口点只指定算法。

#### 2.22 `base64` — `src/applets/base64.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-d` | 解码 |
| `-w N` | 换行列数（默认 76，0 不换行） |
| `-i` | 解码时忽略非 base64 字符 |

#### 2.23 `base32` — `src/applets/base32.cpp`

**选项**：同 base64。

#### 2.24 `zcat` — `src/applets/zcat.cpp`

**选项**：`-f`（强制，即使无 .gz 后缀）

**实现**：读取文件，通过 `compress.hpp` 的 `gzip_decompress()` 解压到 stdout。支持多文件。
考虑与 `gunzip.cpp` 共享逻辑 — 可以是别名入口指向同一函数。

---

## Part 3: 现有 Applet 深化

### 3.1 `sh` 深化 — `src/applets/sh/`（7 个文件）

| 新功能 | 涉及文件 | 说明 |
|--------|---------|------|
| `case` 语句 | `sh_parser.cpp` | `case WORD in PATTERN) COMMANDS ;; esac` |
| here-doc | `sh_lexer.cpp`、`sh_parser.cpp` | `<<EOF`、`<<-EOF`（带 tab 剥离） |
| 函数定义和调用 | `sh_parser.cpp`、`sh_executor.cpp` | `fname() { ... }`、`local` 变量 |
| 算术展开 `$((expr))` | `sh_expand.cpp` | 整数运算：`+`、`-`、`*`、`/`、`%`、`(``)`、比较 |
| `return` 内建 | `sh_builtins.cpp` | 函数内返回 |
| `break`/`continue` 带计数 | `sh_builtins.cpp` | `break N`、`continue N` |
| `read` 内建 | `sh_builtins.cpp` | `-p PROMPT`、`-r`、`-s`、`-n N`、`-t TIMEOUT` |
| `local` 内建 | `sh_builtins.cpp` | 函数作用域变量 |
| `trap` 内建 | `sh_builtins.cpp` | 信号处理注册 |

**每个新功能**：至少 3 个 GTest + 1 个集成测试。

### 3.2 `tar` 深化 — `src/applets/tar.cpp`

| 新选项 | 说明 |
|--------|------|
| `-z` | gzip 压缩/解压 |
| `-v` | 详细列表 |
| `--exclude=PATTERN` | 排除匹配文件 |
| `-p` | 提取时保留权限 |
| `-O` | 提取到 stdout |
| `-T FILE` | 从文件读取文件名列表 |
| `--strip-components=N` | 提取时剥离 N 层路径 |

**关键改动**：当前 tar 可能全量加载到内存。`-z` 支持需要流式解压。考虑分块处理大归档。

### 3.3 `find` 深化 — `src/applets/find.cpp`

| 新谓词/功能 | 说明 |
|-------------|------|
| `-iname PATTERN` | 大小写不敏感文件名匹配 |
| `-path PATTERN` | 完整路径匹配 |
| `-perm MODE` | 权限匹配：精确、`+MODE`（任意位）、`-MODE`（所有位） |
| `-mtime N` / `+N` / `-N` | 修改时间 |
| `-size N` | 文件大小 |
| `-user NAME` / `-group NAME` | 属主/组过滤 |
| `-not` / `!` / `(` `)` | 布尔表达式 |
| `-print0` | null 分隔输出 |
| `-delete` | 删除匹配文件 |
| `-newer FILE` | 比参考文件更新 |

**关键改动**：当前解析器是简单线性的。需要构建表达式树以支持布尔逻辑。这是 Phase 1 中最复杂的深化。

### 3.4 `grep` 深化 — `src/applets/grep.cpp`

| 新选项 | 说明 |
|--------|------|
| `-A N` | 匹配后 N 行上下文 |
| `-B N` | 匹配前 N 行上下文 |
| `-C N` | 匹配前后 N 行上下文 |
| `-F` | 固定字符串（不使用正则） |
| `-w` | 单词匹配 |
| `-f FILE` | 从文件读取模式 |
| `-H` | 始终打印文件名 |
| `-h` | 始终不打印文件名 |
| `-x` | 匹配整行 |

**关键改动**：上下文显示 (`-A/-B/-C`) 需要缓冲前 N 行。实现为环形缓冲区。

### 3.5 `sed` 深化 — `src/applets/sed.cpp`

| 新功能 | 说明 |
|--------|------|
| `y/src/dst/` | 字符转换 |
| `a\`、`i\`、`c\` | 追加、插入、替换行 |
| `q` | 退出命令 |
| `r FILE` | 读文件到流 |
| `w FILE` | 写模式空间到文件 |
| `-i[SUFFIX]` | 原地编辑（可选备份） |
| hold space (`h`/`H`/`g`/`G`/`x`) | 第二缓冲区操作 |
| `-E` | 扩展正则别名 |

**关键改动**：hold space 需要在 sed 执行引擎中添加第二缓冲区。

### 3.6 `cp` 深化 — `src/applets/cp.cpp`

| 新选项 | 说明 |
|--------|------|
| `-a` | 归档模式（= `-rpd`） |
| `-d` | 保留链接 |
| `-f` | 强制（必要时删除目标） |
| `-i` | 交互式确认覆盖 |
| `-l` | 硬链接代替复制 |
| `-s` | 符号链接代替复制 |
| `-u` | 更新模式（跳过更新的目标） |
| `-v` | 详细 |

**关键改动**：递归复制需要正确处理符号链接。

### 3.7 `tail` 深化 — `src/applets/tail.cpp`

| 新选项 | 说明 |
|--------|------|
| `-f` | 跟踪模式（监控追加数据） |
| `-F` | 跟踪 + 重试（日志轮转） |
| `-q` | 不打印文件头 |
| `-v` | 始终打印文件头 |
| `+N` | 从第 N 行开始 |

**关键改动**：`-f` 需要 `poll()`/`inotify` 循环。可复用 `tui.hpp` 的 `poll()` 使用模式。

### 3.8 `ps` 深化 — `src/applets/ps.cpp`

| 新选项 | 说明 |
|--------|------|
| `-o FORMAT` | 自定义输出格式 |
| `-p PID` | 指定 PID |
| `-u USER` | 用户过滤 |
| UID 解析 | `getpwuid()` 查找，显示用户名 |

**关键改动**：`-o` 需要格式解析器，映射说明符（pid/comm/args/%cpu/%mem 等）到列输出。

### 3.9 `df` 深化 — `src/applets/df.cpp`

| 新选项 | 说明 |
|--------|------|
| `-T` | 打印文件系统类型 |
| `-t TYPE` | 限制为指定类型 |
| `-x TYPE` | 排除指定类型 |
| `-i` | inode 信息 |
| `-P` | POSIX 输出格式 |

**关键改动**：`-i` 需要读取 `statvfs` 的 `f_files`/`f_ffree` 字段。

### 3.10 `sort` 深化 — `src/applets/sort.cpp`

| 新选项 | 说明 |
|--------|------|
| `-k KEYDEF` | 按字段排序（如 `-k2,2n`） |
| `-t SEP` | 字段分隔符 |
| `-h` | 人类数字排序（1K/2M 等） |
| `-M` | 月份排序 |
| `-R` | 随机排序 |
| `-u` | 去重 |
| `-C` / `-c` | 检查是否已排序 |

**关键改动**：`-k` 是最复杂的添加；需要解析字段定义如 `-k2,2n -k1,1`。

### 3.11 `du` 深化 — `src/applets/du.cpp`

| 新选项 | 说明 |
|--------|------|
| `-a` | 显示所有文件 |
| `-c` | 显示总计 |
| `-d N` / `--max-depth=N` | 限制深度 |
| `-L` | 跟随符号链接 |
| `-P` | 不跟随符号链接 |
| `--apparent-size` | 显示表观大小 |
| `--exclude=PATTERN` | 排除匹配 |

**关键改动**：当前可能只显示总计。需要逐子目录累积。

### 3.12 `test` 深化 — `src/applets/test.cpp`

| 新功能 | 说明 |
|--------|------|
| `-nt` / `-ot` / `-ef` | 文件比较 |
| 字符串比较 | `=`、`!=`、`<`、`>` |
| 算术比较 | `-eq`、`-ne`、`-gt`、`-ge`、`-lt`、`-le` |
| `-z STRING` / `-n STRING` | 空字符串/非空 |
| 文件类型测试 | `-b`、`-c`、`-d`、`-e`、`-f`、`-g`、`-G`、`-h`/`-L`、`-k`、`-O`、`-p`、`-r`、`-s`、`-S`、`-t`、`-u`、`-w`、`-x` |
| 分组 | `(` `)` |
| 逻辑 | `-a`（AND）、`-o`（OR） |

**关键改动**：按 POSIX test 规范全面审计。确保 `[` 变体正确解析 `]`。

### 3.13 `ls` 深化 — `src/applets/ls.cpp`

| 新选项 | 说明 |
|--------|------|
| `-R` | 递归列出 |
| `-i` | 显示 inode 号 |
| `-S` | 按大小排序 |
| `-t` | 按时间排序 |
| `-r` | 反转排序 |
| `-1` | 每行一个 |
| 颜色输出 | 使用 `term.hpp` |

---

## Part 4: 测试

### 4.1 单元测试（GTest）

新增测试文件 `tests/unit/`：

| 文件 | 测试内容 |
|------|---------|
| `test_signal.cpp` | 信号名/编号转换 |
| `test_digest.cpp` | SHA-1/256/384/512 NIST 测试向量、流式一致性、空输入、大输入 |
| `test_base64.cpp` | RFC 4648 测试向量、编解码往返、填充、行换行 |
| `test_chmod.cpp` | 符号模式解析、八进制模式、递归、--reference |
| `test_chown.cpp` | owner:group 解析、UID/GID 解析 |
| `test_dd.cpp` | 块大小解析、skip/seek、count、conv 标志 |
| `test_stty.cpp` | termios 标志解析、波特率名 |
| `test_mount.cpp` | 挂载标志构造 |
| `test_flock.cpp` | 锁获取、非阻塞失败、超时 |
| `test_find_deepen.cpp` | 布尔表达式、时间谓词、-print0 |
| `test_grep_deepen.cpp` | 上下文行、固定字符串、单词匹配 |
| `test_sed_deepen.cpp` | 字符转换、hold space、原地编辑 |
| `test_tar_deepen.cpp` | gzip 支持、排除、strip-components |
| `test_cp_deepen.cpp` | 归档模式、更新、符号链接 |
| `test_tail_deepen.cpp` | 跟踪模式、+N 语法 |
| `test_sh_case.cpp` | case 语句解析执行 |
| `test_sh_heredoc.cpp` | here-doc 和 tab 剥离 |
| `test_sh_functions.cpp` | 函数定义、local 变量、return |
| `test_sh_arithmetic.cpp` | $((expr)) 展开 |
| `test_sh_trap.cpp` | trap 内建 |

**目标**：331 + ~170 = **500+** GTest 用例。

### 4.2 集成测试（Shell 脚本）

新增 `tests/integration/`：

| 脚本 | 测试场景 |
|------|---------|
| `test_chmod.sh` | 八进制/符号模式、递归、错误 |
| `test_chown.sh` | 属主/组修改（需 root 或跳过） |
| `test_dd.sh` | 创建/验证测试镜像、skip/seek |
| `test_mount.sh` | mount/umount proc/tmpfs（需 namespace 或跳过） |
| `test_stty.sh` | 基础 termios 查询 |
| `test_sha256sum.sh` | 哈希文件、校验模式、多文件 |
| `test_base64.sh` | 编解码往返、行换行 |
| `test_grep_deepen.sh` | 上下文、固定字符串、单词匹配 |
| `test_find_deepen.sh` | 时间谓词、布尔表达式、`-print0 \| xargs -0` |
| `test_sort_deepen.sh` | 按字段排序、数字、人类数字 |
| `test_killall.sh` | 按名称杀进程 |
| `test_flock.sh` | 排他/共享锁行为 |
| `test_setsid.sh` | 新会话创建 |

**目标**：56 + ~25 = **80+** 集成测试。

### 4.3 差异测试

创建 `tests/differential/` 目录：

- **框架**：`tests/differential/run_diff.sh` — 运行 CFBox 和 BusyBox/GNU 同一命令，比较 stdout/stderr/退出码
- **三分归类**：完全一致、可接受差异、缺陷
- **Phase 1 目标 applet**：`chmod`、`chown`、`chgrp`、`dd`、`grep`、`sed`、`find`、`sort`、`cp`、`tail`、`test`、`sha256sum`、`base64`

### 4.4 特权隔离测试

创建 `tests/privileged/`（可通过环境变量跳过）：

- `test_mount_privileged.sh` — 在用户 namespace 中挂载
- `test_chroot.sh` — chroot 基本操作
- `test_halt.sh` — 验证信号发送（模拟）

---

## Part 5: 文档

### 每个 applet
- 标准 `HelpEntry`：选项、用法、示例
- 深化 applet 更新 HELP 常量

### 架构文档更新
- `document/architecture.md` 添加新头文件条目（signal、digest、base64、mount、tty）

### Cookbook 页面（为文档站准备）
- 使用 `chmod/chown/chgrp` 修复权限
- 使用 `dd` 创建和写入磁盘镜像
- 使用 `mount/umount` 挂载文件系统
- 使用 `sha256sum` 校验文件完整性
- 使用 `tar` 打包和解包 rootfs
- 使用 `find | xargs grep` 工作流

---

## Part 6: 兼容性

### POSIX 审计目标
| 命令 | POSIX 标准 | 关键审计点 |
|------|-----------|-----------|
| `chmod` | POSIX chmod | 符号模式语法完整性 |
| `test` | IEEE 1003.1 test | 所有条件操作符 |
| `dd` | POSIX dd | operand 语法 |
| `stty` | POSIX stty | termios 操作 |

### BusyBox 对齐
- 所有新 applet 短选项语义与 BusyBox 一致
- 输出格式对齐 BusyBox 风格
- 多文件部分失败退出码对齐

### 有意差异记录
- 记录不支持的 BusyBox 高频选项
- 记录不支持的 GNU 高频选项
- 记录已知行为差异
- 标注是否属于有意差异

### 退出码审计
首批审计对象：`cp`、`mv`、`rm`、`find`、`tar`、`dd`、`mount`、`umount`
- 全部成功：0
- 部分失败：非 0，继续处理后续目标
- 参数错误：与 BusyBox/POSIX 对齐
- 系统调用失败：错误信息包含目标和 errno 语义

---

## Part 7: 发布

### CMake 更新
- `cmake/Config.cmake` 的 `CFBOX_APPLETS` 列表添加 24 个新 applet
- `applet_config.hpp.in` 添加 24 个 `#cmakedefine01` 条目

### Profile 定义
| Profile | 包含 applet |
|---------|-----------|
| `rescue` | core P0 + sh + tar + gzip + grep + find + sed + mount/umount + dd + chmod + chown + stty |
| `container` | rescue + sort + awk + xargs + ps + kill + killall + sha256sum + base64 + which |
| `embedded` | container + halt/reboot/poweroff + setsid + flock |

### 体积影响评估
| 新增基础设施 | 预估体积增量 |
|-------------|-------------|
| signal.hpp | ~2 KB |
| digest.hpp (SHA-256) | ~8 KB |
| digest.hpp (SHA-1/384/512) | ~4 KB |
| base64.hpp | ~3 KB |
| mount.hpp | ~2 KB |
| tty.hpp | ~3 KB |
| 24 个新 applet | ~40 KB |
| 深化改动 | ~15 KB |

---

## Part 8: 依赖图与里程碑

### 依赖关系

```
基础设施
├── signal.hpp ────── Wave 2 (killall, halt, reboot, poweroff, flock)
├── digest.hpp ────── Wave 4 (sha1/256/384/512sum)
├── base64.hpp ────── Wave 4 (base64, base32)
├── mount.hpp ─────── Wave 3 (mount, umount)
└── tty.hpp ───────── Wave 3 (stty)

Wave 1 (无依赖) ── chmod, chown, chgrp, clear, which, mountpoint
Wave 2 ──────────── 依赖 signal.hpp
Wave 3 ──────────── 依赖 mount.hpp, tty.hpp
Wave 4 ──────────── 依赖 digest.hpp, base64.hpp

深化工作可在 Wave 1-4 期间并行推进
```

### 里程碑

| 里程碑 | 时间 | 内容 |
|--------|------|------|
| M1 | 第 1-2 周 | 基础设施头文件（signal、digest、base64）+ 单元测试 |
| M2 | 第 3-4 周 | Wave 1 applet（chmod/chown/chgrp/clear/which/mountpoint）+ 集成测试 |
| M3 | 第 5-6 周 | Wave 2+3 applet（killall/halt/reboot/poweroff/flock/setsid + dd/stty/mount/umount/chroot）|
| M4 | 第 7-8 周 | 核心深化（sh: case/heredoc/函数/算术；tar: gzip/verbose/exclude；find: 布尔表达式）|
| M5 | 第 9-10 周 | 剩余深化（grep/sed/cp/tail/ps/df/sort/du/test/ls） |
| M6 | 第 11-12 周 | Wave 4 applet（sha*/base64/base32/zcat）+ 差异测试 + 体积优化 + **Release v0.2.0** |

---

## 相关文档

- [生产化路线图](../production-roadmap.md) — 全局阶段顺序和优先级
- [兼容性策略](../compatibility-policy.md) — POSIX/BusyBox/GNU 行为裁决
- [v1.0 生产可用标准](../v1-production-criteria.md) — 最终发布门槛
- [Phase 0 前置门禁](phase-0a-baseline-inventory.md) — Phase 0A-0F 执行细节
