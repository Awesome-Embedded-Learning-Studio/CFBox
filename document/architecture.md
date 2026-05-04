# 架构与设计

## 分发机制

CFBox 是单一可执行文件，通过两种方式调用子命令：

1. **符号链接检测：** [main.cpp](../src/main.cpp) 读取 `argv[0]`，提取文件名部分，在 `APPLET_REGISTRY` 中查找对应的入口函数。用户通过创建符号链接（如 `ln -s cfbox echo`）来分发命令。
2. **子命令语法：** 若 `argv[0]` 未匹配任何 applet，则将 `argv[1]` 作为子命令名查找（如 `cfbox echo ...`）。

```cpp
// include/cfbox/applets.hpp — 条件编译的注册表
constexpr auto APPLET_REGISTRY = std::to_array<cfbox::applet::AppEntry>({
#if CFBOX_ENABLE_ECHO
    {"echo",   echo_main,   "display text"},
#endif
#if CFBOX_ENABLE_CAT
    {"cat",    cat_main,    "concatenate files"},
#endif
    // ... 共 109 个条目，每个由 #if 守卫
});
```

## 核心基础设施

| 头文件 | 用途 |
|--------|------|
| [error.hpp](../include/cfbox/error.hpp) | `std::expected<T, Error>` 及 `CFBOX_TRY` 宏用于错误传播 |
| [applet.hpp](../include/cfbox/applet.hpp) | `AppEntry` 结构体与 `find_applet()` 模板查找 |
| [applets.hpp](../include/cfbox/applets.hpp) | `APPLET_REGISTRY` 注册表，每个条目由 `#if CFBOX_ENABLE_xxx` 守卫 |
| [applet_config.hpp.in](../include/cfbox/applet_config.hpp.in) | CMake 生成的配置头文件：`CFBOX_ENABLE_<APPLET>` 宏和 `CFBOX_VERSION_STRING` |
| [args.hpp](../include/cfbox/args.hpp) | 命令行参数解析器 — 短标志、长选项（`--recursive`）、带值标志、`--` 分隔符、位置参数 |
| [io.hpp](../include/cfbox/io.hpp) | 文件 I/O 工具 — 流式 `for_each_line()`、`read_all`、`write_all`、`open_file` RAII、`split_lines` |
| [stream.hpp](../include/cfbox/stream.hpp) | 流处理管线 — `for_each_line()`、`split_fields()`、`split_whitespace()`、`LineProcessor` |
| [deflate.hpp](../include/cfbox/deflate.hpp) | 手写 DEFLATE 压缩（固定 Huffman + LZ77 hash chain，零外部依赖） |
| [inflate.hpp](../include/cfbox/inflate.hpp) | 手写 inflate 解压（fixed/dynamic/stored block） |
| [compress.hpp](../include/cfbox/compress.hpp) | gzip 封装，使用自实现 deflate/inflate |
| [regex.hpp](../include/cfbox/regex.hpp) | POSIX regex_t RAII 包装器 `scoped_regex` |
| [fs_util.hpp](../include/cfbox/fs_util.hpp) | 返回 `Result<T>` 的文件系统封装 — `exists`、`mkdir_recursive`、`copy_recursive`、`rename` 等 |
| [escape.hpp](../include/cfbox/escape.hpp) | `echo` / `printf` 的转义序列处理（`\n`、`\t`、`\0NNN` 等） |
| [help.hpp](../include/cfbox/help.hpp) | 帮助系统 — `HelpEntry` 结构体、`print_help()`、`print_version()`，支持彩色输出 |
| [term.hpp](../include/cfbox/term.hpp) | 终端颜色输出 — ANSI SGR 辅助函数（`[[nodiscard]] noexcept`），尊重 `NO_COLOR` |
| [utf8.hpp](../include/cfbox/utf8.hpp) | UTF-8 工具 — Unicode 感知的代码点计数、终端显示宽度（全 `constexpr` + `static_assert`） |
| [terminal.hpp](../include/cfbox/terminal.hpp) | 终端控制 — RawMode RAII、终端大小检测、光标控制、备用屏幕、视频属性 |
| [tui.hpp](../include/cfbox/tui.hpp) | TUI 框架 — ScreenBuffer 双缓冲增量渲染、Key 解析、TuiApp 事件循环 |
| [proc.hpp](../include/cfbox/proc.hpp) | /proc 解析器 — 进程信息、内存、CPU 统计、磁盘、挂载点 |
| [checksum.hpp](../include/cfbox/checksum.hpp) | 校验和 — CRC-32、MD5、BSD/SysV sum |

## 错误处理

所有可能失败的操作返回 `cfbox::base::Result<T>`（即 `std::expected<T, Error>`），通过 `CFBOX_TRY` 宏传播错误：

```cpp
auto content = CFBOX_TRY(cfbox::io::read_all(path));
```

`CFBOX_TRY(var, expr)` 将 `expr` 的 `Result<T>` 解包到 `var`，失败时从当前函数返回错误。

## RAII 安全

所有资源管理均使用 RAII 包装器，ASan 验证零泄漏：

