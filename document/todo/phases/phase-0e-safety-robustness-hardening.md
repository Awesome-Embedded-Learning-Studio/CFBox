# Phase 0E: 安全、鲁棒性与危险操作治理

## 概述

**目标**：在新增特权命令和系统控制能力之前，先治理崩溃输入、执行边界、危险操作和 parser 风险。生产可用的 BusyBox 替代品必须在坏输入、权限不足、容器限制和恶意文件下可预期失败。

**进入条件**：Phase 0A 完成风险标签，Phase 0D 完成 IO 风险清单。

**退出条件**：
- 数值解析统一为不抛异常、不溢出、可返回错误的 helper。
- `fork/exec/popen/kill/mount/mknod/sysctl` 等危险路径有审计清单和测试策略。
- parser 和文件格式解析器建立 fuzz smoke。
- 特权命令和破坏性命令的错误信息、权限要求、容器限制写入文档。
- Phase 1 新增 `mount/umount/chroot/dd/reboot/poweroff` 前具备危险操作规范。

**硬门禁**：
- 未捕获的 `stoi/stoul/stoull` 异常不得进入 production profile。
- parser 崩溃样本必须修复并进入回归。
- 破坏性或特权命令没有隔离测试不得标为 production。

---

## Part 1: 数值解析治理

当前风险点包括：

`proc.hpp`、`fuser`、`watch`、`kill`、`ar`、`mknod`、`pgrep`、`iostat`、`cut`、`hexdump`、`install`、`fold`、`mkfifo`、`nice`、`top`、`pmap`、`cal`、`expand`、`shuf`、`renice`、`find`、`xargs`、`split`、shell builtins。

要求新增或统一使用：

```cpp
namespace cfbox::parse {
    auto parse_int(std::string_view, int min, int max) -> base::Result<int>;
    auto parse_uint64(std::string_view, uint64_t max) -> base::Result<uint64_t>;
    auto parse_octal_mode(std::string_view) -> base::Result<mode_t>;
}
```

### 数值解析风险点详表

以下表格基于代码库审计，列出每个使用不安全数值解析的文件、具体调用及上下文：

