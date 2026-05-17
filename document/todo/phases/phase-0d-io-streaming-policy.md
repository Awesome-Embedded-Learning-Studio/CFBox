# Phase 0D: IO、流式处理与内存策略

## 概述

**目标**：确保 CFBox 的核心命令具备 BusyBox 风格的 IO 友好性。面向流的命令必须能处理大文件、管道和无限输入；必须全量读入的命令要有边界、理由和测试。

**进入条件**：Phase 0A 完成 applet 风险标签，Phase 0B 建立 RSS 测量口径。

**退出条件**：
- 所有 P0/P1 applet 标注 IO 模式：streaming、bounded-buffer、full-read、interactive。
- `read_all`、`read_all_stdin`、`read_lines` 使用点完成审计。
- 面向流的核心命令完成整改或建立豁免。
- 大文件、管道、短读、写失败、broken pipe、权限错误均有测试策略。
- Phase 1 新增 applet 必须先声明 IO 模式。

**硬门禁**：
- 面向流的大文件命令不得无理由全量读入。
- stdin 无限流场景必须可控，不得默认耗尽内存。
- 文件复制、压缩、摘要、编码等命令必须设计为分块处理。

---

## Part 1: IO 分类

| 类型 | 说明 | 示例 |
|------|------|------|
| streaming | 输入可逐块或逐行处理，RSS 不随输入线性增长 | `cat`、`wc`、`grep`、`head`（整改后）、`md5sum`（整改后）、`cksum`、`od`（整改后）、`tee`、`fold`、`expand`、`cut`、`nl`、`tr`（整改后） |
| bounded-buffer | 需要窗口或缓冲，但有固定上限 | `tail`、未来 `dd`、`shuf` |
| full-read | 算法天然需要全量输入或当前实现暂时如此 | `sort`、`diff`、`comm`、`paste`、`split`（临时）、`xargs`（临时） |
| interactive | 终端驱动，关注响应和状态恢复 | `top`、`more`、`ed`、`watch` |

每个 applet 的帮助文档或内部清单必须记录 IO 类型。

### 各类型详细说明

#### streaming（流式）

流式命令是 CFBox 中最理想的 IO 模式。其核心特征是：无论输入多大，内存占用始终恒定或与输入规模无关。实现上通常采用固定大小的读缓冲区（如 8 KiB–64 KiB），在循环中逐块读取、处理、写出，直到 EOF 或提前终止条件满足。

判断一个命令是否属于 streaming 的关键标准：
- 算法不需要"看到"后续输入即可处理当前输入。
- 输出可以在输入尚未读完时持续产生。
- 可以安全地从管道、套接字、`/dev/urandom` 等无限源读取。

**已实现的流式命令**：`cat`、`wc`、`grep`——这些命令在当前代码中已采用分块或逐行处理，可作为后续整改的参考模板。

**待整改为流式的命令**：
- `head`——读取到第 N 行后立即停止，无需继续读入。
- `md5sum`——哈希摘要天然支持增量更新（`update` 接口），只需将全量读入改为分块调用 `update`。
- `cksum`——同 `md5sum`，CRC 校验也支持逐块累积。
- `od`——当指定 `-n`（只输出 N 字节）或 `-s`（只输出 N 个字符串）时，应提前停止；未指定限制时仍为流式逐块输出。
- `tee`——从 stdin 逐块读取并写出到 stdout 及所有目标文件，天然流式。
- `fold`——逐行读取，按指定宽度折行输出，单行缓冲即可。
- `expand`——逐行读取，将制表符替换为空格后输出，单行缓冲即可。
- `cut`——逐行读取，提取指定字段或字符后输出，单行缓冲即可。
- `nl`——逐行读取，添加行号后输出，单行缓冲即可。
- `tr`——逐块或逐行读取，通过固定查找表（256 条目）进行字符替换，天然适合流式处理。

#### bounded-buffer（有界缓冲）

有界缓冲命令需要保存一定量的输入数据，但缓冲区大小有固定的上限，不随输入规模线性增长。典型场景是需要查看"尾部"或维护固定窗口的命令。

