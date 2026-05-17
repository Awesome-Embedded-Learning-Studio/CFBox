# Phase 4: 多用户与嵌入式运行时

## 概述

**目标**：使 CFBox 能充当完整的嵌入式运行时，支持多用户登录、系统日志、设备管理和存储操作。本阶段关闭"工具集"与"系统运行时"之间的差距。

**进入条件**：Phase 3 完成。Release candidate 构建通过所有测试。

**前置引用**：兼容性裁决原则见 [兼容性策略](../compatibility-policy.md)；全局阶段顺序和优先级见 [生产化路线图](../production-roadmap.md)。

**退出条件**：
- login/getty/su/passwd 在 QEMU 串口控制台工作
- syslogd/logger/logread 往返正确
- switch_root 在 initramfs-to-rootfs 转换中工作
- mdev 在内核 uevent 上创建设备节点
- embedded profile 二进制体积 ≤ 800 KB

**体积预算**：embedded profile static musl ≤ 800 KB（Phase 3 后 ~650 KB，预算新增 ~150 KB）

---

## Part 1: 新增基础设施

### 1.1 `include/cfbox/userdb.hpp` — 用户/组数据库

**动机**：`login`、`su`、`passwd`、`getty`、`chpasswd`、`groups`、`adduser`、`deluser`、`addgroup`、`delgroup` 都需要解析和验证 `/etc/passwd`、`/etc/shadow`、`/etc/group`。共享模块提供数据结构和解析器。

**接口设计**：

```cpp
namespace cfbox::userdb {
    struct PasswdEntry {
        std::string name;
        std::string password;  // 'x' if shadow
        uid_t uid = 0;
        gid_t gid = 0;
        std::string gecos;
        std::string home;
        std::string shell;
    };

    struct ShadowEntry {
        std::string name;
        std::string hash;      // $id$salt$hash
        int last_change = 0;   // days since epoch
        int min_days = 0;
        int max_days = 99999;
        int warn_days = 7;
        int inactive_days = -1;
        int expire_date = -1;
    };

    struct GroupEntry {
        std::string name;
        std::string password;
        gid_t gid = 0;
        std::vector<std::string> members;
    };

    // 解析
    auto parse_passwd() -> base::Result<std::vector<PasswdEntry>>;
    auto parse_shadow() -> base::Result<std::vector<ShadowEntry>>;
    auto parse_group() -> base::Result<std::vector<GroupEntry>>;

    // 查询
    auto get_passwd_entry(std::string_view name) -> base::Result<PasswdEntry>;
    auto get_passwd_entry(uid_t uid) -> base::Result<PasswdEntry>;
    auto get_group_entry(std::string_view name) -> base::Result<GroupEntry>;
    auto get_group_entry(gid_t gid) -> base::Result<GroupEntry>;

    // 密码操作
    auto verify_password(std::string_view username, std::string_view password)
        -> base::Result<bool>;
    auto update_password(std::string_view username, std::string_view new_hash)
        -> base::Result<void>;
    auto hash_password(std::string_view password) -> std::string;  // SHA-256 crypt ($5$)

    // 组查询
    auto user_groups(std::string_view username) -> base::Result<std::vector<gid_t>>;
}
```

**依赖**：`digest.hpp`（密码哈希）、`<crypt.h>` 或自实现 SHA-256 crypt。

**安全要点**：
- 验证密码后必须清零内存中的明文密码
- 失败登录必须引入延迟（防暴力破解）
- 不在日志中记录密码

### 1.2 `include/cfbox/syslog.hpp` — Syslog 数据结构

**动机**：`syslogd`、`logger`、`logread` 共享 ring buffer 格式和通信机制。

**接口设计**：

```cpp
namespace cfbox::syslog {
    struct LogEntry {
        int facility = 0;
        int severity = 6;  // LOG_INFO
        std::string timestamp;
        std::string hostname;
        std::string tag;     // 程序名
        std::string message;
    };

    // 格式化/解析
    auto format_rfc3164(const LogEntry& e) -> std::string;
    auto parse_rfc3164(std::string_view line) -> base::Result<LogEntry>;
    auto severity_name(int sev) -> std::string_view;
    auto facility_name(int fac) -> std::string_view;
    auto parse_priority(std::string_view pri) -> std::pair<int, int>;  // facility, severity
}
```