| 文件 | 风险调用 | 上下文 |
|------|----------|--------|
| `include/cfbox/proc.hpp` (L264) | `std::strtol(p, nullptr, 10)` → `pi.priority` | `/proc/[pid]/stat` priority 字段解析，恶意/竞态进程可注入非数字 |
| `include/cfbox/proc.hpp` (L269) | `std::strtol(p, nullptr, 10)` → `pi.nice_val` | `/proc/[pid]/stat` nice 字段解析，同上 |
| `include/cfbox/proc.hpp` (L312) | `std::stoi(name)` → `pid_t` | `/proc` 目录遍历，name 为目录名；空/非数字目录名可触发 `std::invalid_argument` |
| `src/applets/fuser.cpp` (L63) | `std::stoi(name)` → `pid_t` | `/proc/[pid]/fd` 遍历解析 PID，竞态条件下进程消失 |
| `src/applets/watch.cpp` (L85) | `std::stoi(*v)` → `interval` | 用户 `-n` 参数指定刷新间隔秒数，非法值导致未捕获异常 |
| `src/applets/kill.cpp` (L45) | `std::atoi(name.data())` | 信号名/编号解析，`atoi` 无法检测溢出和无效输入 |
| `src/applets/kill.cpp` (L116) | `std::strtol(arg.data(), nullptr, 10)` → `pid_t` | PID 参数解析，无溢出检查，无尾随垃圾检查 |
| `src/applets/ar.cpp` (L91) | `std::stoul(size_str)` | AR 归档头文件大小字段解析，畸形归档可触发 `std::out_of_range` |
| `src/applets/mknod.cpp` (L47-57) | `std::stoul(pos[2])`, `std::stoul(pos[3])` | 设备号 major/minor 解析，用户输入直接传入，可溢出 |
| `src/applets/pgrep.cpp` (L61) | `std::stoi(*v)` → `pid_t` (filter_ppid) | `-P` 父进程 PID 过滤，非法输入崩溃 |
| `src/applets/pgrep.cpp` (L64) | `std::stoi(*v)` → `uid_t` (filter_uid) | `-u` UID 过滤，非法输入崩溃 |
| `src/applets/pgrep.cpp` (L76) | `std::stoi(*v)` → signal | `-s` 信号编号解析，非法输入崩溃 |
| `src/applets/iostat.cpp` (L81) | `std::stoi(*v)` → count | `-c` 采样次数参数，非法输入崩溃 |
| `src/applets/cut.cpp` (L33-42) | 多处 `std::stoi(token)` | `-f`/`-b` 字段范围解析（如 `1-3,5`），非数字段触发异常 |
| `src/applets/hexdump.cpp` (L68) | `std::stoull(*v)` → max_bytes | `-n` 字节数参数，溢出时 `std::out_of_range` |
| `src/applets/hexdump.cpp` (L69) | `std::stoull(*v)` → skip_bytes | `-s` 跳过字节数参数，同上 |
| `src/applets/install.cpp` (L40) | `std::stoul(s, nullptr, 8)` → mode | `-m` 权限模式解析（八进制），溢出或非八进制字符未处理 |
| `src/applets/fold.cpp` (L31) | `std::stoi(*w)` → width | `-w` 行宽参数，非法输入崩溃 |
| `src/applets/mkfifo.cpp` (L31) | `std::stoul(*mode_str, nullptr, 8)` → mode_t | `-m` 权限模式（八进制），溢出风险 |
| `src/applets/nice.cpp` (L32) | `std::stoi(*n)` → adjustment | `-n` nice 值调整，非法输入崩溃 |
| `src/applets/top/top_main.cpp` (L251) | `std::stoi(*v)` → delay | `-d` 刷新延迟秒数，非法输入崩溃 |
| `src/applets/top/top_main.cpp` (L252) | `std::stoi(*v)` → iterations | `-n` 迭代次数，非法输入崩溃 |
| `src/applets/pmap.cpp` (L101) | `std::stoi(args[0])` → pid_t | 位置参数 PID，空或非数字输入直接崩溃 |
| `src/applets/cal.cpp` (L93,101,102) | 多处 `std::stoi(pos[...])` | 月份/年份参数解析，非法输入崩溃 |
| `src/applets/expand.cpp` (L29) | `std::stoi(*t)` → tab_stop | `-t` 制表符宽度，非法输入崩溃 |
| `src/applets/shuf.cpp` (L36) | `std::stoi(*n)` → max_count | `-n` 输出行数限制，非法输入崩溃 |
| `src/applets/renice.cpp` (L38) | `std::stoi(*v)` → increment | `-n` nice 增量，非法输入崩溃 |
| `src/applets/renice.cpp` (L48) | `std::stoi(id_str)` → id_t | PID/PGID/UID 参数解析，非法输入崩溃 |
| `src/applets/find.cpp` (L94) | `std::atoi(argv[i + 1])` | `-size`/`-depth` 等 numeric predicate 值，`atoi` 无错误检测 |
| `src/applets/xargs.cpp` (L42) | `std::stoi(*n)` → max_args | `-n` 每次命令最大参数数，非法输入崩溃 |
| `src/applets/split.cpp` | （逻辑中使用前缀数字，当前通过字符串处理） | `-l` 行数 / `-b` 字节数参数，未来增强时需纳入 |
| `src/applets/sh/sh_builtins.cpp` (L88) | `std::atoi(args[1].c_str())` | `exit` 内建命令退出码解析，非数字退出码静默返回 0 |
| `src/applets/sh/sh_builtins.cpp` (L135) | `std::atoi(args[1].c_str())` | `read` 内建命令的超时参数，非法输入静默返回 0 |
| `src/applets/sh/sh_executor.cpp` (L59) | `std::atoi(r.target.c_str())` | shell 重定向 fd 编号解析（如 `2>`），非数字静默返回 0 |

**风险分类说明**：
- `std::stoi` / `std::stoul` / `std::stoull`：当输入为空字符串、纯非数字或值超出类型范围时抛出 `std::invalid_argument` 或 `std::out_of_range`，未被 try-catch 包裹时直接导致进程崩溃。
- `std::atoi`：无法区分 "0" 和非法输入（如 "abc"→0），无法检测溢出，静默返回错误结果。
- `std::strtol`：需手动检查 `errno` 和 `endptr`，当前多处调用未做此检查。

### 验收

新增 helper 必须通过以下具体测试用例：

