# Phase 0B: 性能与资源基线

## 概述

**目标**：在新增核心系统能力前建立性能、RSS、启动耗时和回归阈值。CFBox 的目标不是只"能跑"，而是在 rescue、container、embedded 场景中具备 BusyBox 风格的低开销和稳定行为。

**进入条件**：Phase 0A 完成 applet/profile 清单。

**退出条件**：
- 建立 `tests/benchmark/` 或等价 benchmark 入口。
- 至少覆盖 P0 命令：`cat`、`wc`、`grep`、`sed`、`sort`、`find`、`tar`、`gzip`、`cp`、`tail`、`dd`、`sha256sum`。
- 每个 benchmark 输出吞吐、墙钟时间、峰值 RSS、启动耗时、二进制信息。
- CFBox、BusyBox、GNU/Toybox 的参考数据可同机对比。
- CI 或本地门禁能检测性能回归。

**硬门禁**：
- 基线建立后，核心场景性能退化超过 10% 必须阻断或豁免。
- 与 BusyBox 在同类场景差距超过 2-3 倍时，必须阻断、修复或记录原因。
- 峰值 RSS 出现无界增长的命令不得进入生产 profile。

---

## Part 1: Benchmark 范围

| 命令 | 场景 | 参数 | 指标 |
|------|------|------|------|
| `cat` | 10 MB、100 MB 顺序输出 | 10 MB：`dd if=/dev/urandom bs=1M count=10 of=fixture-cat-10m.bin`；100 MB：`dd if=/dev/urandom bs=1M count=100 of=fixture-cat-100m.bin`；输入均为随机字节，不可压缩 | MB/s、RSS |
| `wc` | 10 MB、100 MB 文本 | 10 MB：约 100K 行（`seq 100000 \| while read i; do echo "line $i content here"; done` 生成约 10 MB）；100 MB：约 1M 行；输入含混合行长（短行 10-20 字符，穿插长行 500-1000 字符） | MB/s、lines/s |
| `grep` | 固定字符串、正则、无匹配、大量匹配 | 固定字符串：`grep "patternXYZ"`；正则：`grep -E "[0-9]+abc"`；无匹配：`grep "ZZZZZ_NOT_FOUND"`（确保全量扫描无短路）；大量匹配：`grep "line"` 在全部匹配的输入上 | MB/s、exit code |
| `sed` | `s/X/Y/g`、删除、打印、地址范围 | 替换：`sed 's/oldtext/newtext/g'`（混合内容，50% 行含 "oldtext"）；删除：`sed '/^#/d'`（输入含 30% 注释行）；打印：`sed -n '/important/p'`（约 1% 匹配行）；地址范围：`sed -n '1000,2000p'` | MB/s、RSS |
| `sort` | 100K、1M 行；随机/已排序/重复 | 100K 行随机数：每行一个 `-1000000` 到 `1000000` 之间的随机整数；1M 行随机数：同上规模；已排序：`seq 1000000` 作为最佳情况；50% 重复：随机数中一半为重复值（测试去重和稳定排序路径）；额外场景：逆序输入测试 worst-case | lines/s、RSS |
| `find` | 1K、10K 文件目录树 | 1K 文件 / 100 目录：深度 3-5 层，混合文件类型（普通文件、符号链接、目录）；10K 文件 / 1000 目录：深度 5-8 层，含隐藏文件和不可读目录；额外场景：`find -name "*.c"` 过滤、`find -type f` 类型过滤 | files/s、syscall/墙钟 |
| `tar` | 1K、10K 文件 create/extract | 1K 文件 create/extract：平均文件大小 256B-4KB，含子目录结构；10K 文件 create/extract：平均文件大小 256B-4KB，深层嵌套（最大深度 10）；format 覆盖：默认格式 + gzip 压缩（`tar czf`）；extract 到空目录，测量文件创建吞吐 | files/s、MB/s、RSS |
| `gzip` | 文本和随机数据压缩/解压 | 100 MB 文本（高压缩率）：重复模式文本，预期压缩率 > 80%；100 MB 随机数据（低压缩率）：`/dev/urandom` 生成，预期压缩率 < 5%；解压场景：对上述压缩产物分别解压；额外：`-1`（快速）到 `-9`（最佳压缩）各级别对比 | MB/s、压缩率、RSS |
| `cp` | 大文件、小文件树 | 大文件：100 MB 单文件（`dd if=/dev/urandom bs=1M count=100`）；小文件树：1000 个小文件（平均 512B-2KB）在多层目录结构中；额外场景：`cp -r` 递归复制整棵目录树、`cp -a` 保留属性复制 | MB/s、files/s |
| `tail` | 大文件最后 N 行、pipe 输入 | 1M 行文件最后 10 行：`tail -n 10`；1M 行文件最后 100 行：`tail -n 100`；1M 行文件最后 1000 行：`tail -n 1000`；stdin pipe：`generate_1m_lines \| tail -n 100`；额外：`tail -f` follow 模式的首字节延迟 | latency、RSS |
| `dd` | bs/count、skip/seek、status | `dd bs=1M count=100`：大块顺序读写基准；`dd bs=4K count=25600`：小块等效 100MB 读写；skip/seek：`dd bs=1M skip=10 seek=10 count=80` 跳跃读写；status：`status=progress` 测试进度报告不影响吞吐；额外：`if=/dev/zero of=/dev/null` 纯 memcpy 基准 | MB/s、错误路径 |
| `sha256sum` | 100 MB 文件、stdin | 100 MB 文件：预生成固定 fixture 文件，记录预期 hash；100 MB stdin pipe：`dd if=/dev/urandom bs=1M count=100 \| sha256sum`；额外：空输入、1 字节输入的启动开销占比 | MB/s、RSS |