### 1.3 `include/cfbox/mdev.hpp` — 设备事件处理

**动机**：`mdev` 需要解析 `/sys/class/` 和 `/sys/block/` 条目，应用 `/etc/mdev.conf` 规则。

**接口设计**：

```cpp
namespace cfbox::mdev {
    struct DeviceEvent {
        std::string action;    // "add", "remove", "change"
        std::string devpath;   // /sys/devices/...
        std::string subsystem;
        std::string devname;   // sda1, ttyS0, etc.
        std::string devtype;   // "disk", "partition", etc.
        int major = 0;
        int minor = 0;
    };

    struct MdevRule {
        std::string pattern;   // 设备名 glob
        uid_t uid = 0;
        gid_t gid = 0;
        mode_t mode = 0660;
        std::string symlink;   // 可选符号链接
        std::string command;   // 可选执行命令
    };

    auto parse_mdev_conf(const std::string& path) -> base::Result<std::vector<MdevRule>>;
    auto read_device_event() -> base::Result<DeviceEvent>;  // 从 netlink 或 stdin
    auto apply_rule(const DeviceEvent& event, const MdevRule& rule) -> base::Result<void>;
    auto create_device_node(const DeviceEvent& event, mode_t mode, uid_t uid, gid_t gid)
        -> base::Result<void>;
    auto remove_device_node(const DeviceEvent& event) -> base::Result<void>;
}
```

---

## Part 2: 新增 Applet（按依赖排序分波）

### Wave 1: 登录管理（依赖 `userdb.hpp`）

#### 2.1 `getty` — `src/applets/getty.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-L` | 本地线路，无载波检测 |
| `-h` | 启用硬件流控 |
| `-i` | 不打印 /etc/issue |
| `-n` | 不提示登录名 |
| `-t TIMEOUT` | 登录超时 |
| `-b BAUD` | 波特率 |
| `TTY` | 终端设备 |
| `BAUD_RATE` | 波特率参数 |
| `TERM` | 终端类型 |

**实现**：打开 TTY、设置 termios 波特率/奇偶校验、打印 `/etc/issue`、提示登录名、`exec login`。

**依赖**：`tty.hpp`（Phase 1 已建）

#### 2.2 `login` — `src/applets/login.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-f USERNAME` | 跳过认证 |
| `-h HOST` | 来源主机 |
| `-p` | 保留环境 |
| `-t TIMEOUT` | 认证超时 |

**依赖**：`userdb.hpp`、`tty.hpp`

**实现**：
1. 提示密码（关闭回显）
2. 通过 `userdb::verify_password()` 验证
3. 设置环境（HOME、SHELL、USER、LOGNAME、PATH）
4. `exec` shell

**安全要求**：
- 密码内存使用后清零
- 失败登录延迟（至少 1 秒递增）
- 限制连续失败次数

#### 2.3 `su` — `src/applets/su.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-` / `-l` | 登录 shell |
| `-c CMD` | 执行命令 |
| `-s SHELL` | 指定 shell |
| `USER` | 目标用户 |

**依赖**：`userdb.hpp`

**实现**：验证当前用户密码（非 root 时），切换到目标用户，exec shell 或命令。

#### 2.4 `passwd` — `src/applets/passwd.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-a ALGO` | 哈希算法 |
| `-d` | 删除密码 |
| `-l` | 锁定账户 |
| `-u` | 解锁账户 |
| `-S` | 显示状态 |
| `USERNAME` | 目标用户 |

**依赖**：`userdb.hpp`

**实现**：提示旧密码（非 root 时）、提示新密码两次、哈希并更新 shadow 文件。

#### 2.5 `chpasswd` — `src/applets/chpasswd.cpp`

**选项**：`-e`（已加密）、`-m`（MD5）、`-c ALGO`

**实现**：从 stdin 读取 `user:password` 行，更新 shadow 条目。

#### 2.6 `groups` — `src/applets/groups.cpp`

**选项**：`USERNAME`

