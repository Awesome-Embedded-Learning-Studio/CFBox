# Phase 3: 生产质量门禁

## 概述

**目标**：将 CFBox 从"核心场景功能完整"推进到"可发布、可回归、可审计"。本阶段不以新增 applet 为主，而是构建测试、分析、基准测试和发布工程基础设施，使每个现有 applet 都可信。

**进入条件**：Phase 2 完成。120+ applet，~650 KB 二进制。

**前置引用**：兼容性裁决原则见 [兼容性策略](../compatibility-policy.md)；全局阶段顺序和优先级见 [生产化路线图](../production-roadmap.md)。

**退出条件**：
- 差异测试套件覆盖所有 P0 applet（对比 BusyBox 和 GNU）
- 所有解析器 fuzz target 无已知崩溃
- 基准测试套件比较 CFBox vs BusyBox vs GNU coreutils
- ASan/UBSan 在所有目标上 clean
- clang-tidy 通过核心代码库
- 静态 musl release artifact（x86_64 和 aarch64）
- initramfs 替换测试在 QEMU 中通过
- Alpine minirootfs 替换测试通过

**体积预算**：本阶段不增加 applet，体积增量仅来自测试基础设施代码（不影响发布二进制）。发布二进制目标不变。

---

## Part 1: 差异测试框架

### 1.1 框架设计

**位置**：`tests/differential/`

**核心组件**：

```
tests/differential/
├── framework.sh            # 核心框架
├── run_all.sh              # 运行所有差异测试
├── test_diff_sh.sh         # sh 差异测试
├── test_diff_grep.sh       # grep 差异测试
├── ...                     # 其他 applet
└── known_differences/      # 已知有意差异记录
    ├── sh.yaml
    ├── grep.yaml
    └── ...
```

**框架接口**（`framework.sh`）：

```bash
# 核心函数
run_diff() {
    local applet="$1" shift
    local cfbox_args=("$@")
    # 运行 cfbox <applet> <args>，捕获 stdout/stderr/exit_code
    # 运行 busybox <applet> <args>，捕获 stdout/stderr/exit_code
    # 可选运行 gnu_<applet> <args>
    # 比较并归类
}

# 归类
classify_result() {
    # MATCH: stdout + stderr + exit_code 完全一致
    # ACCEPTABLE_DIFF: 输出格式微小差异（如空格、排序），文档记录
    # FAILURE: 行为不一致，需调查
}

# 输出
# - 终端：彩色表格
# - CI：JUnit XML
```

**测试输出**：区分三类结果：完全一致、可接受差异、缺陷。

### 1.2 P0 Applet 差异测试场景

每个 applet 需要覆盖以下维度：

| Applet | 关键测试场景 |
|--------|-------------|
| `sh` | 变量展开、管道、重定向、命令替换、条件、循环、函数调用 |
| `test` | 所有文件测试操作符、字符串比较、算术比较、逻辑组合 |
| `echo` | `-n`、`-e` 转义、多参数 |
| `cat` | 普通文件、`-n` 行号、`-A` 显示不可见、多文件 |
| `ls` | `-l` 长格式、`-a` 隐藏文件、`-R` 递归、排序 |
| `cp` | 普通复制、`-r` 递归、`-a` 归档、符号链接处理 |
| `mv` | 普通移动、跨文件系统、覆盖 |
| `rm` | 普通删除、`-r` 递归、`-f` 强制、不存在的文件 |
| `mkdir` | 普通创建、`-p` 递归、已存在目录 |
| `chmod` | 八进制模式、符号模式、`-R` 递归 |
| `chown` | `owner:group` 格式、仅 owner、仅 :group |
| `find` | `-name`、`-type`、`-mtime`、布尔表达式、`-print0` |
| `grep` | 基本匹配、`-i`、`-v`、`-n`、`-c`、`-r`、`-F`、`-w`、上下文 |
| `sed` | `s/X/Y/g`、`d`、`p`、`-n`、`-i`、多命令 |
| `awk` | BEGIN/END、字段分割、print、if/for/while、NR/NF |
| `sort` | 默认排序、`-n`、`-r`、`-k`、`-u`、`-t` |
| `head` | 默认 10 行、`-n N`、多文件 |
| `tail` | 默认 10 行、`-n N`、`+N`、`-f`（基本） |
| `wc` | `-l`、`-w`、`-c`、多文件合计 |
| `tar` | 创建/提取、`-z` gzip、`-v`、`--exclude`、`-p` |
| `gzip`/`gunzip` | 压缩/解压往返、`-c` stdout、`-d` |
| `dd` | 基本 bs/count、skip/seek、`status=none` |
| `mount`/`umount` | `-t proc`、`-o ro`（需特权，可跳过） |
| `ps` | 默认输出、`-o` 格式、`-p PID` |
| `kill` | PID 指定、信号名、`-l` 列出信号 |
| `df` | 默认输出、`-h`、`-T`、`-i` |
| `du` | 默认输出、`-s`、`-h`、`-d N` |
| `sha256sum` | 哈希文件、`-c` 校验模式 |

