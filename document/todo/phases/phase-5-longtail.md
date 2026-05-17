# Phase 5: 长尾完备性

## 概述

**目标**：补齐剩余 applet，使 CFBox 成为全面的 BusyBox 替代品：可视化编辑器（`vi`）、额外压缩格式（bzip2/xz/lzma）、cron 调度、内核模块管理、硬件枚举和生活质量工具。

**进入条件**：Phase 4 完成。所有生产 profile 功能正常。

**前置引用**：兼容性裁决原则见 [兼容性策略](../compatibility-policy.md)；全局阶段顺序和优先级见 [生产化路线图](../production-roadmap.md)。

**退出条件**：
- `vi` 支持打开/编辑/保存/搜索/替换/基础 ex 命令
- bzip2/xz/lzma 支持集成
- crond/crontab 功能正常
- 模块管理 applet 工作（insmod/rmmod/lsmod/modprobe）
- 总 applet 数达到 150+
- full profile 二进制 ≤ 900 KB（no-TLS）

**体积预算**：full profile static musl no-TLS ≤ 900 KB（Phase 4 后 ~800 KB，预算新增 ~100 KB）

**原则**：
- 长尾 applet 必须服从 profile 裁剪
- 不为追求数量牺牲核心命令质量
- 大型依赖或复杂格式必须提供体积预算和安全测试说明

---

## Part 1: 新增基础设施

### 1.1 `include/cfbox/bzip2.hpp` — Bzip2 压缩

**动机**：bzip2 系列命令需要独立的压缩/解压实现。

**接口设计**：

```cpp
namespace cfbox::bzip2 {
    auto compress(std::string_view data) -> std::string;
    auto decompress(std::string_view data) -> base::Result<std::string>;
    auto is_bzip2(std::string_view data) -> bool;  // 检查 "BZ" magic
}
```

**实现要点**：
- 自实现 Burrows-Wheeler Transform + Move-to-Front + Huffman 编码
- 无 libbz2 依赖
- 预估 ~500 行，是 Phase 5 中最复杂的单件基础设施

**体积影响**：~15 KB

### 1.2 `include/cfbox/xz.hpp` — XZ/LZMA 压缩

**动机**：xz/lzma 系列命令需要 LZMA2 算法实现。

**接口设计**：

```cpp
namespace cfbox::xz {
    auto compress(std::string_view data) -> std::string;
    auto decompress(std::string_view data) -> base::Result<std::string>;
    auto lzma_compress(std::string_view data) -> std::string;
    auto lzma_decompress(std::string_view data) -> base::Result<std::string>;
}
```

**实现策略**：
- LZMA2 算法 + XZ 格式容器解析
- 考虑使用精简嵌入式 LZMA 解码器（BusyBox 包含 ~3KB 代码的精简 LZMA 解码器）
- **推荐**：先实现解码器（decompress），编码器后加。很多嵌入式场景只需解压。

**体积影响**：~10 KB（仅解码器）

### 1.3 `include/cfbox/cron.hpp` — Cron 表达式解析

**动机**：`crond` 和 `crontab` 需要解析 cron 时间表达式。

**接口设计**：

```cpp
namespace cfbox::cron {
    struct CronTime {
        int minute;    // 0-59, -1 = *
        int hour;      // 0-23, -1 = *
        int day;       // 1-31, -1 = *
        int month;     // 1-12, -1 = *
        int weekday;   // 0-6 (0=Sunday), -1 = *
    };

    struct CronEntry {
        CronTime schedule;
        std::string command;
    };

    auto parse_cron_line(std::string_view line) -> base::Result<CronEntry>;
    auto parse_cron_file(const std::string& path) -> base::Result<std::vector<CronEntry>>;
    auto should_run(const CronEntry& entry, std::time_t now) -> bool;
    auto next_run(const CronEntry& entry, std::time_t now) -> std::time_t;
}
```

**体积影响**：~4 KB

---

## Part 2: 新增 Applet（按依赖排序分波）

### Wave 1: 额外压缩格式（依赖 `bzip2.hpp`、`xz.hpp`）

**推荐实现**：`src/applets/compress_extra.cpp` 单文件，多个入口点。在 `APPLET_REGISTRY` 中注册多个名称。

#### 2.1 `bzip2` — 入口点 `bzip2_main`