Phase 1 尚未实现的 `dd`、`sha256sum` 先以占位 benchmark 记录目标口径，落地时必须补齐。

### 参数生成与 Fixture 管理

所有 benchmark fixture 必须可重现：

- **随机种子固定**：使用 `seed=42` 的伪随机生成器，确保跨运行 fixture 一致。
- **Fixture 生成脚本**：`tests/benchmark/fixtures/generate_fixtures.sh` 接受 `--seed` 和 `--size`（quick/full）参数。
- **Fixture 校验**：生成后写入 SHA256 校验和到 `fixtures/manifest.sha256`，benchmark 运行前自动校验。
- **Fixture 版本化**：fixture 格式变更时递增 `fixtures/VERSION`，旧 baselines 自动失效。

---

## Part 2: 输出格式

benchmark 输出 JSON Lines 或 CSV，字段固定：

| 字段 | 说明 |
|------|------|
| command | applet 名称 |
| scenario | 场景名 |
| target | cfbox、busybox、gnu、toybox |
| version | `--version` 或 binary hash |
| input_size | 输入规模（字节） |
| elapsed_ms | 墙钟时间（毫秒） |
| throughput | MB/s、lines/s 或 files/s |
| peak_rss_kb | 峰值 RSS（KB） |
| startup_ms | exec 到首字节输出 |
| exit_code | 退出码 |
| binary_size | 当前 binary 大小（字节） |

### JSON Lines 示例

以下为一条实际数据点示例，展示完整字段和典型值范围：

```json
{"command":"cat","scenario":"10mb-sequential","target":"cfbox","version":"0.1.0","input_size":"10485760","elapsed_ms":42,"throughput":"249.7 MB/s","peak_rss_kb":820,"startup_ms":3,"exit_code":0,"binary_size":456789}
```

字段说明补充：

- `input_size`：字符串类型，值为精确字节数（如 `"10485760"` 而非 `"10 MB"`），便于程序解析和计算。
- `elapsed_ms`：整数，使用 `getrusage` 或 `/usr/bin/time -v` 精确到毫秒级。
- `throughput`：字符串，包含数值和单位（`MB/s`、`lines/s`、`files/s`），单位由 scenario 类型决定。
- `peak_rss_kb`：整数，取自 `/proc/self/status` 的 `VmHWM` 或 `getrusage(RUSAGE_CHILDREN).ru_maxrss`。
- `startup_ms`：整数，测量方式为 pipe 首字节时间戳减去 exec 时间戳，使用 `strace -e trace=write` 或自定义 wrapper 测量。
- `binary_size`：整数，`stat -c %s` 获取，stripped binary 的大小。

### 多 target 对比示例

同一场景下不同 target 的对比数据示例：