```cpp
// parse_int 基础用例
assert(parse_int("123", 0, 1000) == 123);              // 正常值
assert(parse_int("", 0, 1000).is_error());              // 空字符串 → 错误
assert(parse_int("999999999999", 0, 1000).is_error()); // 溢出 → 错误
assert(parse_int("12abc", 0, 1000).is_error());         // 尾随垃圾 → 错误
assert(parse_int("-5", 0, 1000).is_error());            // 负数超出范围 → 错误

// parse_uint64 用例
assert(parse_uint64("18446744073709551616", UINT64_MAX).is_error()); // 溢出 → 错误
assert(parse_uint64("0", UINT64_MAX) == 0);                         // 边界下限
assert(parse_uint64("18446744073709551615", UINT64_MAX) == UINT64_MAX); // 边界上限

// parse_octal_mode 用例
assert(parse_octal_mode("755") == 0755);   // 标准权限
assert(parse_octal_mode("4755") == 04755); // setuid + 标准权限
assert(parse_octal_mode("1755") == 01755); // sticky + 标准权限
assert(parse_octal_mode("7777") == 07777); // 全部位置位
assert(parse_octal_mode("").is_error());              // 空字符串
assert(parse_octal_mode("9").is_error());             // 非八进制字符
assert(parse_octal_mode("888").is_error());           // 非八进制字符
assert(parse_octal_mode("12345678901").is_error());   // 溢出

// parse_int 边界用例
assert(parse_int("0", -10, 10) == 0);                 // 范围内零
assert(parse_int("-10", -10, 10) == -10);              // 负数下限
assert(parse_int("10", -10, 10) == 10);                // 正数上限
assert(parse_int("-11", -10, 10).is_error());          // 低于范围
assert(parse_int("11", -10, 10).is_error());           // 超出范围
assert(parse_int("  42", 0, 100).is_error());          // 前导空格不自动跳过 → 错误
assert(parse_int("+7", 0, 100).is_error());            // 显式正号不自动接受 → 错误（视设计决定）
```

验收要求：

- 非数字、空字符串、溢出、负数、尾随垃圾均有测试。
- CLI 错误返回码与 BusyBox/GNU 高频行为对齐。
- `/proc` 解析遇到竞态进程消失时跳过或返回清晰错误，不崩溃。
- 所有上表中列出的风险调用点必须在 Phase 0E 期间逐一迁移至新 helper 或添加等效防御。
- 迁移进度须记录在风险清单中（待迁移 / 已迁移 / 不适用 + 原因）。

---

## Part 2: 执行边界

以下路径必须审计：

| 类型 | 当前/未来命令 | 要求 |
|------|---------------|------|
| `fork/exec` | `sh`、`xargs`、`find -exec`、`timeout`、`watch`、`env`、`nice`、`nohup` | 参数构造安全、错误码、信号退出码、fd 关闭 |
| `popen` | shell 命令替换 | 注入边界、嵌套、错误处理、资源释放 |
| signal | `kill`、`pgrep/pkill`、future `killall/halt/reboot` | 信号解析统一，拒绝危险误用 |
| `/proc` 写入 | `sysctl` | 权限错误、容器限制、dry-run/文档 |
| 特殊文件 | `mkfifo`、`mknod`、future `mount/umount` | root/capability 要求、namespace/QEMU 测试 |

执行型命令必须记录：

- 是否查找 `PATH`。
- 是否继承环境。
- 是否关闭/重定向 fd。
- 子进程失败时父进程如何返回。
- 信号退出码是否为 `128 + signal`。

---

## Part 3: Parser 与格式安全

P0 fuzz target：

- 压缩：inflate、deflate、gzip header/trailer。
- 归档：tar、cpio、ar、zip/unzip。
- 语言：sh、awk、sed。
- 文本格式：patch/diff、find expression。
- 系统文件：inittab、proc/net 未来解析器。

### Fuzz target 入口命令

每个 P0 fuzz target 的具体入口命令如下：

```bash
# ---- 压缩格式 ----
# inflate 解压 fuzz：测试 deflate 解码器对畸形 zlib/gzip 数据的处理
./build-fuzz/fuzz_inflate < corpus/inflate/*

# deflate 压缩 fuzz：测试压缩器对极端输入的处理
./build-fuzz/fuzz_deflate < corpus/deflate/*

# gzip header/trailer fuzz：测试 gzip 封装头解析（魔数、字段、大小）
./build-fuzz/fuzz_gzip_header < corpus/gzip/*

# ---- 归档格式 ----
# tar 归档 fuzz：测试 tar header 解析（文件名、大小、权限字段）
./build-fuzz/fuzz_tar < corpus/tar/*

# cpio 归档 fuzz：测试 cpio 格式解析
./build-fuzz/fuzz_cpio < corpus/cpio/*

# ar 归档 fuzz：测试 ar header 解析（受影响于 ar.cpp L91 的 stoul 风险）
./build-fuzz/fuzz_ar < corpus/ar/*

# zip/unzip fuzz：测试 ZIP 本地文件头和中央目录解析
./build-fuzz/fuzz_zip < corpus/zip/*

# ---- 语言解析器 ----
# shell parser fuzz：测试词法分析器、语法分析器对畸形脚本的处理
./build-fuzz/fuzz_sh_parser < corpus/sh/*

# awk parser fuzz：测试 AWK 词法/语法分析器
./build-fuzz/fuzz_awk_parser < corpus/awk/*

# sed parser fuzz：测试 sed 命令解析器（地址、命令、替换表达式）
./build-fuzz/fuzz_sed_parser < corpus/sed/*

# ---- 文本格式 ----
# patch/diff fuzz：测试 unified diff 解析和 hunk 应用逻辑
./build-fuzz/fuzz_patch < corpus/patch/*

# find expression fuzz：测试 find 表达式解析器（复杂谓词组合）
./build-fuzz/fuzz_find_expr < corpus/find/*

# ---- 系统文件 ----
# inittab fuzz：测试 inittab 配置解析器（未来）
# ./build-fuzz/fuzz_inittab < corpus/inittab/*
```