- `tail`——维护一个固定大小的环形缓冲区（ring buffer），仅保存最后 N 行。即使输入无限增长，缓冲区大小恒定。`-f`/`-F` 模式需要文件监控（inotify/poll）以检测追加内容。
- `dd`——使用用户指定的块大小（`bs`）进行读写，缓冲区大小等于 `bs`，不随输入变化。属于未来计划实现的命令。
- `shuf`——如果输入行数在可接受范围内（如 `-n` 限制），可全量读入；否则使用蓄水池抽样（reservoir sampling）算法，仅保留固定大小的样本集。

#### full-read（全量读入）

全量读入命令的算法本质要求看到完整输入后才能产出结果。对于这类命令，不强制整改为流式，但必须满足以下约束：
- 文档明确记录最大可处理规模。
- benchmark 记录峰值 RSS。
- 超过规模限制时给出清晰的错误信息，不得静默崩溃或耗尽内存。

- `sort`——排序算法需要所有数据在场才能确定全局顺序。当前实现可接受，但应记录 RSS 基线。
- `diff`——差异比较算法（LCS/Myers）需要访问全部行。当前实现可接受，但需规模边界说明和 RSS 基线测试。
- `comm`——对两个已排序文件进行逐行比较，需要同时持有两文件行数据。可接受但需大文件说明和 RSS 测试。
- `paste`——需要并行读取多个文件的对应行，需持有每文件当前行。当文件数量较多时需关注 RSS。
- `split`——当前实现可能临时全量读入，但可整改为流式（见 Part 2）。
- `xargs`——当前实现可能全量读入 stdin，但可整改为流式 token 批处理（见 Part 2）。

#### interactive（交互式）

交互式命令直接驱动终端，关注响应延迟、键盘输入处理和屏幕状态恢复。这类命令通常不处理管道输入，内存策略以终端缓冲为主。

- `top`——定期刷新系统状态显示，关注刷新频率和 CPU 开销。
- `more`——分页显示，需要终端尺寸检测和键盘输入处理。
- `ed`——行编辑器，维护编辑缓冲区，关注缓冲区管理。
- `watch`——定期执行命令并全屏显示输出，关注子进程管理和屏幕刷新。

---

## Part 2: 当前全量读入审计清单

以下使用点必须在 Phase 0D 中分类处理：