- **`unique_file`**（io.hpp）：`unique_ptr<FILE, FileCloser>`，自动 `fclose`
- **`scoped_regex`**（regex.hpp）：析构自动 `regfree`，grep/sed/awk 使用
- **`unique_pipe`**（sh_expand.cpp）：popen/pclose RAII

## 流式 I/O

`io.hpp` 提供两种 I/O 模式：

1. **全量读取**：`read_all()` / `read_lines()` — 适用于需要随机访问的场景
2. **流式处理**：`for_each_line(FILE*, callback)` — 逐行读取，不加载整个文件到内存

grep、cat、wc 等工具使用流式处理，可处理无限输入（如 `/dev/urandom`）而不耗尽内存。

## 参数解析

[args.hpp](../include/cfbox/args.hpp) 提供统一的参数解析器，支持短选项和 GNU 风格长选项：

```cpp
auto parsed = cfbox::args::parse(argc, argv, {
    {'r', false, "recursive"},  // -r 或 --recursive，无值标志
    {'n', false, ""},           // -n 仅短选项
    {'m', true,  "mode"},       // -m 755 或 --mode=755 或 --mode 755
});

if (parsed.has('r')) { /* ... */ }                 // 短选项查询
if (parsed.has_long("recursive")) { /* ... */ }    // 长选项查询
if (parsed.has_any('r', "recursive")) { /* ... }   // 两者任一
auto mode = parsed.get_any('m', "mode");           // 从短或长形式获取值
auto files = parsed.positional();
```

支持的语法：短标志组合（`-ne`）、带值标志（`-n5` 或 `-n 5`）、长选项（`--recursive`、`--mode=755`）、`--` 分隔符。未注册的长选项（如 `--help`、`--version`）仍存储在结果中供 applet 检查。

## 帮助系统

每个 applet 定义一个 `HelpEntry` 常量：

```cpp
static constexpr cfbox::help::HelpEntry HELP = {
    .name    = "echo",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display a line of text",
    .usage   = "echo [OPTIONS] [STRING]...",
    .options = "  -n     do not output trailing newline\n"
               "  -e     enable backslash escape interpretation",
    .extra   = "",
};
```

在 `parse()` 之后检查 `--help` / `--version`：

```cpp
if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }
```

`print_help()` 输出格式化的帮助文本（带彩色标题和使用说明），自动追加 `--help` / `--version` 选项。当 `NO_COLOR` 环境变量设置时自动禁用颜色。

## CMake 配置系统

[cmake/Config.cmake](../cmake/Config.cmake) 提供 per-applet 编译选项：

```bash
# 禁用单个 applet
cmake -DCFBOX_ENABLE_GREP=OFF ..

# 使用预设 profile
cmake -DCFBOX_PROFILE=minimal ..   # 仅核心文件操作 applet
cmake -DCFBOX_PROFILE=embedded ..  # 除文本处理外全部启用
cmake -DCFBOX_PROFILE=desktop ..   # 全部启用

# 体积优化构建
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON
```

配置通过 `configure_file()` 生成 `include/cfbox/applet_config.hpp`，包含 `CFBOX_ENABLE_<APPLET>` 宏（0 或 1）和 `CFBOX_VERSION_STRING`。

## Applet 注册

每个 applet 遵循统一签名：

```cpp
auto echo_main(int argc, char* argv[]) -> int;
```

添加新 applet 需要：
1. 实现 `_main` 函数
2. 在 [applets.hpp](../include/cfbox/applets.hpp) 用 `#if CFBOX_ENABLE_xxx` 守卫声明
3. 在 `APPLET_REGISTRY` 用 `#if` 守卫添加条目
4. 在 `cmake/Config.cmake` 的 `CFBOX_APPLETS` 列表中添加名字
5. 定义 `HelpEntry` 常量并处理 `--help`/`--version`

详见 [CONTRIBUTING.md](../CONTRIBUTING.md)。

## 测试体系

### 单元测试（331 个用例）

基于 GoogleTest（通过 CPM 获取），位于 [tests/unit/](../tests/unit/)：

- [test_capture.hpp](../tests/unit/test_capture.hpp) — 测试工具：stdout 捕获、临时目录
- 各 applet 独立测试文件（`test_echo.cpp`、`test_grep.cpp` 等）
- 基础设施测试：`test_args.cpp`、`test_help.cpp`、`test_term.cpp`、`test_utf8.cpp`、`test_compress.cpp` 等
- Applet 测试文件由 `#if CFBOX_ENABLE_xxx` 守卫，禁用 applet 时自动跳过

运行：`ctest --test-dir build --output-on-failure`

### 集成测试（54 个脚本）

Shell 脚本位于 [tests/integration/](../tests/integration/)，与 GNU coreutils 行为对比：

- [helpers.sh](../tests/integration/helpers.sh) — `assert_output()`、`assert_exit()` 等断言函数
- 各 applet 独立测试脚本
- [test_help.sh](../tests/integration/test_help.sh) — 验证所有 applet 的 `--help` 和 `--version`

运行：`bash tests/integration/run_all.sh`

### ASan 验证

Debug 构建启用 AddressSanitizer，验证所有 applet 零内存泄漏：

```bash
cmake -B build-dbg -DCMAKE_BUILD_TYPE=Debug && cmake --build build-dbg
ctest --test-dir build-dbg --output-on-failure
```
