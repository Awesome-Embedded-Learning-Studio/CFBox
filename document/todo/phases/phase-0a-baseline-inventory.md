# Phase 0A: 基线盘点与承诺校准

## 概述

**目标**：在继续新增 applet 前，先把当前代码、profile、文档承诺和兼容性边界盘清楚。Phase 0A 是后续所有 Phase 0 门禁的输入：如果不知道当前真实能力，就无法判断性能、体积、IO、安全和发布工程是否退化。

**进入条件**：v0.1.0 代码库，当前 `document/todo/` 路线图已建立。

**退出条件**：
- applet 清单、成熟度、profile 归属和文档承诺形成单一事实源。
- `minimal`、`rescue`、`container`、`embedded`、`full` profile 的目标 applet 集合明确，并和 CMake profile 计划对齐。
- 现有文档漂移完成审计：README、架构文档、交叉编译文档、todo 路线图不得互相矛盾。
- P0/P1 applet 的 BusyBox/GNU/POSIX 差异进入兼容性矩阵。
- Phase 1 的新增 applet 只能基于本清单分配优先级和体积预算。

**硬门禁**：Phase 0A 未完成时，不得进入 Phase 1 新 applet 实现。

### 当前基线快照

以下数字来自 v0.1.0 代码库的实测结果，作为 Phase 0A 全部工作的基准参照。所有后续 Phase 的回归检测均以本快照为基准线。

| 指标 | 值 | 来源 |
|------|----|------|
| Applet 总数 | 109 | `cmake/Config.cmake` 中 `CFBOX_APPLETS` 列表 |
| APPLET_REGISTRY 条目 | 109 | `include/cfbox/applets.hpp` 中 `APPLET_REGISTRY` 数组 |
| 源文件数 | 105 个 `src/applets/*.cpp` | `ls src/applets/*.cpp \| wc -l` |
| CMake Profile | 4 个：`minimal`、`embedded`、`desktop`、`full` | `cmake/Config.cmake` 第 39 行 CACHE 定义 |
| GTest 单元测试 | 331 个 | `tests/unit/test_*.cpp`（82 个文件） |
| Shell 集成测试 | 54 个 | `tests/integration/test_*.sh`（54 个文件） |
| 测试总计 | 385 个 | 331 GTest + 54 shell |
| 二进制体积 | ~446 KB | size-opt 构建（LTO + strip） |
| 源码结构 | `src/applets/*.cpp`、`include/cfbox/*.hpp`、`tests/unit/test_*.cpp`、`tests/integration/test_*.sh` | 代码库顶层目录 |

---

## Part 1: 事实源

建立 `document/todo/baseline-inventory.md` 或等价表格，至少包含：

| 字段 | 要求 |
|------|------|
| applet | 与 `cmake/Config.cmake`、`include/cfbox/applets.hpp` 一致 |
| 成熟度 | `experimental`、`basic`、`compatible`、`production` |
| profile | `minimal`、`rescue`、`container`、`embedded`、`full` |
| 测试状态 | GTest、integration、QEMU、differential |
| IO 模式 | streaming、bounded-buffer、full-read、interactive |
| 风险标签 | parser、privileged、exec、filesystem、archive、network |
| 兼容性说明 | BusyBox/GNU/POSIX 差异链接 |

### 字段语义

每个字段的取值范围和含义如下：

1. **applet**：命令名称字符串，必须同时出现在 `cmake/Config.cmake` 的 `CFBOX_APPLETS` 列表和 `include/cfbox/applets.hpp` 的 `APPLET_REGISTRY` 数组中。大小写与 CMake 列表一致（小写）。有效值示例：`grep`、`sed`、`awk`。值必须与对应源文件 `src/applets/<name>.cpp` 一一对应。

2. **成熟度**：反映该 applet 的功能完整度和兼容性水平。有效值为：
   - `experimental`——仅有核心路径，无选项或仅部分选项，未做兼容性测试。
   - `basic`——主要选项已实现，基本 smoke 测试通过，但与 BusyBox/GNU 的行为差异未系统验证。
   - `compatible`——已通过 differential smoke 测试，已知差异已记录在兼容性矩阵中。
   - `production`——在所有目标 profile 中经过完整测试，已知差异为零或有明确豁免，可用于生产环境。