| 命令 | 当前模式 | 要求 | 整改策略 |
|------|----------|------|----------|
| `head` | full-read | 改为逐块/逐行提前停止 | **读取 N 行后立即停止**：使用 `for_each_line` 逐行读取，行计数器达到 N 后 callback 返回 `false` 中断读取。对于字节模式（`-c`），使用 `read_chunks` 并累计已读字节数，达到指定字节数后停止。避免将剩余输入读入内存。 |
| `tail` | full-read | 改为 bounded ring buffer；`-f/-F` 预留策略 | **环形缓冲区保存最后 N 行**：分配固定大小的 `std::deque<std::string>` 作为环形缓冲区，逐行读取时持续入队并在超过 N 行时出队。读取完成后从缓冲区输出。对于 `-f`/`-F` 模式，使用 `inotify`（Linux）或 `poll` 作为文件监控机制，检测到文件追加时读取新增内容并输出。多文件场景（多个 `-f`）需考虑 `poll`/`epoll` 多路复用。 |
| `sed` | full-read | 常规命令流式处理；仅 `$`/全局地址需要特殊策略 | **按地址类型分流**：对于行号地址（如 `3d`）、正则地址（如 `/pattern/d`）、范围地址（如 `2,5d`），采用逐行流式处理，每读一行判断是否匹配并执行命令。仅在以下情况需要缓冲：(1) `$` 地址（最后一行）需要等待 EOF 才能确认；(2) `P` 命令操作 hold space 多行内容时。建议实现时维护一个标志位 `needs_full_buffer`，在编译命令列表时根据是否包含 `$` 地址或复杂 hold space 操作来设置。 |
| `gzip`/`gunzip` | full-read | 改为分块压缩/解压或记录短期豁免 | **流式 deflate/inflate**：检查现有 `compress.hpp` 是否支持分块接口。压缩时使用固定缓冲区（如 64 KiB）循环读取输入，调用 `deflate` 的 `Z_NO_FLUSH`/`Z_FINISH` 模式，将压缩输出写出到目标。解压时类似，使用 `inflate` 逐块解压。关键点：确保 zlib/bz2/lzma 的流式 API 被正确使用，而非一次性调用。 |
| `tar`/`cpio`/`ar` | full-read | 大文件 entry 分块读写；归档索引有边界 | **流式 entry 迭代**：读取归档头（header）获取 entry 元数据（文件名、大小、权限等），根据元数据决定是否处理该 entry。对于文件数据部分，使用固定缓冲区（如 64 KiB）分块读写，不将整个文件加载到内存。创建归档时同样分块读取源文件并写入归档。对于 `tar` 的 `-t`（列出）操作，只需读取 header 跳过数据块，完全不加载文件内容。 |
| `split` | full-read | 按行/字节流式输出 | **流式读取 + 计数器分流**：逐行或逐块读取输入，维护行计数器（`-l` 模式）或字节计数器（`-b` 模式）。当前计数器达到分割阈值时关闭当前输出文件并打开下一个。不持有已读数据，仅维护计数器状态。 |
| `xargs` | full-read stdin | 按 token 流式批处理，支持最大参数限制 | **流式 token 读取器**：逐行从 stdin 读取，按空白分割为 token，将 token 填入参数列表。当参数列表达到限制（`-n` 指定的每次最大参数数，或系统 `ARG_MAX`）时执行目标命令并清空参数列表，然后继续读取后续 token。关键：不需要等待 stdin 全部读完才开始执行，实现"边读边执行"的批处理模式。 |
| `tr` | full-read stdin | 改为分块转换 | **固定查找表 + 逐块处理**：在启动时根据命令行参数构建一个 256 条目的查找表（`unsigned char table[256]`），将每个输入字节映射为目标字节。然后使用 `read_chunks` 逐块读取输入，对每个字节通过查找表替换后写出。查找表构建是一次性 O(1) 操作，后续处理完全流式，无需任何状态。对于补集（`-c`）和截断（`-t`）模式，查找表构建逻辑稍有不同，但运行时行为相同。 |
| `cmp` | full-read 双文件 | 改为并行块比较，首次差异即可停止 | **并行块读取 + 首差停止**：同时打开两个文件，使用固定缓冲区（如 8 KiB）分别读取对应块，逐字节比较。发现首个差异时输出比较结果并返回退出码。如果块大小不同，说明文件长度不同，也视为差异。对于使用 `-l`（列出所有差异）模式的场景，仍可逐块处理但需继续读取直到两个文件都到达 EOF。 |
| `comm` | read_lines | 可接受但需大文件说明和 RSS 测试 | **保持全量读入，增加边界保护**：算法要求两个文件均已排序，逐行比较需要同时持有行数据。整改方向：(1) 文档记录最大可处理文件大小；(2) benchmark 记录峰值 RSS；(3) 超过合理大小时输出警告或错误。长期可考虑外部排序 + 归并比较策略，但非 Phase 0D 范围。 |
| `diff` | read_lines | 算法可全量读入，但需规模边界和 RSS 基线 | **保持全量读入，增加边界保护**：Myers/LCS diff 算法天然需要全量数据。整改方向：(1) 文档记录最大可处理文件大小和行数限制；(2) benchmark 测量不同规模下的峰值 RSS；(3) 对于超大文件（如超过 100 万行），输出警告建议使用 `diff --speed-large-files` 等优化策略（如果实现）。 |
| `patch` | read_all/read_lines | parser 风险，需 fuzz 和尺寸边界 | **增强 parser 健壮性**：patch 文件的解析器是高风险区域，恶意或畸形的 patch 文件可能导致缓冲区溢出或内存耗尽。整改方向：(1) 使用 fuzz 测试（libFuzzer/AFL）覆盖 parser 路径；(2) 设置文件大小上限（如 64 MiB）；(3) 对 hunk 头部的行号和偏移量进行范围检查；(4) 单个 hunk 的上下文行数设上限。 |
| `md5sum`/`cksum`/`sum` | full-read | 改为流式摘要/校验 | **流式哈希更新**：哈希摘要算法（MD5、CRC32、System V checksum）天然支持增量更新。将当前全量读入改为 `read_chunks` 循环，每读一块数据调用哈希的 `update` 方法（如 `MD5_Update`、CRC32 累积计算）。读完全部数据后调用 `finalize` 获取最终摘要。内存占用从"整个文件大小"降为"固定缓冲区大小 + 哈希上下文"（通常 < 1 KiB）。 |
| `unzip` | full-read archive | 短期豁免需记录 zip 大小边界 | **记录 zip 大小边界作为短期豁免**：zip 格式的中央目录（central directory）位于文件末尾，定位它需要从文件尾部向前搜索 end-of-central-directory 记录。这使得完全流式处理 zip 文件较为困难。短期方案：(1) 文档记录最大支持的 zip 文件大小（如 2 GiB）；(2) benchmark 记录峰值 RSS；(3) 提取单个文件时仍可流式处理该 entry 的数据块。长期方案：实现 zip64 支持和按需加载中央目录。 |
| `od` | full-read | 改为分块，尊重 `-n/-s` | **带限制的流式输出**：使用 `read_chunks` 逐块读取输入，按指定格式（八进制、十六进制等）转换后输出。当指定了 `-n`（只解读 N 个输入字节）或 `-s`（只输出 N 个字符串）时，维护字节/字符串计数器，达到限制后停止读取。未指定限制时仍为流式逐块处理直到 EOF。 |