**corpus 构建说明**：
- 初始 corpus 可从对应格式的合法文件中提取最小样本。
- 每个 fuzz target 的 corpus 目录结构建议：`corpus/<target_name>/seed/`（合法样本）和 `corpus/<target_name>/crash/`（已知崩溃样本，用于回归）。
- crash 样本必须纳入版本控制的回归测试。

Phase 0E 只要求 smoke fuzz：

```bash
cmake -B build-fuzz -DCFBOX_ENABLE_FUZZING=ON -DCMAKE_CXX_COMPILER=clang++
cmake --build build-fuzz
bash tests/fuzz/run_smoke.sh --seconds 30
```

Phase 3 再扩展为长期 corpus、coverage 和 nightly fuzz。

---

## Part 4: 危险命令治理

以下命令必须在进入 production 前具备隔离测试：

`rm`、`mv`、`cp`、`ln`、`install`、`mkfifo`、`mknod`、`sysctl`、future `chmod`、`chown`、`chgrp`、`chroot`、`dd`、`mount`、`umount`、`halt`、`reboot`、`poweroff`。

要求：

- 单独错误路径测试。
- QEMU、namespace 或临时目录隔离。
- 文档列出权限要求和容器限制。
- 错误信息包含操作、目标、errno 语义。
- 破坏性示例必须使用临时路径，避免误导用户。

---

## Part 5: 供应链与许可证前置

CFBox 当前许可证为 MIT。任何新增第三方代码必须：

- 记录来源、版本、许可证。
- 确认与 MIT 分发兼容。
- 避免引入不可裁剪的大型依赖。
- 进入 SBOM。

对 `third_party/linux` 这类测试/构建用来源，必须明确是否进入 release artifact；默认不得打包进 CFBox 二进制分发。

### 当前第三方代码来源审计表

| 组件 | 来源 | 许可证 | 用途 | 进入 release |
|------|------|--------|------|-------------|
| GTest | Google (`github.com/google/googletest`) | BSD-3-Clause | C++ 测试框架，用于单元测试和集成测试 | 否（仅构建时依赖，不链接到最终二进制） |
| `third_party/linux` | kernel.org (`kernel.org`) | GPLv2 | Linux 内核头文件，用于 QEMU 系统调用和 `/proc`/`/sys` 格式测试 | 否（仅测试用，不打包进 CFBox 二进制分发） |
| `competition/busybox` | busybox.net (`busybox.net`) | GPLv2 | 行为对比参考，用于验证 CFBox 输出兼容性 | 否（仅开发环境参考，不打包） |

**许可证兼容性说明**：
- BSD-3-Clause 与 MIT 兼容，GTest 作为构建时依赖无分发风险。
- GPLv2 组件（Linux 头文件、BusyBox）仅在测试和开发中使用，不链接到 CFBox 二进制，不影响 MIT 分发。
- 任何新增 GPLv2/LGPL 组件必须在此表中记录并明确标注不进入 release artifact。
- 如未来需要引入 Apache-2.0 组件，需额外确认是否包含 NOTICE 文件要求。

**SBOM 维护要求**：
- 每次 CI 构建应生成 `cfbox-sbom.json`，包含上表中所有组件的名称、版本、许可证、使用范围。
- SBOM 格式建议遵循 CycloneDX 或 SPDX 标准。
- 新增第三方依赖必须同步更新此表和 SBOM 配置。

---

## 验收产物

- 数值解析风险清单与 helper 计划。
- 执行边界审计清单。
- P0 fuzz smoke 入口。
- 特权/破坏性命令治理表。
- 许可证和第三方来源清单。
