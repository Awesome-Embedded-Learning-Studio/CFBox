# Phase 0F: CI、可复现构建与调试友好

## 概述

**目标**：让 CFBox 的质量信号可追踪、可复现、可调试。Phase 0F 把"本地能跑"升级为"CI 能证明、release 能复现、用户现场能诊断"。

**进入条件**：Phase 0A-0E 的指标和清单已定义。

**退出条件**：
- CI 输出测试、体积、性能、sanitizer、静态分析的可追踪 artifact。
- Debug、Release、size-opt、static 构建口径统一。
- release artifact 包含版本、构建信息、checksum、size report。
- debug symbols 或 split debug artifact 规则明确。
- 错误格式和诊断输出规范落地到贡献指南。

**硬门禁**：
- CI 不能生成 Phase 0 指标时，不得推进 Phase 1。
- release candidate 没有 checksum、size report、测试矩阵不得发布。
- 新增 applet 没有调试和错误输出规范不得进入 production profile。

---

## Part 1: CI 分层

建议 CI 分层：

| Job | 频率 | 内容 |
|-----|------|------|
| native-debug | 每 PR | Debug + ASan/UBSan + unit/integration |
| release-size | 每 PR | Release size-opt + size report artifact |
| benchmark-quick | 每 PR 或 nightly | P0 小规模 benchmark，检查明显回归 |
| differential-smoke | 每 PR 或 nightly | P0 BusyBox/GNU smoke |
| static-analysis | 每 PR | clang-tidy 低噪声规则、cppcheck 可选 |
| cross-static | 每 PR | aarch64/armhf static build |
| qemu-user | 每 PR | 交叉 binary integration smoke |
| qemu-system | main/release | initramfs/PID 1 boot |
| fuzz-smoke | nightly 或手动 | libFuzzer short run |

Phase 0F 的重点不是一次性跑满所有昂贵任务，而是为每类质量信号建立稳定入口和 artifact。

### GitHub Actions 工作流结构

建议将上述 9 个 job 组织在 `.github/workflows/ci.yml` 中，结构如下：