### 1.3 POSIX 子集验证

分层验证策略：

| 层级 | 内容 | 方法 |
|------|------|------|
| L1 | POSIX shell 和 coreutils 子集测试 | 自建测试用例覆盖 POSIX 必要语义 |
| L2 | Open POSIX Test Suite 可移植子集 | 运行 Open POSIX Test Suite 中可移植的部分 |
| L3 | 真实脚本语料 | Alpine init scripts、Autoconf `configure`、常见安装脚本片段 |

所有 POSIX 差异应进入兼容性矩阵。

---

## Part 2: Fuzzing 基础设施

### 2.1 框架设计

**位置**：`tests/fuzz/`

**框架选择**：libFuzzer（clang 内置）+ ASan/UBSan。

**构建集成**：

```cmake
# tests/fuzz/CMakeLists.txt
option(CFBOX_ENABLE_FUZZING "Build fuzz targets" OFF)

if(CFBOX_ENABLE_FUZZING)
    add_compile_options(-fsanitize=fuzzer,address,undefined)
    # 每个 fuzz target 是独立可执行文件
    foreach(target IN ITEMS deflate gzip_header tar cpio ar unzip
                              sh_parser awk_parser sed_parser
                              find_expr patch)
        add_executable(fuzz_${target} fuzz_${target}.cpp)
        target_link_libraries(fuzz_${target} PRIVATE cfbox_lib)
    endforeach()
endif()
```

### 2.2 Fuzz Target 列表

| 文件 | 目标 | 输入格式 |
|------|------|---------|
| `fuzz_deflate.cpp` | deflate + inflate 往返 | 随机字节 |
| `fuzz_gzip_header.cpp` | gzip 头解析 | 随机字节作为 gzip header |
| `fuzz_tar.cpp` | tar 归档解析 | 随机字节作为 tar 归档 |
| `fuzz_cpio.cpp` | cpio 归档解析 | 随机字节作为 cpio 归档 |
| `fuzz_ar.cpp` | ar 归档解析 | 随机字节作为 ar 归档 |
| `fuzz_unzip.cpp` | zip 解压 | 随机字节作为 zip 文件 |
| `fuzz_sh_parser.cpp` | shell 脚本解析 | 随机文本作为 shell 脚本 |
| `fuzz_awk_parser.cpp` | awk 程序解析 | 随机文本作为 awk 程序 |
| `fuzz_sed_parser.cpp` | sed 脚本解析 | 随机文本作为 sed 脚本 |
| `fuzz_find_expr.cpp` | find 表达式解析 | 随机文本作为 find 表达式 |
| `fuzz_patch.cpp` | diff/patch 输入 | 随机字节作为 patch 文件 |

### 2.3 每个 Fuzz Target 模式

```cpp
// fuzz_tar.cpp 示例
#include <cfbox/compress.hpp>
#include <cfbox/tar.hpp>  // tar 解析接口

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
    // 将随机输入喂给 tar 解析器
    // 确保不崩溃、不越界、不内存泄漏
    cfbox::tar::parse(std::string_view(
        reinterpret_cast<const char*>(data), size));
    return 0;
}
```

### 2.4 Corpus 管理

```
tests/fuzz/
├── corpus/
│   ├── tar/           # 有趣的 tar 归档样本
│   ├── gzip/          # gzip 文件样本
│   ├── sh/            # shell 脚本样本
│   └── ...
└── crashes/           # 崩溃样本（成为回归测试）
```

- 所有崩溃样本进入回归 corpus
- AFL++ 用于 nightly 或长期 corpus 演化
- libFuzzer 用于 CI 快速验证

---

## Part 3: 基准测试套件

### 3.1 框架设计

**位置**：`tests/benchmark/`

**框架**：简单计时工具（或 Google Benchmark）。

**比较对象**：CFBox vs BusyBox vs GNU coreutils（系统安装的版本）。

### 3.2 P0 基准测试

| 文件 | 测试内容 | 输入规模 |
|------|---------|---------|
| `bench_cat.cpp` | 文件吞吐 | 100 MB 文件 |
| `bench_grep.cpp` | 模式匹配（简单/复杂正则） | 1M 行文件 |
| `bench_sed.cpp` | `s/X/Y/g` 替换 | 1M 行文件 |
| `bench_awk.cpp` | 模式-动作处理 | 1M 行文件 |
| `bench_sort.cpp` | 默认/数字/字段排序 | 1M 行文件 |
| `bench_wc.cpp` | 单词计数 | 1M 行文件 |
| `bench_find.cpp` | 递归遍历 | 10K 文件目录树 |
| `bench_tar.cpp` | 创建/提取归档 | 10K 文件 |
| `bench_gzip.cpp` | 压缩/解压 | 100 MB 文件 |
| `bench_cp.cpp` | 文件复制 | 10K 文件 |
| `bench_dd.cpp` | 块复制 | 100 MB |
| `bench_ls.cpp` | 目录列表 | 10K 文件 |
| `bench_sha256sum.cpp` | 文件哈希 | 100 MB 文件 |