允许保留 full-read 的命令必须满足：

- 算法需要随机访问或全量排序。
- 文档写明规模边界。
- benchmark 记录 RSS。
- 大输入失败时错误清晰，不崩溃。

### 审计优先级排序

根据用户影响和整改难度，建议按以下顺序处理：

1. **高优先级、低难度**：`head`、`tr`、`md5sum`/`cksum`/`sum`——整改方案明确，工作量小，用户高频使用。
2. **高优先级、中难度**：`tail`、`cmp`、`od`——需要一定的缓冲管理逻辑，但方案清晰。
3. **中优先级、中难度**：`sed`、`gzip`/`gunzip`、`split`、`xargs`——涉及更复杂的状态管理或外部库集成。
4. **中优先级、高难度**：`tar`/`cpio`/`ar`——需要重构 entry 迭代逻辑，影响面广。
5. **低优先级（豁免为主）**：`comm`、`diff`、`patch`、`unzip`——以文档记录、边界保护、fuzz 测试为主，不强制整改为流式。

---

## Part 3: 基础设施要求

扩展 `include/cfbox/io.hpp` 或新增轻量工具：

| 能力 | 要求 |
|------|------|
| 块读取 | `read_chunks(path, buffer_size, callback)` |
| 块写入 | 处理短写、EINTR、EPIPE |
| 行读取 | 保留长行策略，避免单行无限增长无提示 |
| stdin/stdout | 统一 `-` 语义 |
| 错误上下文 | 包含操作、路径、errno |
| 测试夹具 | 大文件、pipe、broken pipe、权限拒绝 |

### 具体函数签名设计

#### 块读取接口

分块读取是流式处理的基础。该接口从指定文件路径读取数据，每次填充一块缓冲区后调用回调函数。回调返回 `false` 表示提前终止读取。

```cpp
namespace cfbox::io {

/// 分块读取文件内容。
///
/// 从 path 指定的文件中读取数据，每次读取最多 buffer_size 字节到内部缓冲区，
/// 然后调用 callback 处理该块数据。callback 返回 false 时提前终止读取。
///
/// @param path         文件路径；当为 "-" 时读取 stdin
/// @param buffer_size  每次读取的最大字节数（建议 8 KiB - 64 KiB）
/// @param callback     块数据回调，接收当前块的数据视图；
///                     返回 true 继续读取，返回 false 提前终止
/// @return             成功返回 void，失败返回错误信息
///
/// 错误情况：
/// - 文件不存在或不可读：返回错误
/// - 读取过程中发生 IO 错误：返回错误
/// - callback 抛出异常：由调用方处理
auto read_chunks(const std::filesystem::path& path,
                 size_t buffer_size,
                 std::function<bool(std::span<const std::byte>)> callback
                 ) -> base::Result<void>;

} // namespace cfbox::io
```

#### 行读取接口

逐行读取是许多文本处理命令的核心需求。该接口处理行结束符（`\n`）的分割，并对超长行提供保护机制。