**实现**：查找用户主组和附加组，打印组名。

#### 2.7 `adduser` — `src/applets/adduser.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-h DIR` | 家目录 |
| `-s SHELL` | 登录 shell |
| `-G GROUP` | 主组 |
| `-D` | 不分配密码 |
| `-u UID` | 指定 UID |
| `USERNAME` | 用户名 |

**依赖**：`userdb.hpp`

**实现**：添加 `/etc/passwd` 条目、创建家目录、可选设置密码。

#### 2.8 `deluser` — `src/applets/deluser.cpp`

**选项**：`USERNAME`

**实现**：从 `/etc/passwd` 和 `/etc/shadow` 移除条目。

#### 2.9 `addgroup` — `src/applets/addgroup.cpp`

**选项**：`-g GID`（指定 GID）、`-S`（系统组）、`GROUPNAME`

#### 2.10 `delgroup` — `src/applets/delgroup.cpp`

**选项**：`GROUPNAME`

#### 2.11 `sulogin` — `src/applets/sulogin.cpp`

**选项**：`TTY`

**实现**：单用户模式登录，只接受 root 密码。

### Wave 2: 系统日志（依赖 `syslog.hpp`）

#### 2.12 `syslogd` — `src/applets/syslogd/`（多文件）

**文件结构**：
```
src/applets/syslogd/
├── syslogd.hpp          // 内部共享定义
├── syslogd_main.cpp     // 入口点
├── syslogd_klog.cpp     // 内核日志 (/proc/kmsg)
└── syslogd_udp.cpp      // UDP 远程日志
```

**选项**：
| 选项 | 说明 |
|------|------|
| `-n` | 前台运行 |
| `-O FILE` | 输出文件 |
| `-l N` | 日志级别 |
| `-s SIZE` | ring buffer 大小 |
| `-b N` | 轮转数量 |
| `-R HOST` | 远程日志 |
| `-L` | 本地 + 远程 |
| `-K` | 内核日志 |

**实现**：
- 监听 `/dev/log` Unix socket
- 接收日志消息
- 写入 ring buffer 和/或文件
- 可选内核日志（`/proc/kmsg`）
- 可选 UDP 转发

#### 2.13 `logger` — `src/applets/logger.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-p PRI` | 优先级（如 user.info） |
| `-t TAG` | 程序标签 |
| `-i` | 包含 PID |
| `-f FILE` | 记录文件内容 |
| `-s` | 同时输出到 stderr |

**依赖**：`syslog.hpp`

**实现**：连接 `/dev/log` Unix socket，格式化 RFC 3164 消息，发送。

#### 2.14 `logread` — `src/applets/logread.cpp`

**选项**：`-f`（跟踪，持续输出）、`-t`（显示时间戳）

**依赖**：`syslog.hpp`

**实现**：读取 syslogd 的共享内存 ring buffer 或日志文件，输出条目。

### Wave 3: 设备与存储

#### 2.15 `mdev` — `src/applets/mdev.cpp`

**选项**：`-s`（启动时扫描 /sys）

**依赖**：`mdev.hpp`

**实现**：
1. 从 netlink 或 stdin 读取 uevent
2. 解析设备属性
3. 应用 `/etc/mdev.conf` 规则
4. 在 `/dev` 创建/移除设备节点

#### 2.16 `blkid` — `src/applets/blkid.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-o FORMAT` | 输出格式 |
| `-s TAG` | 显示指定标签 |
| `-t TOKEN` | 匹配 token |
| `DEVICE` | 指定设备 |

**实现**：探测块设备的文件系统类型、UUID、LABEL。读取 `/dev/` 或 `/proc/partitions`。

#### 2.17 `lsblk` — `src/applets/lsblk.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-l` | 列表格式 |
| `-n` | 无表头 |
| `-o COLUMNS` | 指定列 |
| `-p` | 完整路径 |

**依赖**：`proc.hpp`（可能需要扩展 `read_partitions()`）

**实现**：解析 `/sys/block/` 层次结构获取设备树、大小、挂载点。