**选项**：
| 选项 | 说明 |
|------|------|
| `-1`..`-9` | 压缩级别 |
| `-d` | 解压 |
| `-k` | 保留原文件 |
| `-f` | 强制覆盖 |
| `-c` | 输出到 stdout |
| `-z` | 压缩（默认） |

**依赖**：`bzip2.hpp`

#### 2.2 `bunzip2` — 入口点 `bunzip2_main`

**实现**：`bzip2 -d` 的别名包装。

#### 2.3 `bzcat` — 入口点 `bzcat_main`

**实现**：`bzip2 -dc` 的别名包装。

#### 2.4 `xz` — 入口点 `xz_main`

**选项**：
| 选项 | 说明 |
|------|------|
| `-d` | 解压 |
| `-k` | 保留原文件 |
| `-f` | 强制覆盖 |
| `-c` | 输出到 stdout |
| `-z` | 压缩 |
| `-0`..`-9` | 预设级别 |

**依赖**：`xz.hpp`

#### 2.5 `unxz` — 入口点 `unxz_main`

**实现**：`xz -d` 的别名包装。

#### 2.6 `xzcat` — 入口点 `xzcat_main`

**实现**：`xz -dc` 的别名包装。

#### 2.7 `lzma` — 入口点 `lzma_main`

**选项**：`-d`、`-k`、`-f`、`-c`

**依赖**：`xz.hpp`（LZMA 是 XZ 格式族的子集）

#### 2.8 `unlzma` — 入口点 `unlzma_main`

**实现**：`lzma -d` 的别名包装。

#### 2.9 `lzcat` — 入口点 `lzcat_main`

**实现**：`lzma -dc` 的别名包装。

#### tar 深化（压缩格式集成）

在现有 `src/applets/tar.cpp` 中添加：
| 新选项 | 说明 |
|--------|------|
| `-j` | bzip2 压缩/解压 |
| `-J` | xz 压缩/解压 |

### Wave 2: 可视化编辑器

#### 2.10 `vi` — `src/applets/vi/`（多文件，Phase 5 最大新 applet）

**文件结构**：
```
src/applets/vi/
├── vi.hpp           // 内部共享定义
├── vi_main.cpp      // 入口点、模式分发
├── vi_buffer.cpp    // 缓冲区管理（gap buffer）
├── vi_display.cpp   // 屏幕渲染
├── vi_ex.cpp        // ex 命令模式
├── vi_normal.cpp    // normal 模式命令
└── vi_insert.cpp    // insert 模式处理
```

**缓冲区设计**：
- Gap buffer 或 piece chain 实现高效插入/删除
- 支持大文件（不全部加载到内存）

**模式支持**：

| 模式 | 功能 |
|------|------|
| **Normal** | `h/j/k/l` 移动、`dd` 删行、`yy` 复制行、`p` 粘贴、`x` 删字符、`w/b` 单词跳转、`0/$` 行首尾、`G/gg` 文件首尾、`u` 撤销 |
| **Insert** | `i/a/o/O` 进入、`Esc` 退出 |
| **Command (ex)** | `:w` 保存、`:q` 退出、`:wq`、`:q!`、`:%s/old/new/g` 替换、`:set number`、`:r FILE` |
| **Search** | `/pattern` 搜索、`n/N` 下/上一个 |

**选项**：
| 选项 | 说明 |
|------|------|
| `-e` | ex 模式 |
| `-R` | 只读 |

**目标**：不是完整 vim 克隆。BusyBox vi 级别的功能——基础编辑、保存/加载、搜索。

**依赖**：`terminal.hpp`（已有）、`tui.hpp`（已有）

**体积影响**：~25 KB

### Wave 3: Cron 调度（依赖 `cron.hpp`）

#### 2.11 `crond` — `src/applets/crond.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-f` | 前台运行 |
| `-b` | 后台运行（默认） |
| `-l LOG_LEVEL` | 日志级别 |
| `-L LOG_FILE` | 日志文件 |
| `-d DIR` | spool 目录 |
| `-c CRONDIR` | crontab 目录 |

**依赖**：`cron.hpp`

**实现**：
- 每分钟唤醒
- 解析 `/var/spool/cron/crontabs/` 和 `/etc/crontab`
- 运行到期的命令

#### 2.12 `crontab` — `src/applets/crontab.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-l` | 列出 crontab |
| `-e` | 编辑 crontab |
| `-r` | 删除 crontab |
| `-u USER` | 指定用户 |
| `FILE` | 安装新 crontab |

**依赖**：`cron.hpp`