```cpp
namespace cfbox::io {

/// 逐行读取文件内容。
///
/// 从 path 指定的文件中逐行读取文本，每读到一行（以 '\n' 分隔）调用 callback。
/// callback 返回 false 时提前终止读取。
///
/// @param path         文件路径；当为 "-" 时读取 stdin
/// @param callback     行数据回调，接收当前行的 string_view（不含 '\n'）；
///                     返回 true 继续读取，返回 false 提前终止
/// @return             成功返回 void，失败返回错误信息
///
/// 长行策略：
/// - 单行超过 max_line_length（默认 10 MiB）时，输出警告到 stderr 并截断该行。
/// - max_line_length 可通过参数调整，设为 0 表示无限制（不推荐）。
///
/// 错误情况：
/// - 文件不存在或不可读：返回错误
/// - 读取过程中发生 IO 错误：返回错误
/// - 行长度超过限制：输出警告但继续处理
auto for_each_line(const std::filesystem::path& path,
                   std::function<bool(std::string_view)> callback,
                   size_t max_line_length = 10 * 1024 * 1024  // 10 MiB
                   ) -> base::Result<void>;

} // namespace cfbox::io
```

#### 安全写入接口

安全写入确保所有数据都被写出，处理短写、信号中断（EINTR）和管道破裂（EPIPE）等边缘情况。

```cpp
namespace cfbox::io {

/// 安全写入全部数据到文件描述符。
///
/// 循环调用 write() 直到所有数据写完，处理以下边缘情况：
/// - 短写（partial write）：继续写入剩余数据
/// - EINTR：被信号中断后重试写入
/// - EPIPE：管道破裂时静默返回错误（不触发 SIGPIPE）
///
/// @param fd   目标文件描述符
/// @param data 待写入的数据
/// @return     成功返回 void，失败返回错误信息
///
/// 错误情况：
/// - fd 无效：返回错误
/// - 磁盘空间不足（ENOSPC）：返回错误
/// - 管道破裂（EPIPE）：返回特定错误，调用方可选择静默处理
auto write_all(int fd,
               std::span<const std::byte> data
               ) -> base::Result<void>;

} // namespace cfbox::io
```

#### 统一 stdin/stdout 语义接口

支持 `-` 作为 stdin/stdout 的约定别名，使命令行接口与 GNU/coreutils 行为一致。

```cpp
namespace cfbox::io {

/// 解析文件路径，处理 "-" 语义。
///
/// 将 "-" 映射为 stdin（读模式）或 stdout（写模式），
/// 其他路径原样返回。与 GNU coreutils 行为一致。
///
/// @param path     输入路径字符串
/// @param for_read true 表示读模式（"-" 映射为 stdin），false 表示写模式
/// @return         解析后的文件描述符或路径
auto resolve_path(const std::string& path, bool for_read
                  ) -> base::Result<std::variant<int, std::filesystem::path>>;

} // namespace cfbox::io
```

#### 错误上下文接口

提供统一的错误上下文信息，便于调试和用户错误报告。

```cpp
namespace cfbox::io {

/// IO 操作错误上下文。
///
/// 包含足够的上下文信息来定位和报告 IO 错误：
/// - 操作类型（读、写、打开、关闭等）
/// - 涉及的文件路径
/// - 系统 errno 值和对应描述
struct IOErrorContext {
    std::string operation;                  // 如 "read"、"write"、"open"
    std::filesystem::path path;             // 涉及的文件路径
    int errno_value;                        // 系统 errno
    std::string errno_description;          // errno 的文本描述

    /// 格式化为用户友好的错误消息。
    /// 格式："<operation> <path>: <errno_description>"
    auto format() const -> std::string;
};

} // namespace cfbox::io
```

### 实现约束

不要为了"流式"引入复杂框架；优先使用简单、可审计的循环和固定缓冲。

具体原则：
- **固定缓冲区**：所有流式命令使用固定大小的栈分配或堆分配缓冲区（默认 8 KiB），不使用自适应增长策略。
- **回调模式**：优先使用回调函数模式（`read_chunks` + callback），避免协程或复杂迭代器。
- **零拷贝优先**：在不需要数据变换的场景（如 `cat`），尽量使用 `splice`/`sendfile` 等零拷贝系统调用。
- **SIGPIPE 处理**：进程启动时将 `SIGPIPE` 设为 `SIG_IGN`，通过 `EPIPE` 返回值检测管道破裂，避免异步信号中断。