```yaml
# .github/workflows/ci.yml structure
name: CFBox CI

on:
  push:
    branches: [main]
  pull_request:
    branches: [main]
  schedule:
    - cron: '0 3 * * *'   # nightly at 03:00 UTC
  workflow_dispatch:        # manual trigger

jobs:
  native-debug:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install dependencies
        run: sudo apt-get update && sudo apt-get install -y cmake g++ libc6-dev
      - name: Configure (Debug + Sanitizers)
        run: cmake -B build -DCMAKE_BUILD_TYPE=Debug
                    -DCFBOX_ENABLE_ASAN=ON
                    -DCFBOX_ENABLE_UBSAN=ON
      - name: Build
        run: cmake --build build -j$(nproc)
      - name: Test
        run: ctest --test-dir build --output-on-failure
      - name: Upload sanitizer logs
        if: failure()
        uses: actions/upload-artifact@v4
        with:
          name: sanitizer-logs
          path: build/reports/sanitizer/

  release-size:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure (Release + Size Optimization)
        run: cmake -B build-release
                    -DCMAKE_BUILD_TYPE=Release
                    -DCFBOX_OPTIMIZE_FOR_SIZE=ON
      - name: Build
        run: cmake --build build-release -j$(nproc)
      - name: Measure size
        run: scripts/measure_size.sh build-release/cfbox
      - name: Upload size report
        uses: actions/upload-artifact@v4
        with:
          name: size-report
          path: build-release/reports/size/

  benchmark-quick:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Configure (Release)
        run: cmake -B build-bench -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build-bench -j$(nproc)
      - name: Quick benchmark
        run: bash tests/benchmark/run_benchmarks.sh --quick

  differential-smoke:
    runs-on: ubuntu-latest
    needs: native-debug
    steps:
      - uses: actions/checkout@v4
      - name: Configure (Release)
        run: cmake -B build-diff -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build-diff -j$(nproc)
      - name: Differential smoke tests
        run: bash tests/differential/run_all.sh --smoke

  static-analysis:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install clang-tidy
        run: sudo apt-get install -y clang-tidy
      - name: Configure
        run: cmake -B build-analysis -DCMAKE_BUILD_TYPE=Debug
                    -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
      - name: Run clang-tidy
        run: bash scripts/analysis/clang-tidy.sh

  cross-static:
    runs-on: ubuntu-latest
    strategy:
      matrix:
        target: [aarch64, armhf]
    steps:
      - uses: actions/checkout@v4
      - name: Install cross toolchain
        run: |
          sudo apt-get install -y \
            gcc-${{ matrix.target }}-linux-gnu \
            g++-${{ matrix.target }}-linux-gnu
      - name: Configure (cross static)
        run: cmake -B build-cross
                    -DCMAKE_TOOLCHAIN_FILE=cmake/${{ matrix.target }}.cmake
                    -DCMAKE_BUILD_TYPE=Release
      - name: Build
        run: cmake --build build-cross -j$(nproc)
      - name: Upload cross binary
        uses: actions/upload-artifact@v4
        with:
          name: cfbox-${{ matrix.target }}-static
          path: build-cross/cfbox

  qemu-user:
    runs-on: ubuntu-latest
    needs: cross-static
    strategy:
      matrix:
        target: [aarch64, armhf]
    steps:
      - uses: actions/checkout@v4
      - name: Download cross binary
        uses: actions/download-artifact@v4
        with:
          name: cfbox-${{ matrix.target }}-static
      - name: Install QEMU user-mode
        run: sudo apt-get install -y qemu-user-static
      - name: Integration smoke
        run: bash tests/cross/qemu_user_smoke.sh ${{ matrix.target }}

  qemu-system:
    runs-on: ubuntu-latest
    if: github.ref == 'refs/heads/main'
    needs: cross-static
    steps:
      - uses: actions/checkout@v4
      - name: Install QEMU system
        run: sudo apt-get install -y qemu-system-arm
      - name: Build initramfs
        run: bash tests/cross/build_initramfs.sh aarch64
      - name: Boot and smoke test
        run: bash tests/cross/qemu_system_boot.sh aarch64

  fuzz-smoke:
    runs-on: ubuntu-latest
    if: github.event_name == 'schedule' || github.event_name == 'workflow_dispatch'
    steps:
      - uses: actions/checkout@v4
      - name: Configure with fuzzing
        run: cmake -B build-fuzz
                    -DCMAKE_BUILD_TYPE=Debug
                    -DCFBOX_ENABLE_FUZZING=ON
      - name: Build fuzzers
        run: cmake --build build-fuzz -j$(nproc)
      - name: Short fuzz run
        run: bash tests/fuzz/run_smoke.sh --seconds 30
```

### Job 依赖图

```
native-debug ──────┐
                   ├──► differential-smoke
release-size       │
                   │
cross-static ──────┼──► qemu-user
      │            │
      └────────────┼──► qemu-system (main branch only)
                   │
static-analysis   (独立)
benchmark-quick   (独立)
fuzz-smoke        (schedule / manual only)
```

**依赖关系说明**：

- `differential-smoke` 依赖 `native-debug` 通过，确保基础测试健康后再跑差分测试。
- `qemu-user` 依赖 `cross-static` 完成并产出交叉编译 binary。
- `qemu-system` 依赖 `cross-static`，且仅在 main 分支触发。
- `static-analysis`、`benchmark-quick`、`fuzz-smoke` 为独立 job，不依赖其他 job 的结果。
- `fuzz-smoke` 仅在 nightly schedule 或手动触发时运行，不在普通 PR 上消耗资源。

---

## Part 2: 可复现构建

记录构建元数据：

| 字段 | 要求 |
|------|------|
| git commit | release 和 CI artifact 必须记录 |
| compiler | 名称、版本、target |
| CMake | 版本、配置参数 |
| profile | `minimal/rescue/container/embedded/full` |
| target | `x86_64-linux-musl` 等 |
| link mode | static/dynamic |
| strip state | stripped/unstripped |
| dependencies | GTest、toolchain、第三方来源 |

建议新增：

```text
build/reports/build-info.json
build/reports/test-matrix.json
build/reports/size/*.json
```

### build-info.json 完整示例

构建完成后，CMake 脚本应自动生成 `build/reports/build-info.json`，格式如下：

```json
{
  "git_commit": "abc1234",
  "git_branch": "main",
  "compiler": "gcc 13.2.0",
  "compiler_target": "x86_64-linux-gnu",
  "cmake_version": "3.28.1",
  "cmake_config": "-DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON",
  "profile": "full",
  "target": "x86_64-linux-musl",
  "link_mode": "static",
  "strip_state": "stripped",
  "dependencies": {"gtest": "1.14.0"}
}
```