**实现**：管理 `/var/spool/cron/crontabs/` 中的用户 crontab 文件。

### Wave 4: 内核模块管理

#### 2.13 `insmod` — `src/applets/insmod.cpp`

**选项**：`MODULE [PARAMS...]`

**依赖**：`<sys/syscall.h>` 的 `init_module()`/`finit_module()` 系统调用

**实现**：读取 .ko 文件，调用 `finit_module()` 或 `init_module()`。

#### 2.14 `rmmod` — `src/applets/rmmod.cpp`

**选项**：`-f`（强制）、`-w`（等待）、`MODULE`

**依赖**：`<sys/syscall.h>` 的 `delete_module()` 系统调用

#### 2.15 `lsmod` — `src/applets/lsmod.cpp`

**选项**：无

**实现**：解析并显示 `/proc/modules`。

#### 2.16 `modprobe` — `src/applets/modprobe.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-r` | 移除模块 |
| `-a` | 所有 |
| `-n` | 干运行 |
| `-v` | 详细 |
| `MODULE` | 模块名 |

**实现**：
- 解析模块依赖（`/lib/modules/$(uname -r)/modules.dep`）
- 解析依赖后通过 `insmod` 逻辑加载

**注意**：`modprobe` 依赖 `depmod` 生成的 `modules.dep`。

#### 2.17 `depmod` — `src/applets/depmod.cpp`

**选项**：`-a`（全部）、`-b BASE`（基目录）、`-e`（错误符号）、`-n`（干运行）

**实现**：扫描模块目录，解析符号依赖，写入 `modules.dep`。

#### 2.18 `modinfo` — `src/applets/modinfo.cpp`

**选项**：`-F FIELD`（指定字段）、`-0`（null 分隔）、`MODULE`

**实现**：读取 .ko 文件 ELF 段（.modinfo）并显示元数据。

**注意**：模块管理是 Linux 特有的，需要理解 ELF 模块格式。推荐先实现 `insmod`/`rmmod`/`lsmod`（较简单），再添加 `modprobe`/`depmod`/`modinfo`。

### Wave 5: 硬件枚举

#### 2.19 `lspci` — `src/applets/lspci.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-n` | 数字 ID |
| `-v` | 详细 |
| `-m` | 机器可读 |
| `-k` | 内核驱动 |

**实现**：解析 `/proc/bus/pci/devices` 或扫描 `/sys/bus/pci/devices/`。查找 vendor/device ID。

#### 2.20 `lsusb` — `src/applets/lsusb.cpp`

**选项**：`-v`（详细）、`-d VENDOR:DEVICE`、`-s BUS:DEV`

**实现**：解析 `/sys/bus/usb/devices/` 或 `/proc/bus/usb/devices`。

### Wave 6: 生活质量工具

#### 2.21 `less` — `src/applets/less.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-N` | 行号 |
| `-S` | 截断长行 |
| `-i` | 搜索忽略大小写 |
| `-e` | EOF 退出 |
| `-F` | 单屏退出 |
| `+N` | 起始行号 |

**依赖**：`tui.hpp`、`terminal.hpp`

**实现**：复用为 `top` 和 `watch` 构建的 TUI 框架。逐页浏览文件，支持搜索和滚动。

#### 2.22 `strings` — `src/applets/strings.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-a` | 扫描全部 |
| `-n N` | 最小长度（默认 4） |
| `-t FORMAT` | 偏移格式 |
| `-e ENCODING` | 字符编码 |
| `FILE` | 输入文件 |

**实现**：扫描二进制文件，提取长度 ≥ N 的可打印 ASCII 序列。

#### 2.23 `tree` — `src/applets/tree.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-d` | 仅目录 |
| `-L N` | 最大深度 |
| `-a` | 包含隐藏 |
| `-f` | 完整路径 |
| `-p` | 显示权限 |
| `-h` | 人类可读大小 |
| `-P PATTERN` | 匹配模式 |

**依赖**：`fs_util.hpp`、`term.hpp`（颜色输出）

#### 2.24 `xxd` — `src/applets/xxd.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-c N` | 列数 |
| `-g N` | 分组大小 |
| `-l N` | 长度 |
| `-s OFFSET` | 起始偏移 |
| `-r` | 反向（hex → binary） |
| `-p` | 纯 hex |
| `-i` | C include 格式 |

**注意**：项目已有 `hexdump.cpp`；`xxd` 有不同输出格式和反向模式。