---

## Part 4: 测试场景

最小测试集：

- 100 MB 文件的 `cat/wc/grep/head/tail/cmp/cksum/md5sum`。
- 无限或长时间 stdin：`yes | head`、`yes | tail -n 10` 的受控场景。
- broken pipe：`cfbox cat large | head -c 1` 不应异常输出噪声。
- 短读/短写模拟：通过小 buffer 或测试 helper。
- 权限错误：目录、不可读文件、不可写目标。
- 归档大文件 entry：tar/cpio 创建和提取不加载整个文件。

### 具体测试命令

#### 场景 1：100 MB 大文件 cat

验证流式命令对大文件的处理能力和 RSS 控制效果。

```bash
# 生成 100 MB 随机数据文件
dd if=/dev/urandom of=/tmp/test_100m.bin bs=1M count=100

# 测试 cat：输出大小应与输入一致
dd if=/dev/urandom bs=1M count=100 | cfbox cat | wc -c
# 预期：输出 104857600（100 * 1024 * 1024）

# 测试 wc：行数/字数统计
cfbox wc -l /tmp/test_100m.bin

# 测试 grep：搜索不存在的模式，应正常完成
cfbox grep "IMPOSSIBLE_PATTERN_12345" /tmp/test_100m.bin
# 预期：退出码 1，无输出

# 测试 md5sum：计算摘要，RSS 应恒定
cfbox md5sum /tmp/test_100m.bin

# 测试 cksum：校验和计算
cfbox cksum /tmp/test_100m.bin
```

#### 场景 2：无限 stdin 的受控处理

验证命令在无限输入场景下的可控性，确保不会耗尽内存。

```bash
# head 应在读取 5 行后停止，yes 应被 SIGPIPE 终止
yes | cfbox head -n 5
# 预期：输出 5 行 "y"

# tail 应维护固定缓冲区，RSS 不随输入增长
yes | cfbox tail -n 10
# 预期：输出 10 行 "y"

# wc 应在管道关闭后输出统计
yes | timeout 1 cfbox wc -l
# 预期：输出行数（因超时截断，行数应接近 timeout 期间 yes 的输出速率）

# grep 应在找到匹配后可选停止（如 -m 1 模式）
yes "hello world" | cfbox grep -m 1 "hello"
# 预期：输出 1 行 "hello world" 后停止
```

#### 场景 3：broken pipe 处理

验证命令在输出端关闭（broken pipe）时的行为——应静默退出，不输出错误噪声。

```bash
# cat 从无限源读取，管道在 1 字节后关闭
cfbox cat /dev/urandom | head -c 1
# 预期：输出 1 字节，cfbox cat 静默退出（退出码可非零但无 stderr 噪声）

# 大文件 cat 中途管道关闭
dd if=/dev/urandom of=/tmp/test_large.bin bs=1M count=200
cfbox cat /tmp/test_large.bin | head -c 1
# 预期：输出 1 字节，cfbox cat 静默退出

# grep 输出被管道关闭中断
cfbox grep -r "anything" /usr/share/dict/words | head -c 1
# 预期：输出 1 字节后干净退出

# tr 处理 broken pipe
echo "abcdef" | cfbox tr 'a-z' 'A-Z' | head -c 1
# 预期：输出 "A" 后干净退出
```

#### 场景 4：权限错误

验证命令在权限不足时的错误处理——应输出清晰的错误消息并返回非零退出码。

```bash
# 读取 root 所有的文件（以非 root 用户运行）
cfbox cat /root/.bashrc 2>&1
# 预期：stderr 输出 "Permission denied" 类信息，退出码非零

# 读取不存在的文件
cfbox cat /nonexistent_file_12345 2>&1
# 预期：stderr 输出 "No such file or directory"，退出码非零

# 写入只读目录
mkdir -p /tmp/test_readonly_dir && chmod 000 /tmp/test_readonly_dir
cfbox tee /tmp/test_readonly_dir/output.txt < /dev/null 2>&1
# 预期：stderr 输出权限错误
chmod 755 /tmp/test_readonly_dir && rm -rf /tmp/test_readonly_dir

# 读取目录而非文件
cfbox cat /tmp 2>&1
# 预期：stderr 输出 "Is a directory" 类信息
```