**字段说明**：

- `git_commit`：完整构建对应的 commit SHA（短格式 7 位即可，release 使用完整 40 位）。
- `git_branch`：构建时的分支名，用于溯源。
- `compiler`：编译器名称和版本号，格式 `<name> <major>.<minor>.<patch>`。
- `compiler_target`：编译器的默认 target triplet，如 `x86_64-linux-gnu`、`aarch64-linux-gnu`。
- `cmake_version`：CMake 版本，确保构建工具链可追溯。
- `cmake_config`：完整的 CMake 配置命令行参数，用于一键复现。
- `profile`：当前构建使用的 CFBox profile，取值为 `minimal/rescue/container/embedded/full`。
- `target`：目标平台 triplet，如 `x86_64-linux-musl`、`aarch64-linux-musl`。
- `link_mode`：链接方式，`static` 或 `dynamic`。
- `strip_state`：binary 是否经过 strip，`stripped` 或 `unstripped`。
- `dependencies`：第三方依赖的名称和版本号映射表。

**生成方式**：在 CMake 的 configure 阶段通过 `configure_file()` 从 `.cmake.in` 模板生成，确保每次构建自动记录。

### test-matrix.json 结构示例

```json
{
  "timestamp": "2026-05-17T03:00:00Z",
  "total_tests": 256,
  "passed": 254,
  "failed": 0,
  "skipped": 2,
  "duration_seconds": 42.3,
  "applets_tested": ["cp", "mv", "grep", "cat", "ls", "..."],
  "profiles_tested": ["full"],
  "sanitizer_results": {
    "asan": "clean",
    "ubsan": "clean"
  }
}
```

### 报告目录完整结构

```text
build/reports/
├── build-info.json          # 构建元数据（上述）
├── test-matrix.json         # 测试矩阵汇总
├── size/
│   ├── cfbox-size.json      # binary 体积明细
│   ├── section-sizes.txt    # ELF section 大小
│   └── applet-sizes.json    # 各 applet 符号大小
├── sanitizer/
│   ├── asan.log             # ASan 输出日志
│   └── ubsan.log            # UBSan 输出日志
├── coverage/
│   └── coverage-summary.txt # 覆盖率摘要
└── benchmark/
    └── quick-results.json   # 快速 benchmark 结果
```

release artifact 必须包括：

- binary。
- SHA-256 checksum。
- size report。
- test matrix。
- changelog。
- SBOM 计划或实际 SBOM。

### release artifact 目录结构

```text
release/
├── cfbox-x86_64-linux-musl-static-stripped
├── cfbox-x86_64-linux-musl-static-stripped.sha256
├── cfbox-aarch64-linux-musl-static-stripped
├── cfbox-aarch64-linux-musl-static-stripped.sha256
├── reports/
│   ├── build-info.json
│   ├── test-matrix.json
│   └── size/
│       ├── cfbox-size.json
│       └── section-sizes.txt
├── CHANGELOG.md
└── SBOM.spdx.json
```

---

## Part 3: 调试友好

调试构建要求：

- Debug 默认保留 `-g3 -O0 -fno-omit-frame-pointer`。
- sanitizer failure 上传日志。
- release 可选 `*-debug` 或 split debug symbols。
- core parser 和归档/压缩路径保留足够错误上下文。
- `--version` 输出能定位版本；release notes 记录 build profile。

错误信息规范：

```text
cfbox <applet>: <operation> '<target>': <errno message>
cfbox <applet>: invalid <field> '<value>'
cfbox <applet>: <feature> is not supported in <context>
```

### 错误格式详细说明

三种规范格式分别对应不同场景：

**模式 1 — 系统调用/IO 错误**：`cfbox <applet>: <operation> '<target>': <errno message>`

适用于文件操作失败、权限不足、设备不存在等涉及 errno 的场景。`<operation>` 使用小写动词（如 `cannot stat`、`cannot open`、`cannot read`、`cannot write`），`<target>` 用单引号包裹实际路径或对象名，`<errno message>` 使用 `strerror(errno)` 的输出。

**模式 2 — 参数/输入验证错误**：`cfbox <applet>: invalid <field> '<value>'`

适用于用户传入非法选项、参数格式错误等场景。`<field>` 描述字段类型（如 `option`、`mode`、`count`），`<value>` 用单引号包裹实际值。

**模式 3 — 功能/上下文限制**：`cfbox <applet>: <feature> is not supported in <context>`