3. **profile**：该 applet 被包含的 profile 集合。有效值为当前 CMake profile 列表（`minimal`、`embedded`、`desktop`、`full`）加上路线图规划的 `rescue`、`container`。一个 applet 可属于多个 profile。当标记为 `minimal` 时意味着该 applet 在 `cmake/Config.cmake` 的 `minimal` profile 分支中被设为 `ON`。

4. **测试状态**：描述该 applet 当前已有的测试覆盖类型。有效值为以下组合（可多选）：`GTest`（存在对应的 `tests/unit/test_<name>.cpp`）、`integration`（存在对应的 `tests/integration/test_<name>.sh`）、`QEMU`（跨架构测试）、`differential`（与 BusyBox/GNU 的输出对比测试）。无测试的 applet 标记为 `none`。

5. **IO 模式**：描述该 applet 的标准输入输出处理方式。有效值为：
   - `streaming`——逐行或固定缓冲区处理，内存使用不随输入增长（如 `cat`、`grep`）。
   - `bounded-buffer`——有限缓冲区，但非严格流式（如 `head`、`tail`）。
   - `full-read`——需要将全部输入读入内存后处理（如 `sort`、`diff`）。
   - `interactive`——需要终端交互（如 `sh`、`more`）。

6. **风险标签**：标识该 applet 涉及的安全或可靠性风险域。有效值为以下组合（可多选）：`parser`（复杂输入解析，如 `sed`、`awk`）、`privileged`（需要 root 或 capability，如 `kill`、`mount`）、`exec`（执行外部命令，如 `xargs`）、`filesystem`（文件系统修改操作，如 `rm`、`cp`）、`archive`（归档格式解析，如 `tar`、`unzip`）、`network`（网络操作，未来 applet）。

7. **兼容性说明**：自由文本或链接，记录该 applet 与 BusyBox、GNU coreutils、POSIX 规范的已知行为差异。格式为 `reference:<target>:<category>`，例如 `reference:gnu:acceptable-diff` 表示与 GNU 存在已记录的合理差异。无差异时标记为 `identical`。

### 示例行：grep

以下为 `grep` applet 的完整事实源记录示例：

| 字段 | 值 |
|------|----|
| applet | `grep` |
| 成熟度 | `compatible` |
| profile | `minimal`, `embedded`, `desktop`, `full` |
| 测试状态 | `GTest`, `integration` |
| IO 模式 | `streaming` |
| 风险标签 | `parser` |
| 兼容性说明 | `reference:gnu:acceptable-diff`——POSIX regex (`regex_t`) 替代 GNU 扩展正则；`-P` (Perl regex) 不支持 |

### 源生成命令

初始清单必须从当前源码生成或手工核对：

```bash
# 列出所有 applet 源文件、头文件和测试文件，用于交叉核对完整性
rg --files src/applets include/cfbox tests/unit tests/integration

# 从 CMake 配置中提取 CFBOX_APPLETS 列表和 CFBOX_PROFILE 定义，
# 验证 applet 名称和 profile 选项的一致性
rg -n "CFBOX_APPLETS|CFBOX_PROFILE" cmake/Config.cmake

# 从 applets.hpp 提取 APPLET_REGISTRY 条目和 extern 声明，
# 验证每个 applet 都有 {name, main_func, description} 三元组
rg -n "APPLET_REGISTRY|extern auto" include/cfbox/applets.hpp
```

各命令提取内容说明：

- `rg --files src/applets include/cfbox tests/unit tests/integration`：列出 `src/applets/` 下的实现文件、`include/cfbox/` 下的头文件、`tests/unit/` 和 `tests/integration/` 下的测试文件。通过对比文件名可以快速发现缺失源文件或缺失测试的 applet。