#### 场景 5：归档大文件 entry

验证归档命令在处理包含大文件的归档时的内存行为。

```bash
# 创建包含 1000 个小文件的目录
mkdir -p /tmp/test_tar_dir
for i in $(seq 1 1000); do
    dd if=/dev/urandom of="/tmp/test_tar_dir/file_${i}.bin" bs=1024 count=1 2>/dev/null
done

# 创建 tar 归档
cfbox tar -cf /tmp/test_archive.tar -C /tmp/test_tar_dir .

# 提取归档，监控 RSS
mkdir -p /tmp/test_tar_extract
# 在提取过程中通过 /proc/<pid>/status 监控 VmRSS
cfbox tar -xf /tmp/test_archive.tar -C /tmp/test_tar_extract
# 预期：RSS 不随文件数量线性增长，峰值 RSS 应远小于归档总大小

# 添加单个大文件到归档
dd if=/dev/urandom of=/tmp/test_tar_dir/large.bin bs=1M count=50 2>/dev/null
cfbox tar -cf /tmp/test_archive_large.tar -C /tmp/test_tar_dir .
cfbox tar -xf /tmp/test_archive_large.tar -C /tmp/test_tar_extract
# 预期：提取 large.bin 时 RSS 不接近 50 MiB

# 清理
rm -rf /tmp/test_tar_dir /tmp/test_tar_extract /tmp/test_archive.tar /tmp/test_archive_large.tar
```

#### 场景 6：短读/短写模拟

验证命令在 IO 系统返回不完整读写时的行为。

```bash
# 通过小缓冲区测试 helper 模拟短读
# （需要测试框架支持，以下为概念性测试）

# 方案 A：使用 LD_PRELOAD 拦截 read/write 系统调用，注入短读/短写行为
# 方案 B：在 read_chunks 内部使用 1 字节缓冲区进行测试

# 概念测试：确保 write_all 正确处理短写
# 即使每次只写 1 字节，write_all 也应确保全部数据写出
echo "test data for short write" | cfbox cat > /tmp/test_short_write.txt
# 预期：输出文件内容与输入完全一致

# 概念测试：确保 read_chunks 正确处理短读
# 从 pipe 读取时数据可能分多次到达
echo -n "partial" | (sleep 0.1; echo " data") | cfbox cat
# 预期：输出 "partial data"
```

---

## 验收指标

| 指标 | 要求 |
|------|------|
| streaming 命令 RSS | 对 100 MB 输入，RSS 不随输入线性增长 |
| bounded-buffer 命令 RSS | 与窗口大小成比例 |
| full-read 命令 | 有规模边界、RSS 基线、豁免记录 |
| 错误路径 | 读失败、写失败、EPIPE 有测试或说明 |

### 验收测试方法

#### RSS 测量方法

```bash
# 方法 1：通过 /proc 监控
# 启动 cfbox 命令后，从另一个终端执行：
while kill -0 $PID 2>/dev/null; do
    grep VmRSS /proc/$PID/status
    sleep 0.1
done

# 方法 2：使用 time 命令（如果支持 RSS 报告）
/usr/bin/time -v cfbox cat /tmp/test_100m.bin > /dev/null
# 关注 "Maximum resident set size" 行

# 方法 3：使用测试框架的 RSS 断言
# 在集成测试中通过 getrusage() 获取 ru_maxrss
```

#### RSS 验收阈值

| 命令类型 | 输入大小 | RSS 阈值（上限） |
|----------|----------|------------------|
| streaming（`cat`, `wc`, `grep`, `tr`） | 100 MB | < 10 MiB |
| streaming hash（`md5sum`, `cksum`） | 100 MB | < 5 MiB |
| bounded-buffer（`tail`） | 100 MB | < 5 MiB + N 行缓冲 |
| full-read（`sort`） | 100 MB | 记录基线，无硬阈值 |
| 归档（`tar`） | 50 MB 大文件 | < 20 MiB |

Phase 0D 完成后，Phase 1 新增 `dd`、SHA、base 编码、zcat 必须默认按流式接口实现。