```json
{"command":"cat","scenario":"10mb-sequential","target":"cfbox","version":"0.1.0","input_size":"10485760","elapsed_ms":42,"throughput":"249.7 MB/s","peak_rss_kb":820,"startup_ms":3,"exit_code":0,"binary_size":456789}
{"command":"cat","scenario":"10mb-sequential","target":"busybox","version":"1.36.1","input_size":"10485760","elapsed_ms":38,"throughput":"276.0 MB/s","peak_rss_kb":784,"startup_ms":2,"exit_code":0,"binary_size":1105124}
{"command":"cat","scenario":"10mb-sequential","target":"gnu","version":"9.4","input_size":"10485760","elapsed_ms":28,"throughput":"374.4 MB/s","peak_rss_kb":788,"startup_ms":4,"exit_code":0,"binary_size":228392}
```

建议位置：

```text
tests/benchmark/
├── run_benchmarks.sh          # 主入口脚本
├── scenarios/                 # 场景定义（每个命令一个 .sh）
│   ├── cat_scenarios.sh
│   ├── wc_scenarios.sh
│   ├── grep_scenarios.sh
│   ├── sed_scenarios.sh
│   ├── sort_scenarios.sh
│   ├── find_scenarios.sh
│   ├── tar_scenarios.sh
│   ├── gzip_scenarios.sh
│   ├── cp_scenarios.sh
│   ├── tail_scenarios.sh
│   ├── dd_scenarios.sh
│   └── sha256sum_scenarios.sh
├── fixtures/                  # 测试输入数据
│   ├── generate_fixtures.sh
│   ├── manifest.sha256
│   └── VERSION
├── baselines/                 # 基线数据（JSON Lines）
│   ├── current.jsonl          # 当前基线
│   └── historical/            # 历史基线（按日期归档）
└── reports/                   # 对比报告输出
    └── comparison_summary.txt
```

---

## Part 3: 当前重点风险

Phase 0B 必须优先验证：

### 风险 1：全量读入命令在大文件或无限 stdin 下的 RSS 行为

流式命令（`cat`、`head`、`tail`、`grep`）应以固定大小的缓冲区处理输入，不得将整个文件读入内存。全量读入命令（`sort`、`wc -c`）应有明确的上限或分块策略。

**复现命令**：

```bash
# 预期：RSS 不随输入量线性增长，应稳定在数 MB 以内
# 使用 /dev/urandom 模拟无限输入，限制时间防止挂起
timeout 10s ./build/cfbox head < /dev/urandom > /dev/null
# 观察：使用 /usr/bin/time -v 记录 Maximum resident set size
/usr/bin/time -v timeout 10s ./build/cfbox cat < /dev/urandom > /dev/null 2>&1 | grep "Maximum resident"

# 对比测试：10MB vs 100MB vs 500MB，RSS 应保持恒定
for size in 10M 100M 500M; do
    dd if=/dev/urandom bs=1 count=1 of=/tmp/test-$size.bin 2>/dev/null
    truncate -s $size /tmp/test-$size.bin
    /usr/bin/time -v ./build/cfbox cat /tmp/test-$size.bin > /dev/null 2>&1 | grep "Maximum resident"
done
```

**预期问题行为**：如果 `cat` 或 `head` 实现使用了 `read()` 到大缓冲区而非流式 `splice()`/`sendfile()`，RSS 会随输入规模线性增长。

### 风险 2：`grep`、`sed` 的 regex 性能和静态链接体积影响

正则表达式引擎可能导致指数级回溯（catastrophic backtracking），同时静态链接 regex 库会显著增加二进制体积。

**复现命令**：

```bash
# 病态正则：指数回溯测试
# 预期： BusyBox/GNU grep 在 1 秒内完成，CFBox 不应超过 3 秒
echo "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaab" | timeout 5 ./build/cfbox grep -E "(a+)+b"

# 重复输入 + 复杂正则
# 预期：线性时间，不发生指数爆炸
python3 -c "print('a' * 1000)" | timeout 5 ./build/cfbox grep -E "(a+)+$"

# regex 库体积影响
# 预期：strip 后静态二进制中 regex 相关代码段不超过总大小的 15%
size ./build/cfbox
nm --size-sort ./build/cfbox | grep -i regex | tail -20
```

**预期问题行为**：如果使用 NFA 回溯引擎而非 Thompson NFA，对 `(a+)+b` 类模式的匹配时间会随输入长度指数增长。静态链接 GNU regex 可能增加 50-100KB。