### 3.3 测量指标

| 指标 | 说明 |
|------|------|
| 吞吐量 | MB/s、lines/s、files/s |
| 峰值 RSS | 通过 `/proc/self/status` VmPeak |
| 启动耗时 | 从 exec 到第一行输出的时间 |
| 二进制体积 | 每个构建配置的 binary size |
| 每 applet 体积增量 | 启用/禁用单个 applet 的体积差 |

**输出格式**：JSON 或 CSV，用于 CI 趋势跟踪。

---

## Part 4: 静态分析配置

### 4.1 clang-tidy

**位置**：项目根目录 `.clang-tidy`

```yaml
Checks: >
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  performance-*,
  modernize-*,
  -modernize-use-trailing-return-type,
  readability-*,
  -readability-magic-numbers,
  -readability-identifier-length
```

**策略**：先启用低噪声规则。不一次性开启高噪声规则导致大规模非功能改动。

**脚本**：`scripts/analysis/clang-tidy.sh`

```bash
#!/bin/bash
# 运行 clang-tidy
# CI: 只检查变更文件
# Full: 检查全部源文件
```

### 4.2 cppcheck

**脚本**：`scripts/analysis/cppcheck.sh`

P1 优先级，覆盖 C++ 常见缺陷。补充 clang-tidy 可能遗漏的问题。

### 4.3 Sanitizers

**门禁顺序**：
1. **ASan/UBSan**：Debug 和 CI 必跑（已集成到 `cmake/compile/CompilerFlag.cmake`）
2. **clang-tidy**：先启用低噪声规则
3. **cppcheck**：P1
4. **include-what-you-use**：P2，长期头文件治理

**脚本**：`scripts/analysis/sanitizers.sh` — 构建并运行 ASan/UBSan/MSan 测试

---

## Part 5: 替换测试基础设施

### 5.1 Initramfs 替换测试

**位置**：`tests/replacement/initramfs/`

```
tests/replacement/initramfs/
├── build_initramfs.sh     # 创建只包含 CFBox 的 cpio initramfs
├── test_rescue.sh         # 启动 QEMU，执行救援操作
└── test_network.sh        # 启动 QEMU（带网络），测试网络命令
```

**test_rescue.sh 测试序列**：
1. QEMU 启动 CFBox 为 init
2. `mount -t proc proc /proc`
3. `mount -t sysfs sysfs /sys`
4. `mkdir -p /dev && mount -t devtmpfs devtmpfs /dev`
5. `dd if=/dev/zero of=/tmp/test.img bs=1M count=5`
6. `tar -czf /tmp/rootfs.tar.gz /etc`
7. `tar -xzf /tmp/rootfs.tar.gz -C /tmp/restore`
8. `sha256sum /tmp/rootfs.tar.gz`
9. `find /tmp -type f -print0 | xargs -0 ls -la`
10. `chmod +x /tmp/test.sh && /tmp/test.sh`
11. 验证所有操作成功

**test_network.sh 测试序列**：
1. 配置 eth0 地址
2. `ping -c 3 10.0.2.2`
3. `wget http://example.com/ -O /tmp/index.html`

### 5.2 Alpine Minirootfs 替换测试

**位置**：`tests/replacement/alpine/`

```
tests/replacement/alpine/
├── setup_alpine.sh        # 下载 Alpine minirootfs，注入 CFBox 符号链接
├── test_alpine_init.sh    # 运行 Alpine init 脚本
└── test_alpine_pkg.sh     # 运行包安装脚本片段
```

**test_alpine_init.sh 测试序列**：
1. 下载 Alpine minirootfs
2. 替换 `/bin/busybox` 为 CFBox 符号链接集
3. 在 QEMU user-mode 中运行 `/sbin/init`
4. 验证基本启动序列：挂载 proc/sys/dev、启动 shell

**test_alpine_pkg.sh 测试序列**：
1. 运行典型 Alpine 包安装脚本片段
2. 验证 shell 脚本兼容性

### 5.3 Container Profile 替换测试

**位置**：`tests/replacement/container/`

```
tests/replacement/container/
├── test_container.sh      # 在 Alpine minirootfs 中执行常见 shell 脚本
└── test_scripts/          # 测试脚本集
    ├── apk_install_sim.sh
    ├── configure_sim.sh
    └── service_init_sim.sh
```