- `rg -n "CFBOX_APPLETS|CFBOX_PROFILE" cmake/Config.cmake`：提取 `CFBOX_APPLETS` 列表（定义所有 applet 名称）和各 profile 分支中的 `CFBOX_ENABLE_*` 设置。用于确认哪些 applet 被包含在哪些 profile 中。

- `rg -n "APPLET_REGISTRY|extern auto" include/cfbox/applets.hpp`：提取 `APPLET_REGISTRY` 常量数组中的每个条目（格式为 `{"name", main_func, "description"}`）以及对应的 `extern auto <name>_main` 声明。用于验证每个 CMake 列表中的 applet 在注册表中都有对应条目。

### 产物位置与校验

Phase 0A 的产物清单文件应生成到以下位置：

```text
document/todo/
├── baseline-inventory.md          # 完整的 applet/profile/maturity 表格
├── baseline-checksum.txt          # 清单文件的 SHA-256，用于检测漂移
└── phases/
    └── phase-0a-baseline-inventory.md  # 本文档
```

**校验方法**：每次 CI 或手动执行时，运行以下命令验证清单与源码一致：

```bash
# 1. 提取当前 CFBOX_APPLETS 列表并计数
cmake_applets=$(rg -o '^\s+(\w+)' cmake/Config.cmake --no-filename | head -n 109 | sort)

# 2. 提取 APPLET_REGISTRY 条目并计数
registry_applets=$(rg '"(\w+)",\s+\w+_main' include/cfbox/applets.hpp -o -r '$1' --no-filename | sort)

# 3. 比较两者差异
diff <(echo "$cmake_applets") <(echo "$registry_applets") && echo "OK: lists match" || echo "DRIFT: lists differ"

# 4. 计算清单文件校验和
sha256sum document/todo/baseline-inventory.md > document/todo/baseline-checksum.txt
```

如果步骤 3 报告 `DRIFT`，说明 `cmake/Config.cmake` 和 `include/cfbox/applets.hpp` 之间存在不一致，必须在 Phase 0A 完成前修复。

---

## Part 2: 当前已知漂移

Phase 0A 必须跟踪并关闭以下已知漂移：

| 漂移 | 当前状态 | 要求 |
|------|----------|------|
| profile 名称 | 路线图承诺 `rescue/container/embedded/full/minimal`，CMake 当前只有 `minimal/embedded/desktop/full` | 明确每个 profile 的 applet 集合和 CMake 落地计划 |
| 质量门禁时序 | benchmark/fuzz/differential 主要放在 Phase 3 | 基础版前移到 Phase 0，Phase 3 保留深化 |
| 正则说明 | `document/cross-compilation.md` 提到 `std::regex`，当前源码使用 POSIX `regex_t` 封装 | 修正文档或记录历史背景 |
| 体积口径 | 文档同时出现 dynamic/static、musl/glibc、stripped/section 等口径 | 统一为 profile + target + link mode + strip state |
| 测试数量 | README、架构文档、todo 文档中的测试数量可能不同 | 建立生成式或每次 release 校准规则 |

### 漂移 1：profile 名称

- **关闭步骤**：
  1. 在 `cmake/Config.cmake` 第 39 行的 `CFBOX_PROFILE` CACHE 定义中添加 `rescue` 和 `container` 选项。
  2. 为 `rescue` profile 定义 applet 集合（shell、权限、挂载、dd、归档、压缩、网络诊断）。
  3. 为 `container` profile 定义 applet 集合（shell、文件、文本处理、下载、脚本兼容）。
  4. 更新 `document/todo/phases/phase-0c-size-profile-budget.md` 中的预算表以包含新 profile。
  5. 更新 README 中的 profile 列表与 CMake 保持一致。
- **验证方法**：
  ```bash
  # 验证 CMake 能识别新 profile
  cmake -B /tmp/test-rescue -DCFBOX_PROFILE=rescue ..
  cmake -B /tmp/test-container -DCFBOX_PROFILE=container ..
  # 验证 profile 文档一致性
  rg "rescue|container" cmake/Config.cmake README.md document/
  ```

### 漂移 2：质量门禁时序