适用于功能在特定环境下不可用的场景。`<feature>` 描述被限制的功能（如 `mount 'proc'`、`pivot_root`），`<context>` 描述限制上下文（如 `container context`、`minimal profile`、`static build`）。

### 正确示例（Good）

以下输出符合规范：

```text
cfbox cp: cannot stat '/no/such/file': No such file or directory
cfbox grep: invalid option '--foo'
cfbox mount: 'proc' is not supported in container context
cfbox chmod: cannot stat '/root/secret': Permission denied
cfbox cat: cannot open '/dev/nullw': No such device or address
cfbox ls: invalid mode 'xyz'
cfbox dd: 'direct' is not supported in embedded profile
cfbox tar: cannot write '/mnt/ro/archive.tar': Read-only file system
cfbox wc: invalid count '-1'
cfbox init: 'switch_root' is not supported in rescue profile
```

### 禁止示例（Bad）

以下输出严禁出现在默认输出中：

```text
read error                          # 缺少 applet 名和目标路径
Segmentation fault                  # 对用户输入异常直接崩溃
DEBUG: buffer=0x7fff...             # debug-only 信息泄漏到默认 stdout
Error: failed                       # 过于笼统，无 applet、无操作、无目标
Segmentation fault (core dumped)    # 用户正常输入触发崩溃，不可接受
open: No such file                  # 缺少 applet 前缀
unexpected error                    # 完全无上下文
[ERROR] code=22                     # 不符合 cfbox 格式规范
```

### 错误输出路由规则

| 条件 | 输出目标 | 说明 |
|------|---------|------|
| 正常操作结果 | stdout | 命令的正常输出 |
| 用户错误（用法、参数） | stderr, exit 1 | 格式化的错误信息 |
| 系统错误（IO、权限） | stderr, exit > 1 | 含 errno 描述的错误信息 |
| 内部断言/不可恢复 | stderr, exit 不可恢复码 | 仅 debug 构建输出 backtrace |
| 诊断/调试信息 | stderr + `--verbose` flag | 不在默认输出中出现 |

禁止：

- 只输出 `read error` 而不含目标。
- 对用户输入异常直接崩溃。
- 把 debug-only 信息泄漏到默认 stdout。
- 对同一类错误使用多种不兼容格式。

---

## Part 4: 静态分析起步

Phase 0F 启用低噪声规则，不做大规模风格重写。

优先级：

1. ASan/UBSan：Debug 必跑。
2. clang-tidy：`bugprone-*`、`performance-*` 中低噪声子集。
3. cppcheck：先作为报告，不立即全量阻断。
4. include-what-you-use：Phase 3 再作为长期治理。

### 优先级详细说明

**P0 — ASan/UBSan（Debug 必跑）**

- 每次 Debug 构建默认启用，不可关闭。
- 检测范围：内存越界、use-after-free、未定义行为、整数溢出。
- 发现问题时 CI job 标记为 failure，日志上传为 artifact。
- 本地开发者可通过 `cmake -DCFBOX_ENABLE_ASAN=ON` 复现。

**P1 — clang-tidy 低噪声子集**

启用规则清单（在 `.clang-tidy` 中配置）：

```yaml
Checks: >
  -*,
  bugprone-*,
  -bugprone-easily-swappable-parameters,
  -bugprone-narrowing-conversions,
  performance-*,
  -performance-no-int-to-ptr,
  modernize-use-nullptr,
  modernize-use-auto,
  modernize-use-override,
  modernize-use-using,
  cppcoreguidelines-pro-type-static-cast-downcast,
  readability-redundant-string-cstr
```

- CI 中作为非阻断警告（`continue-on-error: true`），记录到 artifact。
- 逐步提升为阻断规则。

**P2 — cppcheck（报告模式）**

- 以 `--enable=warning,performance` 运行，不启用 `style` 和 `unusedFunction`。
- CI 输出为报告文件，不阻断合并。
- 建议在 nightly 构建中运行并归档。

**P3 — include-what-you-use（长期治理）**

- Phase 0F 不启用。
- Phase 3 作为 include 治理计划的一部分引入。

建议脚本：

```text
scripts/analysis/
├── clang-tidy.sh
├── cppcheck.sh
├── sanitizers.sh
└── coverage.sh
```

### 分析脚本职责