#### 2.25 `dos2unix` — `src/applets/dos2unix.cpp`

**选项**：无（或 `-u` 切换为 unix2dos）

**实现**：剥离 `\r\n` 前的 `\r`。

#### 2.26 `unix2dos` — 别名或包装

**实现**：在 `\n` 前插入 `\r`。

#### 2.27 `reset` — `src/applets/reset.cpp`

**选项**：无

**依赖**：`terminal.hpp`

**实现**：写入终端重置序列 `\033c`。

#### 2.28 `uuidgen` — `src/applets/uuidgen.cpp`

**选项**：`-r`（随机）、`-t`（时间基）

**实现**：读取 `/proc/sys/kernel/random/uuid` 或使用随机字节 + 格式化。

---

## Part 3: 测试

### 3.1 单元测试（GTest）

| 文件 | 测试内容 |
|------|---------|
| `test_bzip2.cpp` | 压缩/解压往返、bzip2 规范测试向量 |
| `test_xz.cpp` | 解压已知 .xz 文件、压缩/解压往返 |
| `test_cron.cpp` | cron 表达式解析、should_run 逻辑 |
| `test_vi_buffer.cpp` | gap buffer 操作、插入、删除、搜索 |
| `test_strings.cpp` | 最小长度、偏移、编码选项 |
| `test_tree.cpp` | 格式化、深度限制、模式匹配 |
| `test_xxd.cpp` | hex dump 格式、反向模式 |
| `test_lsmod.cpp` | /proc/modules 解析 |
| `test_lspci.cpp` | PCI ID 查找 |
| `test_crontab.cpp` | crontab 文件管理 |

### 3.2 集成测试

| 脚本 | 测试场景 |
|------|---------|
| `test_bzip2.sh` | 创建/压缩/解压/验证 |
| `test_xz.sh` | 创建/压缩/解压/验证 |
| `test_vi.sh` | 基础编辑操作（通过 ex 模式脚本化） |
| `test_crond.sh` | 调度任务、验证执行 |
| `test_less.sh` | 逐页浏览文件、搜索 |
| `test_tar_j.sh` | tar + bzip2 压缩 |
| `test_tar_J.sh` | tar + xz 压缩 |
| `test_tree.sh` | 目录树输出 |
| `test_xxd.sh` | hex dump 和反向 |
| `test_dos2unix.sh` | 行尾转换 |

### 3.3 系统测试（QEMU）

- 模块管理测试：在 QEMU 中加载测试模块
- Cron 执行测试：在 QEMU 中调度并验证
- 完整压缩格式往返：`tar -cjf`、`tar -cJf` 在 QEMU 中

---

## Part 4: 文档

### Applet 文档
- 每个新 applet 标准 `HelpEntry`
- `vi` 需要详细的使用说明（normal/insert/ex 模式）
- 压缩格式 applet 需要格式兼容性说明

### Cookbook 页面
- `vi` 基础操作指南
- 模块管理工作流（insmod/rmmod/modprobe）
- Crontab 格式和使用
- 使用 bzip2/xz 压缩归档
- 使用 `tree` 查看目录结构
- 使用 `xxd` 检查二进制文件

### 兼容性文档
- `vi` 明确标注为 BusyBox vi 级别，不是 vim
- bzip2/xz 格式兼容标准工具
- `crond` 兼容标准 crontab 格式

---

## Part 5: 兼容性

### BusyBox 对齐
- `vi` 目标是 BusyBox vi 功能级别
- bzip2/xz 命令行选项与 BusyBox 一致
- `crond`/`crontab` 兼容 BusyBox 行为
- 模块管理命令与 BusyBox 选项对齐

### 有意差异记录
- `vi` 不支持 vim 扩展功能（多窗口、插件等）
- xz 初始版本可能只有解压
- `depmod` 实现可能简化

### Profile 裁剪规则

| Profile | 包含 |
|---------|------|
| `full` | 全部 Phase 5 applet |
| `embedded` | `lspci`、`lsusb`、模块管理 |
| `rescue` | 模块管理（`insmod`/`rmmod`/`lsmod`） |
| `container` | `less`、`strings`、`tree`、`xxd`、`uuidgen` |
| `minimal` | 无 Phase 5 applet |

---

## Part 6: 发布

### CMake 更新
- `CFBOX_APPLETS` 添加 28+ 个新 applet
- `applet_config.hpp.in` 添加新 `#cmakedefine01` 条目
- `vi` 和 `compress_extra` 作为多文件 applet