- **关闭步骤**：
  1. 在 Phase 0B 中建立最小 benchmark 集合（`cat`、`grep`、`sed`、`sort`、`find`、`tar`、`gzip`、`cp`、`tail`、`wc`），覆盖吞吐和 RSS。
  2. 在 Phase 0A 期间为核心 applet 建立基础 differential smoke 测试框架（见 Part 3）。
  3. 将 fuzz 测试的最小版本（输入解析 applet 的基础 fuzz）写入 Phase 0E 计划。
  4. 更新 Phase 3 文档，将其定位为"深化"而非"初始实现"。
- **验证方法**：
  ```bash
  # 验证 Phase 0B benchmark 入口存在
  ls tests/benchmark/run_benchmarks.sh
  # 验证 differential 测试框架存在
  ls tests/differential/run_diff_smoke.sh 2>/dev/null || echo "pending"
  # 验证 Phase 3 文档已更新
  rg "Phase 0.*基础" document/todo/phases/
  ```

### 漂移 3：正则说明

- **关闭步骤**：
  1. 确认当前源码中 `std::regex` 的实际使用情况：`rg "std::regex" src/ include/`。
  2. 确认 POSIX `regex_t` 封装位置：`rg "regex_t" src/ include/`。
  3. 更新 `document/cross-compilation.md`，将 `std::regex` 引用改为 POSIX `regex_t` 封装的准确描述。
  4. 如存在历史原因（例如早期版本使用 `std::regex` 后迁移），在文档中添加注释说明迁移背景。
- **验证方法**：
  ```bash
  # 确认源码中无 std::regex 使用
  rg "std::regex" src/ include/ && echo "FOUND: needs doc" || echo "OK: no std::regex"
  # 确认文档描述准确
  rg "regex" document/cross-compilation.md
  ```

### 漂移 4：体积口径

- **关闭步骤**：
  1. 统一体积报告格式为 `<profile>/<target>/<link-mode>/<strip-state>`（与 Phase 0C Part 1 对齐）。
  2. 检查所有文档中的体积引用（`rg "KB|kB|byte|size" document/ README.md`），将不一致的口径统一为标准格式。
  3. 在 `scripts/measure_size.sh` 或等价脚本中输出标准口径的体积报告。
  4. 当前基线值（~446 KB）标注为 `full/x86_64-linux-gnu/size-opt/stripped`。
- **验证方法**：
  ```bash
  # 验证体积报告脚本输出格式
  bash scripts/measure_size.sh build-release/cfbox --format json 2>/dev/null || echo "script pending"
  # 验证文档中无歧义体积引用
  rg "\d+\s*KB" README.md document/
  ```

### 漂移 5：测试数量

- **关闭步骤**：
  1. 建立从源码自动生成测试数量的脚本或 CI 步骤：
     ```bash
     gtest_count=$(ls tests/unit/test_*.cpp | wc -l)
     shell_count=$(ls tests/integration/test_*.sh | wc -l)
     echo "GTest files: $gtest_count, Shell tests: $shell_count"
     ```
  2. 在 CI 中运行 `ctest -N` 获取实际注册的测试数量（当前基线：331 GTest + 54 shell = 385 总计）。
  3. 更新 README 和架构文档中的测试数量为实际值，并添加 `<!-- auto-updated by CI -->` 注释标记。
  4. 在 release checklist 中添加"校准测试数量"步骤。
- **验证方法**：
  ```bash
  # 验证 README 中的测试数量与实际一致
  actual=$(ls tests/unit/test_*.cpp tests/integration/test_*.sh | wc -l)
  stated=$(rg -o '\d+.*测试' README.md | head -1)
  echo "Actual: $actual, Stated: $stated"
  ```

---

## Part 3: 兼容性矩阵最小集

在 Phase 1 前，以下 applet 必须至少有 differential smoke 或明确豁免：

### 文本处理