| 脚本 | 输入 | 输出 | 用途 |
|------|------|------|------|
| `clang-tidy.sh` | `compile_commands.json` | `build/reports/clang-tidy.log` | 静态分析报告 |
| `cppcheck.sh` | 源码目录 | `build/reports/cppcheck.log` | 补充静态分析 |
| `sanitizers.sh` | 测试二进制 | `build/reports/sanitizer/*.log` | sanitizer 日志归档 |
| `coverage.sh` | GCDA 文件 | `build/reports/coverage/` | 覆盖率报告 |

---

## Part 5: 文档和开发者体验

更新贡献指南：

- 新 applet 必须声明 IO 类型、体积预算、兼容差异、安全风险。
- 新 parser/格式解析器必须有 fuzz smoke。
- 新 profile 变更必须更新 size report。
- 新特权命令必须有隔离测试和权限文档。

### 贡献检查清单

新 PR 提交时，贡献者应对照以下清单自检：

```markdown
## Applet PR 检查清单
- [ ] 声明 IO 类型（stdio / filesystem / network / device）
- [ ] 声明体积预算（minimal/rescue/container/embedded/full 各 profile 的预期大小）
- [ ] 声明与 BusyBox/GNU coreutils 的兼容差异
- [ ] 声明安全风险（特权操作、文件系统修改、设备访问）
- [ ] 错误输出符合 cfbox 错误格式规范（见 Part 3）
- [ ] 单元测试覆盖核心逻辑
- [ ] 集成测试覆盖典型用例
- [ ] CI 所有 job 通过
```

```markdown
## Parser/格式解析器 PR 检查清单
- [ ] fuzz smoke 测试已添加（tests/fuzz/ 目录下）
- [ ] 边界条件测试（空输入、超大输入、畸形输入）
- [ ] sanitizer 构建下运行无报错
- [ ] 错误路径有足够上下文信息
```

```markdown
## Profile 变更 PR 检查清单
- [ ] 更新 size report（scripts/measure_size.sh）
- [ ] 更新 profile 文档（document/profiles.md）
- [ ] 确认其他 profile 不受影响
```

```markdown
## 特权命令 PR 检查清单
- [ ] 隔离测试（chroot / namespace 模拟）
- [ ] 权限文档（document/privileges.md）
- [ ] capability 需求声明
- [ ] 降级路径（非 root 环境的 fallback 行为）
```

更新用户文档：

- profile 选择。
- 静态/动态链接区别。
- debug artifact 使用方式。
- 已知限制和兼容性差异。

### 用户文档结构

建议在 `document/` 目录下维护以下文档：

```text
document/
├── profiles.md           # profile 选择指南
├── building.md           # 构建、配置、链接方式说明
├── debugging.md          # debug artifact 使用方式
├── compatibility.md      # BusyBox/GNU 兼容性差异
└── limits.md             # 已知限制
```

**`profiles.md` 要点**：

- 各 profile 包含的 applet 列表。
- 典型使用场景推荐。
- 体积对比表格。

**`building.md` 要点**：

- 静态链接 vs 动态链接的取舍。
- 交叉编译工具链设置。
- musl vs glibc 选择建议。

**`debugging.md` 要点**：

- 如何使用 split debug symbols（`objdump --debugging`、`gdb`）。
- 如何解读 sanitizer 日志。
- `--version` 输出的各字段含义。
- 如何从 `build-info.json` 复现特定构建。

**`compatibility.md` 要点**：

- 与 BusyBox 的已知行为差异列表。
- 与 GNU coreutils 的已知行为差异列表。
- 差异的成因和修复计划。

---

## 验收命令

最小验收：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
bash tests/integration/run_all.sh
cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON
cmake --build build-release
scripts/measure_size.sh build-release/cfbox
```

### 验收检查点

最小验收应确认以下内容：

1. Debug 构建成功，sanitizer 无报错。
2. 全部单元测试和集成测试通过。
3. Release size-opt 构建成功。
4. `measure_size.sh` 生成有效的 size report JSON。
5. `build/reports/build-info.json` 自动生成且字段完整。

可选/后续验收：

```bash
bash scripts/analysis/clang-tidy.sh
bash tests/benchmark/run_benchmarks.sh --quick
bash tests/differential/run_all.sh --smoke
bash tests/fuzz/run_smoke.sh --seconds 30
```

Phase 0F 完成后，CI 应能回答：这次改动是否破坏测试、体积、性能、兼容性、安全扫描和调试可定位性。