### 风险 3：`sort`、`diff`、`comm` 这类天然需要全量数据的命令是否有明确规模边界

这些命令必须将全部输入缓存后才能输出，需要验证内存分配策略和大规模输入下的稳定性。

**复现命令**：

```bash
# 10M+ 行排序测试
# 预期：RSS 增长与输入量呈线性关系（不可避免），但不应超过输入大小的 2 倍
seq 10000000 | shuf | /usr/bin/time -v ./build/cfbox sort > /dev/null 2>&1 | grep "Maximum resident"

# 极端行数测试：20M 行
seq 20000000 | shuf | timeout 120 /usr/bin/time -v ./build/cfbox sort > /dev/null 2>&1

# diff 大文件对比
seq 10000000 > /tmp/diff-a.txt
seq 10000001 > /tmp/diff-b.txt
/usr/bin/time -v ./build/cfbox diff /tmp/diff-a.txt /tmp/diff-b.txt 2>&1 | grep -E "(Maximum resident|wall clock)"
```

**预期问题行为**：如果 `sort` 使用简单的 `std::vector<std::string>` 存储全部行而非外部排序（external sort），10M 行输入（约 100-200MB 文本）将消耗 400MB+ RSS。

### 风险 4：`tar`、`gzip`、`cp` 小文件树场景是否出现过高启动或分配开销

处理大量小文件时，每个文件的 `open()/close()/stat()` 开销累积可能远超实际 I/O 时间。

**复现命令**：

```bash
# 创建 10K 小文件的目录树
mkdir -p /tmp/bench-smallfiles
for i in $(seq 100); do
    mkdir -p /tmp/bench-smallfiles/dir$(printf '%03d' $i)
    for j in $(seq 100); do
        echo "content $i $j" > "/tmp/bench-smallfiles/dir$(printf '%03d' $i)/file$(printf '%03d' $j).txt"
    done
done

# syscall 开销测量
# 预期：cfbox 的 syscall 数量与 BusyBox 在 10% 以内
strace -c ./build/cfbox tar cf /tmp/test.tar -C /tmp/bench-smallfiles . 2>&1 | tail -5
strace -c busybox tar cf /tmp/test.tar -C /tmp/bench-smallfiles . 2>&1 | tail -5

# cp 小文件树
/usr/bin/time -v ./build/cfbox cp -r /tmp/bench-smallfiles /tmp/bench-copy 2>&1 | grep -E "(Maximum resident|wall clock)"

# 对比：启动开销占总时间的比例
# 小文件场景中 startup_ms 应 < 5% 的总 elapsed_ms
```

**预期问题行为**：如果每次文件操作都执行不必要的 `stat()`、`lstat()` 或 `readlink()` 调用，10K 文件场景下总 syscall 数可能达到 BusyBox 的 2-3 倍。

### 风险 5：`find`、`du`、`fuser` 等递归/`/proc` 扫描在大目录或大进程表下是否稳定

这些命令需要遍历文件系统和 `/proc` 虚拟文件系统，大量条目下可能出现描述符泄漏、栈溢出或 OOM。

**复现命令**：

```bash
# /proc 扫描（数千条目）
# 预期：不崩溃、不泄漏 fd、RSS 稳定
/usr/bin/time -v ./build/cfbox find /proc -maxdepth 3 -type f 2>/dev/null | wc -l
/usr/bin/time -v ./build/cfbox du /proc 2>/dev/null | tail -1

# 深层嵌套目录（栈溢出风险）
# 预期：不发生栈溢出，depth >= 500 时仍能正常遍历
mkdir -p /tmp/deep-tree/$(python3 -c "print('/d'.join(['']*600))")
timeout 30 ./build/cfbox find /tmp/deep-tree -type f 2>/dev/null | wc -l

# fuser 在大量进程下
# 预期：线性扫描 /proc/<pid>/fd，RSS 不超过 10MB
./build/cfbox fuser /dev/null 2>/dev/null

# 大量并发 fd 场景
# 预期：不泄漏 fd，遍历完成后 lsof 不显示残留 fd
./build/cfbox find /proc/*/fd -maxdepth 0 2>/dev/null | wc -l
```