| applet | 分类 | 说明 |
|--------|------|------|
| `sh` | match / acceptable-diff | shell 兼容性是最高优先级 |
| `test` | match | `[` 和 `test` 的 POSIX 语义 |
| `cat` | match | streaming 基准 |
| `head` | match | bounded-buffer 基准 |
| `tail` | match | bounded-buffer + seek |
| `wc` | match | streaming 计数 |
| `sort` | match | full-read 基准 |
| `uniq` | match | 依赖 sort 的流式处理 |
| `grep` | match / acceptable-diff | POSIX regex vs GNU 扩展正则 |
| `sed` | match / acceptable-diff | 流编辑器，parser 风险高 |
| `awk` | match / acceptable-diff | 完整语言解析器，风险最高 |

### 文件操作

| applet | 分类 | 说明 |
|--------|------|------|
| `find` | match / acceptable-diff | 递归目录遍历 |
| `ls` | match | 列表格式兼容性 |
| `cp` | match | 文件系统修改操作 |
| `mv` | match | 文件系统修改操作 |
| `rm` | match | 文件系统修改操作 |
| `mkdir` | match | 文件系统修改操作 |
| `ln` | match | 符号/硬链接 |

### 归档压缩

| applet | 分类 | 说明 |
|--------|------|------|
| `tar` | match / acceptable-diff | 归档格式解析，archive 风险 |
| `gzip` | match | 压缩 |
| `gunzip` | match | 解压 |

### 系统信息

| applet | 分类 | 说明 |
|--------|------|------|
| `ps` | match / acceptable-diff | `/proc` 解析，格式差异常见 |
| `kill` | match | signal 发送 |
| `df` | match / acceptable-diff | 磁盘信息格式 |
| `du` | match / acceptable-diff | 目录大小计算 |

差异分类：

| 分类 | 含义 |
|------|------|
| match | stdout、stderr、exit code 一致 |
| acceptable-diff | 差异已记录且不破坏目标场景 |
| defect | 必须修复或阻断后续阶段 |
| out-of-scope | v1.0 不承诺，必须公开说明 |

### 测试方法

Differential smoke 测试的执行流程如下：

```bash
# 1. 准备测试数据
mkdir -p /tmp/cfbox-diff/fixtures
# 生成标准测试输入文件（文本、二进制、空文件、大文件等）

# 2. 对每个 applet 运行差异比较
for applet in sh test cat head tail wc sort uniq grep sed awk \
              find ls cp mv rm mkdir ln tar gzip gunzip ps kill df du; do
    # 运行 cfbox 版本
    ./build/cfbox "$applet" <args> > /tmp/cfbox-diff/${applet}.out 2>/tmp/cfbox-diff/${applet}.err
    cfbox_exit=$?

    # 运行 GNU/coreutils 版本
    /usr/bin/"$applet" <args> > /tmp/cfbox-diff/${applet}-gnu.out 2>/tmp/cfbox-diff/${applet}-gnu.err
    gnu_exit=$?

    # 比较结果
    diff /tmp/cfbox-diff/${applet}.out /tmp/cfbox-diff/${applet}-gnu.out && echo "${applet}: stdout OK" || echo "${applet}: stdout DIFF"
    [ "$cfbox_exit" = "$gnu_exit" ] && echo "${applet}: exit OK" || echo "${applet}: exit DIFF ($cfbox_exit vs $gnu_exit)"
done

# 3. 同样与 BusyBox 比较（如已安装）
busybox --list | while read applet; do ...
```

测试场景应覆盖：
- 空输入
- 小文本（1-100 行）
- 大文本（1M+ 行）
- 二进制输入
- Unicode/多字节输入
- 错误路径（不存在的文件、权限拒绝、无效参数）
- 边界条件（空文件、单字节文件、无换行末尾）

### 豁免模板

当某个 applet 无法完成 differential smoke 测试时，必须填写以下豁免模板：