### 体积追踪

| 新增内容 | 预估体积 |
|---------|---------|
| bzip2.hpp | ~15 KB |
| xz.hpp（仅解码器） | ~10 KB |
| cron.hpp | ~4 KB |
| vi applet | ~25 KB |
| crond + crontab | ~8 KB |
| 模块管理（6个） | ~12 KB |
| 硬件枚举（2个） | ~6 KB |
| 生活质量工具（8个） | ~15 KB |
| **合计** | **~95 KB** |

### v1.0 发布矩阵

**必须发布**：
| Artifact | 链接方式 | 用途 |
|----------|---------|------|
| `cfbox-x86_64-linux-musl` | static | 容器、initramfs、通用 Linux |
| `cfbox-aarch64-linux-musl` | static | ARM64 嵌入式、树莓派 |
| `cfbox-armv7-linux-musleabihf` | static | 32 位 ARM 嵌入式 |
| `cfbox-x86_64-linux-gnu` | dynamic/static | 桌面发行版 |

**可选发布**：
| Artifact | 说明 |
|----------|------|
| `*-tls` | 启用 TLS 后端 |
| `*-debug` | 带 debug symbols |
| `*-rescue` | rescue profile |
| `*-container` | container profile |
| `*-embedded` | embedded profile |

### v1.0 体积预算验证

| 构建 | 目标上限 |
|------|---------|
| minimal static musl | ≤ 350 KB |
| rescue static musl | ≤ 500 KB |
| container static musl | ≤ 650 KB |
| embedded static musl | ≤ 800 KB |
| full static musl no-TLS | ≤ 900 KB |
| full static musl TLS | ≤ 1.4 MB |

### v1.0 Release Checklist

- [ ] 所有 applet --help 文档完整
- [ ] 兼容性矩阵公开
- [ ] P0 applet 达到 `production` 成熟度
- [ ] P1 applet 达到 `compatible` 成熟度
- [ ] 所有单元/集成/QEMU 测试通过
- [ ] ASan/UBSan clean
- [ ] Fuzz target 无已知崩溃
- [ ] 每个 profile 通过替换测试
- [ ] Release artifact + checksum + 签名
- [ ] SBOM
- [ ] Size report
- [ ] 测试矩阵
- [ ] Changelog

---

## Part 7: 依赖图与里程碑

### 依赖关系

```
基础设施
├── bzip2.hpp ───── Wave 1 (bzip2 系列)
├── xz.hpp ──────── Wave 1 (xz/lzma 系列)
├── cron.hpp ─────── Wave 3 (crond, crontab)

Wave 1 ── 压缩格式 (9 个命令 + tar 深化)
Wave 2 ── vi (无额外基础设施依赖，依赖已有 tui/terminal)
Wave 3 ── Cron (2 个命令)
Wave 4 ── 模块管理 (6 个命令)
Wave 5 ── 硬件枚举 (2 个命令)
Wave 6 ── 生活质量工具 (8 个命令)

Wave 1-6 可一定程度并行，但有资源优先级
```

### 里程碑

| 里程碑 | 时间 | 内容 |
|--------|------|------|
| M1 | 第 1-3 周 | 压缩格式（bzip2 压缩/解压、xz/lzma 解压、tar -j/-J 支持）+ 测试 |
| M2 | 第 4-6 周 | 可视化编辑器（vi 缓冲区、显示、normal 模式、insert 模式、ex 命令、搜索/替换） |
| M3 | 第 7-8 周 | Cron（cron 表达式解析、crond 守护进程、crontab 管理） |
| M4 | 第 9-10 周 | 模块管理（insmod、rmmod、lsmod、modprobe、depmod、modinfo） |
| M5 | 第 11-12 周 | 硬件枚举 + 生活质量工具（lspci、lsusb、less、strings、tree、xxd、dos2unix、unix2dos、reset、uuidgen） |
| M6 | 第 13-14 周 | 完整回归测试 + 体积优化 + 文档完善 + **Release v1.0.0** |

---

## 相关文档

- [生产化路线图](../production-roadmap.md) — 全局阶段顺序和优先级
- [兼容性策略](../compatibility-policy.md) — POSIX/BusyBox/GNU 行为裁决
- [v1.0 生产可用标准](../v1-production-criteria.md) — 最终发布门槛
- [Phase 0 前置门禁](phase-0a-baseline-inventory.md) — Phase 0A-0F 执行细节