**预期问题行为**：如果递归遍历使用递归函数调用而非迭代栈，深度超过系统栈限制（默认 8MB，约 10K-100K 层）时触发 SIGSEGV。如果 `/proc` 扫描没有处理进程消失（`ENOENT`）的情况，会输出大量错误或崩溃。

---

## Part 4: 回归阈值

| 指标 | 阈值 |
|------|------|
| 同场景吞吐 | 不得低于基线 90% |
| 同场景墙钟 | 不得高于基线 110% |
| 峰值 RSS | 不得高于基线 115%，面向流命令不得随输入线性增长 |
| 启动耗时 | 不得高于基线 115% |
| BusyBox 对比 | 差距超过 2-3 倍必须解释 |

### 与 Phase 0C 体积预算的交叉引用

本阶段记录的 `binary_size` 字段直接服务于 [Phase 0C: 体积、Profile 与裁剪预算](phase-0c-size-profile-budget.md) 的体积门禁：

- **binary_size 一致性**：每次 benchmark 运行记录的 `binary_size` 必须与 Phase 0C 的 `scripts/measure_size.sh` 输出一致。偏差超过 1% 触发告警。
- **Profile 关联**：benchmark 按 profile 分组运行（`minimal`/`rescue`/`container`/`embedded`/`full`），每个 profile 的性能基线与体积预算绑定。体积增长导致 profile 预算超限时，即使性能未退化也必须阻断。
- **新增 applet 影响**：Phase 0C 要求每个新增 applet 有体积预算。本阶段要求每个新增 applet 同时建立性能基线。体积增加但无对应性能数据时不得合入。
- **strip 状态标注**：benchmark 的 `binary_size` 须标注 strip 状态（stripped/unstripped），与 Phase 0C 的预算口径 `<profile>/<target>/<link-mode>/<strip-state>` 保持对齐。
- **回归联动**：Phase 0C 体积超预算 10% 时，本阶段自动触发对应 profile 的全量 benchmark 重跑，确认体积增长未带来意外的性能退化或改善。

噪声控制：

- 每个场景至少运行 5 次，记录 median。
- CI 使用小规模快速基准，本地/nightly 使用完整规模。
- 第一次落基线只记录不阻断；第二次开始阻断回归。
- **环境标注**：每条 baseline 记录必须附带环境信息（CPU 型号、内核版本、CPU 频率调节器、是否在容器内运行），确保跨环境可比。
- **离群值处理**：5 次运行中去掉最高和最低值，取中间 3 次的 median。如剩余 3 次的变异系数（CV）超过 15%，标记该场景为"噪声过高"，不纳入回归判定。
- **热缓存/冷缓存**：首次运行为冷缓存（丢弃），后续为热缓存。如需冷缓存数据需显式指定 `--cold-cache` 参数（运行前执行 `sync; echo 3 > /proc/sys/vm/drop_caches`）。

---

## 验收命令

最小验收入口：

```bash
# 快速基准（CI 用，约 2-3 分钟）
bash tests/benchmark/run_benchmarks.sh --target ./build/cfbox --quick

# 完整基准（nightly 用，约 15-30 分钟）
bash tests/benchmark/run_benchmarks.sh --target ./build/cfbox --full

# 多 target 对比
bash tests/benchmark/run_benchmarks.sh --target ./build/cfbox --compare busybox,gnu

# 仅运行特定命令的 benchmark
bash tests/benchmark/run_benchmarks.sh --target ./build/cfbox --commands cat,grep,sort

# 查看回归报告
bash tests/benchmark/run_benchmarks.sh --target ./build/cfbox --check-regression --baseline baselines/current.jsonl
```

验收产物：

- `tests/benchmark/baselines/current.jsonl`：当前基线数据（JSON Lines 格式，每行一条记录）。
- 性能对比摘要：`tests/benchmark/reports/comparison_summary.txt`，包含 CFBox vs BusyBox vs GNU 的逐场景对比表格。
- 超阈值项清单和豁免记录：`tests/benchmark/reports/regression_violations.txt`，每条记录包含命令、场景、偏离百分比、建议操作（修复/豁免/重新基线化）。
- Fixture 校验报告：`tests/benchmark/fixtures/manifest.sha256` 校验结果。
- 环境信息记录：`tests/benchmark/reports/environment_info.json`，包含 CPU、内存、内核版本、编译器版本等信息。