```text
## 兼容性豁免申请

- **applet**: <applet 名称>
- **申请日期**: <YYYY-MM-DD>
- **申请人**: <姓名或角色>

### 当前状态
- 成熟度: <experimental|basic|compatible|production>
- 测试覆盖: <已有的测试类型列表>
- 已知差异: <与 BusyBox/GNU 的已知差异摘要>

### 豁免理由
<说明为何该 applet 暂时无法完成 differential smoke 测试>

### 不阻断 Phase 1 的理由
<说明为何该豁免不会阻塞后续阶段的进展>

### 关闭计划
- 目标 Phase: <Phase 编号>
- 预计关闭日期: <YYYY-MM-DD>
- 关闭步骤: <具体行动项>

### 对用户承诺的影响
<说明该豁免对用户可见行为的影响>

### 审批
- [ ] 技术负责人审批
- [ ] 豁免记录到 document/todo/baseline-inventory.md
```

---

## Part 4: 验收命令

Phase 0A 的验收至少能运行：

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
bash tests/integration/run_all.sh
```

### 环境准备

在运行验收命令前，确保以下环境条件满足：

```bash
# 1. 确认构建工具链
cmake --version    # >= 3.16
g++ --version      # 支持 C++20 或更高
make --version     # GNU Make >= 4.0

# 2. 确认依赖库
# GTest（通常由 CMake FetchContent 自动获取）
# ripgrep（用于源码扫描，非必须但推荐）

# 3. 清理旧构建产物（可选但推荐）
rm -rf build/

# 4. 确认源码完整性
git status         # 确认无未预期的修改
git log --oneline -5  # 确认在正确分支
```

### 交付产物格式要求

Phase 0A 必须输出以下 4 项文档或报告：

#### 产物 1：applet/profile/maturity 清单

- **格式**：Markdown 表格，文件名 `document/todo/baseline-inventory.md`。
- **必须字段**：applet、成熟度、profile 归属、测试状态、IO 模式、风险标签、兼容性说明（7 列完整填写）。
- **行数**：109 行（对应 109 个 applet），不得遗漏。
- **排序**：按 applet 名称字母序。
- **校验**：清单中的 applet 列表必须与 `cmake/Config.cmake` 的 `CFBOX_APPLETS` 完全一致。

#### 产物 2：文档漂移清单和处理状态

- **格式**：Markdown 表格，包含在 `document/todo/baseline-inventory.md` 或独立文件中。
- **必须字段**：漂移描述、影响文档列表、当前状态（`open`/`in-progress`/`closed`）、处理步骤、关闭验证方法。
- **最小条目**：本文件 Part 2 中的 5 个已知漂移。
- **完成标准**：所有漂移状态为 `closed` 或有书面豁免。

#### 产物 3：P0/P1 applet 兼容性矩阵

- **格式**：Markdown 表格，包含在 `document/todo/baseline-inventory.md` 或独立文件中。
- **必须字段**：applet、对比目标（GNU/BusyBox/POSIX）、差异分类（match/acceptable-diff/defect/out-of-scope）、差异描述。
- **最小条目**：本文件 Part 3 中的 25 个 applet。
- **完成标准**：每个 applet 至少有 differential smoke 结果或书面豁免。

#### 产物 4：Phase 1 进入条件检查表

- **格式**：Markdown checklist（`- [ ]` 格式）。
- **内容**：见下方"Phase 1 进入条件检查表"小节。

### Phase 1 进入条件检查表

Phase 0A 完成后，以下检查项必须全部满足才可进入 Phase 1：

```markdown
## Phase 1 进入条件检查表

### 清单与注册一致性
- [ ] `baseline-inventory.md` 中的 109 个 applet 与 `cmake/Config.cmake` 的 `CFBOX_APPLETS` 列表完全一致
- [ ] `baseline-inventory.md` 中的 109 个 applet 与 `include/cfbox/applets.hpp` 的 `APPLET_REGISTRY` 完全一致
- [ ] 每个 applet 的 `src/applets/<name>.cpp` 源文件存在

### Profile 归属
- [ ] `minimal` profile 的 applet 集合已明确列出
- [ ] `embedded` profile 的 applet 集合已明确列出
- [ ] `desktop` profile 的 applet 集合已明确列出
- [ ] `full` profile 的 applet 集合已明确列出
- [ ] `rescue` profile 的 applet 集合已规划（可在后续 Phase 落地）
- [ ] `container` profile 的 applet 集合已规划（可在后续 Phase 落地）