#### 2.18 `losetup` — `src/applets/losetup.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-d` | 分离 |
| `-f` | 查找空闲 |
| `-j FILE` | 关联查询 |
| `-o OFFSET` | 偏移 |
| `-r` | 只读 |
| `DEVICE FILE` | 设置关联 |

**实现**：通过 `ioctl(LOOP_SET_FD, LOOP_CLR_FD)` 管理回环设备。

#### 2.19 `mkswap` — `src/applets/mkswap.cpp`

**选项**：`-L LABEL`（标签）、`-U UUID`（UUID）、`-f`（强制）、`DEVICE`

**实现**：向设备写入 swap 头。

#### 2.20 `swapon` — `src/applets/swapon.cpp`

**选项**：`-a`（所有）、`-d`（丢弃）、`-p PRI`（优先级）、`DEVICE`

**依赖**：`<sys/swap.h>` 的 `swapon()`

#### 2.21 `swapoff` — `src/applets/swapoff.cpp`

**选项**：`-a`（所有）、`DEVICE`

**依赖**：`<sys/swap.h>` 的 `swapoff()`

#### 2.22 `pivot_root` — `src/applets/pivot_root.cpp`

**选项**：`NEW_ROOT PUT_OLD`

**依赖**：`<sys/syscall.h>` 的 `pivot_root()` 系统调用

**实现**：调用 pivot_root 系统调用，通常后跟 chroot 和 exec。

#### 2.23 `switch_root` — `src/applets/switch_root.cpp`

**选项**：`NEWROOT INIT`

**实现**：
1. 删除 initramfs 中的所有文件
2. `mount --move` 文件系统
3. chroot
4. exec init

**关键场景**：initramfs 到真实 rootfs 的转换。这是嵌入式启动链路的关键步骤。

---

## Part 3: 测试

### 3.1 单元测试（GTest）

| 文件 | 测试内容 |
|------|---------|
| `test_userdb.cpp` | passwd/shadow/group 解析、密码验证、用户查询 |
| `test_syslog.cpp` | RFC 3164 格式化/解析、优先级编码 |
| `test_mdev.cpp` | 规则解析、设备事件解析 |
| `test_getty.cpp` | 波特率解析、termios 设置 |
| `test_login.cpp` | 密码验证、环境设置 |
| `test_switch_root.cpp` | 参数解析（实际 root 切换在 QEMU 中测试） |

### 3.2 集成测试（特权）

| 脚本 | 测试场景 |
|------|---------|
| `test_login.sh` | 正确/错误密码登录 |
| `test_syslogd.sh` | 启动 syslogd、logger 发送、logread 读取 |
| `test_mdev.sh` | 创建模拟 uevent、验证设备节点 |
| `test_blkid.sh` | 创建 loop 设备带文件系统、探测 |
| `test_mkswap.sh` | 在 loop 设备上创建 swap |
| `test_switch_root.sh` | QEMU initramfs 启动 + switch_root 到真实 rootfs |

这些测试可通过环境变量跳过（CI 中非特权环境）。

### 3.3 QEMU 系统测试 — 完整嵌入式启动链

**这是 Phase 4 最关键的集成测试**。

**测试序列**（`tests/replacement/embedded/test_embedded_boot.sh`）：

1. QEMU 启动 CFBox 为 PID 1
2. `mount -t proc proc /proc`
3. `mount -t sysfs sysfs /sys`
4. `mkdir -p /dev && mount -t devtmpfs devtmpfs /dev`
5. `mdev -s`（扫描 /sys 创建设备节点）
6. `syslogd -n &`（后台启动 syslogd）
7. `logger -t test "syslog test message"`
8. `logread | grep "syslog test message"`（验证日志往返）
9. `getty -L ttyS0 115200 vt100 &`（串口 getty）
10. 通过 QEMU 串口发送用户名和密码
11. 验证 shell 提示符出现
12. 执行命令验证环境正确

---

## Part 4: 文档

### Applet 文档
- 每个新 applet 标准 `HelpEntry`

### Cookbook 页面
- 设置串口 getty/login
- 配置 syslogd 日志记录
- 编写 mdev.conf 规则
- switch_root 工作流（initramfs → rootfs）
- 使用 losetup 管理回环设备
- 创建和启用 swap