### 5.4 Embedded Profile 端到端测试

**位置**：`tests/replacement/embedded/`

（此测试在 Phase 4 完成后才能完整运行，但框架在此阶段搭建）

```
tests/replacement/embedded/
├── test_embedded_boot.sh  # PID 1 → mount → mdev → syslogd → getty → login
└── test_pid1.sh           # 基础 PID 1 启动测试
```

---

## Part 6: 覆盖率目标

| 组件类型 | 行覆盖目标 | 关键分支覆盖 |
|---------|-----------|-------------|
| 基础库（`error.hpp`、`args.hpp`、`io.hpp`、`fs_util.hpp`） | 90%+ | — |
| Parser 和格式解析器（`compress.hpp`、sh/awk/sed parser） | 85%+ | 90%+ |
| 校验和/摘要（`checksum.hpp`、`digest.hpp`） | 90%+ | — |
| P0 applet | 80%+ | 含错误路径 |
| P1 applet | 70%+ | — |
| 长尾 applet | 基础 smoke test | help/version test |

**覆盖率下降超过阈值时，PR 必须解释原因。**

**测量方法**：`gcov` / `lcov`，CI 中使用 `--coverage` 标志。

---

## Part 7: 发布工程

### 7.1 Release 构建脚本

**位置**：`scripts/release/build_release.sh`

**功能**：
- 接受目标三元组参数（`x86_64-linux-musl`、`aarch64-linux-musl`、`armv7-linux-musleabihf`、`x86_64-linux-gnu`）
- 使用 musl-cross-make 或 `zig cc` 交叉编译
- 构建所有 profile：full、rescue、container、embedded、minimal
- 原生目标运行测试
- 生成 sha256sum 校验
- 生成 size report

### 7.2 Changelog 生成

**工具**：`git-cliff`

**配置**：`cliff.toml`
- 解析 Conventional Commits
- 分组：Added applets / Changed behavior / Compatibility notes / Fixed bugs / Security / Tests / Size changes
- 生成 `CHANGELOG.md`

### 7.3 CI Pipeline 更新

**`.github/workflows/ci.yml`** 更新：

| Job | 内容 |
|-----|------|
| build-matrix | gcc/clang × Debug/Release × sanitizer on/off |
| test | GTest + 集成测试 |
| differential | 差异测试（可选，非每个 PR） |
| size-report | 每个 PR 的体积报告 |
| clang-tidy | 变更文件检查 |
| fuzz-build | 验证 fuzz target 可编译 |
| cross-build | aarch64、armhf 交叉编译 |
| qemu-test | QEMU user-mode 和 system-mode 测试 |

### 7.4 SBOM 与供应链

- 文档所有第三方代码（CPM.cmake、GTest）
- 生成 SPDX SBOM
- 记录工具链版本
- 审计第三方代码来源，避免 GPL 污染 MIT 项目

### 7.5 Release Checklist

每个 release candidate 必须包含：
- artifact 和 checksum（sha256）
- 签名文件
- changelog
- SBOM
- size report（full/minimal/rescue/container/embedded 体积 + 与上一版增量）
- 测试矩阵
- 已知兼容性差异
- applet 新增、删除、行为变化列表

---

## Part 8: 依赖图与里程碑

### 依赖关系

```
差异测试框架 ──── 无外部依赖
Fuzz 基础设施 ─── 依赖 libFuzzer (clang)
基准测试 ──────── 依赖 BusyBox/GNU 安装
静态分析 ──────── 依赖 clang-tidy, cppcheck
替换测试 ──────── 依赖 QEMU
发布工程 ──────── 依赖 musl-cross-make / zig cc
```

所有轨道可并行推进。

### 里程碑

| 里程碑 | 时间 | 内容 |
|--------|------|------|
| M1 | 第 1-2 周 | 差异测试框架 + P0 applet 差异测试；Fuzz 基础设施 + 初始 target |
| M2 | 第 3-4 周 | clang-tidy 配置 + 初始清理；基准测试套件 P0 applet |
| M3 | 第 5-6 周 | Release 构建脚本；CI pipeline 更新；Changelog 生成 |
| M4 | 第 7-8 周 | QEMU initramfs 替换测试；Alpine minirootfs 替换测试 |
| M5 | 第 9-10 周 | 覆盖率测量 + 缺口填补；体积优化；文档站生成准备；**Release v0.4.0（release candidate）** |

---

## 相关文档

- [生产化路线图](../production-roadmap.md) — 全局阶段顺序和优先级
- [兼容性策略](../compatibility-policy.md) — POSIX/BusyBox/GNU 行为裁决
- [v1.0 生产可用标准](../v1-production-criteria.md) — 最终发布门槛
- [Phase 0 前置门禁](phase-0a-baseline-inventory.md) — Phase 0A-0F 执行细节