### 文档漂移
- [ ] 漂移 1（profile 名称）：已关闭或有明确计划
- [ ] 漂移 2（质量门禁时序）：已关闭或有明确计划
- [ ] 漂移 3（正则说明）：已关闭或有明确计划
- [ ] 漂移 4（体积口径）：已关闭或有明确计划
- [ ] 漂移 5（测试数量）：已关闭或有明确计划

### 兼容性矩阵
- [ ] 文本处理类（11 个 applet）：differential smoke 完成或有豁免
- [ ] 文件操作类（7 个 applet）：differential smoke 完成或有豁免
- [ ] 归档压缩类（3 个 applet）：differential smoke 完成或有豁免
- [ ] 系统信息类（4 个 applet）：differential smoke 完成或有豁免
- [ ] 所有 `defect` 分类的差异已有修复计划

### 构建与测试
- [ ] `cmake -B build -DCMAKE_BUILD_TYPE=Debug` 成功
- [ ] `cmake --build build` 成功，零 warning（或 warning 已记录）
- [ ] `ctest --test-dir build --output-on-failure` 全部通过（331 GTest）
- [ ] `bash tests/integration/run_all.sh` 全部通过（54 shell tests）
- [ ] 体积测量在 `full` profile + size-opt + LTO + strip 下 <= 446 KB（当前基线）

### 审批
- [ ] 技术负责人确认所有检查项通过
- [ ] Phase 1 进入许可记录到 `document/todo/phases/` 目录
```

---

## 豁免规则

豁免必须包含：

- applet 或文档路径。
- 当前缺口。
- 不阻断 Phase 1 的理由。
- 关闭缺口的目标 Phase。
- 对用户承诺的影响。

### 豁免模板

所有豁免必须使用以下统一模板记录。豁免文件存放于 `document/todo/exemptions/` 目录，文件名格式为 `exemption-<applet-or-area>-<date>.md`。

```markdown
# 豁免记录：<applet 或区域名称>

## 基本信息

| 字段 | 值 |
|------|----|
| 豁免编号 | EXEMPT-<序号> |
| applet / 文档路径 | <具体路径或 applet 名称> |
| 申请日期 | <YYYY-MM-DD> |
| 申请人 | <姓名或角色> |
| 状态 | pending / approved / closed |

## 缺口描述

### 当前状态
<描述当前实际情况，包括已有的测试、功能、文档状态>

### 预期状态
<描述 Phase 0A 完成时应该达到的状态>

### 缺口详情
<明确列出缺少什么：differential smoke 测试？文档更新？profile 归属？>

## 不阻断 Phase 1 的理由

<说明为何该缺口不会阻塞 Phase 1 新 applet 的开发。理由可能包括：
- 该 applet 不在 Phase 1 新增功能的关键路径上
- 该 applet 的成熟度足以支持当前使用场景
- 修复需要的外部依赖尚未就绪
- 修复工作量超出 Phase 0A 范围，需在后续 Phase 处理>

## 关闭计划

| 字段 | 值 |
|------|----|
| 目标 Phase | Phase <编号> |
| 预计关闭日期 | <YYYY-MM-DD> |
| 关闭步骤 | <具体行动项列表> |
| 验证方法 | <如何确认缺口已关闭> |

## 对用户承诺的影响

### 影响范围
<说明哪些用户场景会受到影响>

### 影响程度
- [ ] 无影响（缺口仅在内部质量保障层面）
- [ ] 低影响（边缘场景，有 workaround）
- [ ] 中影响（部分场景行为不一致，但已记录）
- [ ] 高影响（核心场景行为缺失，需特别说明）

### 缓解措施
<如有影响，说明当前的缓解方案>

## 审批记录

| 角色 | 姓名 | 日期 | 结果 |
|------|------|------|------|
| 技术负责人 | | | [ ] approved |
| 质量负责人 | | | [ ] approved |

## 变更历史

| 日期 | 变更内容 |
|------|----------|
| <YYYY-MM-DD> | 豁免创建 |
```