### 安全文档
- login/su/passwd 的安全考虑
- 密码存储格式说明（SHA-256 crypt）
- 无 PAM 支持的说明

---

## Part 5: 兼容性

### BusyBox 对齐
- Login 遵循 BusyBox 行为（无 PAM）
- 文档明确 PAM 不支持
- syslog 格式遵循 RFC 3164
- mdev 规则格式与 BusyBox mdev.conf 兼容
- `adduser`/`deluser` 行为与 BusyBox 一致

### 有意差异记录
- 无 PAM 支持
- 密码哈希使用 SHA-256 crypt ($5$) 格式
- syslogd ring buffer 实现可能不同

### 特权命令治理

以下命令必须有额外要求：
- `login`、`su`、`passwd` — 单独错误路径测试、权限要求文档
- `mount`、`umount`、`chroot` — QEMU 或 namespace 隔离测试
- `mdev`、`mkswap`、`swapon`/`swapoff` — 权限和容器限制文档
- `pivot_root`、`switch_root` — 只在 QEMU 测试

**所有特权命令**：
- 错误信息包含操作、目标、errno 语义
- 危险操作有清晰示例，避免误导用户
- 文档列出权限要求和容器限制

---

## Part 6: 发布

### CMake 更新
- `CFBOX_APPLETS` 添加 23 个新 applet
- `applet_config.hpp.in` 添加 23 个 `#cmakedefine01` 条目
- `syslogd` 作为多文件 applet 需要特殊 CMake 处理

### Profile 更新
| Profile | 新增 applet |
|---------|-----------|
| `embedded` | 全部 Phase 4 applet（23个） |
| `rescue` | `mdev`、`pivot_root`、`switch_root` |
| `container` | `su`、`groups` |

### 体积影响评估
| 新增基础设施 | 预估体积增量 |
|-------------|-------------|
| userdb.hpp | ~8 KB |
| syslog.hpp | ~4 KB |
| mdev.hpp | ~3 KB |
| 23 个新 applet | ~60 KB |
| **合计** | **~75 KB** |

### Release Artifact
- 更新发布矩阵以包含 embedded profile artifact
- `cfbox-*-embedded` — 包含完整嵌入式运行时

---

## Part 7: 依赖图与里程碑

### 依赖关系

```
基础设施
├── userdb.hpp ───── Wave 1 (全部 11 个登录管理 applet)
├── syslog.hpp ───── Wave 2 (syslogd, logger, logread)
└── mdev.hpp ─────── Wave 3 (mdev, blkid, lsblk, losetup, mkswap, ...)

Wave 1 ── 登录管理 (11 个 applet)
Wave 2 ── 系统日志 (3 个 applet)    可与 Wave 1 并行
Wave 3 ── 设备与存储 (9 个 applet)  可与 Wave 2 并行

QEMU 完整启动测试 ── 依赖 Wave 1+2+3 全部完成
```

### 里程碑

| 里程碑 | 时间 | 内容 |
|--------|------|------|
| M1 | 第 1-2 周 | 基础设施（userdb、syslog、mdev）+ 单元测试 |
| M2 | 第 3-5 周 | Wave 1：登录管理（getty、login、su、passwd、chpasswd、groups、adduser、deluser、addgroup、delgroup、sulogin） |
| M3 | 第 6-7 周 | Wave 2：系统日志（syslogd、logger、logread） |
| M4 | 第 8-9 周 | Wave 3：设备与存储（mdev、blkid、lsblk、losetup、mkswap、swapon、swapoff、pivot_root、switch_root） |
| M5 | 第 10-12 周 | QEMU 完整嵌入式启动测试 + 特权 applet 安全审查 + 体积优化 + **Release v0.5.0** |

---

## 相关文档

- [生产化路线图](../production-roadmap.md) — 全局阶段顺序和优先级
- [兼容性策略](../compatibility-policy.md) — POSIX/BusyBox/GNU 行为裁决
- [v1.0 生产可用标准](../v1-production-criteria.md) — 最终发布门槛
- [Phase 0 前置门禁](phase-0a-baseline-inventory.md) — Phase 0A-0F 执行细节
