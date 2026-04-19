# 架构与设计

## 分发机制

CFBox 是单一可执行文件，通过两种方式调用子命令：

1. **符号链接检测：** [main.cpp](../src/main.cpp) 读取 `argv[0]`，提取文件名部分，在 `APPLET_REGISTRY` 中查找对应的入口函数。用户通过创建符号链接（如 `ln -s cfbox echo`）来分发命令。
2. **子命令语法：** 若 `argv[0]` 未匹配任何 applet，则将 `argv[1]` 作为子命令名查找（如 `cfbox echo ...`）。

```cpp
// include/cfbox/applets.hpp
constexpr auto APPLET_REGISTRY = std::to_array<cfbox::applet::AppEntry>({
    {"echo",   echo_main,   "display text"},
    {"cat",    cat_main,    "concatenate files"},
    // ... 共 17 个条目
});
```

## 核心基础设施

| 头文件 | 用途 |
|--------|------|
| [error.hpp](../include/cfbox/error.hpp) | `std::expected<T, Error>` 及 `CFBOX_TRY` 宏用于错误传播 |
| [applet.hpp](../include/cfbox/applet.hpp) | `AppEntry` 结构体与 `find_applet()` 模板查找 |
| [args.hpp](../include/cfbox/args.hpp) | 命令行参数解析器 — 短标志、带值标志、`--` 分隔符、位置参数 |
| [io.hpp](../include/cfbox/io.hpp) | 文件 I/O 工具 — `read_all`、`read_lines`、`read_all_stdin`、`write_all`、`split_lines` |
| [fs_util.hpp](../include/cfbox/fs_util.hpp) | 返回 `Result<T>` 的文件系统封装 — `exists`、`mkdir_recursive`、`copy_recursive`、`rename` 等 |
| [escape.hpp](../include/cfbox/escape.hpp) | `echo` / `printf` 的转义序列处理（`\n`、`\t`、`\0NNN` 等） |

## 错误处理

所有可能失败的操作返回 `cfbox::base::Result<T>`（即 `std::expected<T, Error>`），通过 `CFBOX_TRY` 宏传播错误：

```cpp
auto content = CFBOX_TRY(cfbox::io::read_all(path));
```

`CFBOX_TRY(var, expr)` 将 `expr` 的 `Result<T>` 解包到 `var`，失败时从当前函数返回错误。

## 参数解析

[args.hpp](../include/cfbox/args.hpp) 提供统一的参数解析器：

```cpp
auto parsed = cfbox::args::parse(argc, argv, {
    {'n', false},   // -n 无值标志
    {'e', false},   // -e 无值标志
    {'m', true},    // -m 需要值（如 -m 755 或 -m755）
});

if (parsed.has('n')) { /* ... */ }
auto mode = parsed.get('m');  // std::optional<std::string_view>
auto files = parsed.positional();  // const std::vector<std::string_view>&
```

支持的语法：短标志组合（`-ne`）、带值标志（`-n5` 或 `-n 5`）、`--` 分隔符。

## Applet 注册

每个 applet 遵循统一签名：

```cpp
auto echo_main(int argc, char* argv[]) -> int;
```

添加新 applet 只需：
1. 实现 `_main` 函数
2. 在 [applets.hpp](../include/cfbox/applets.hpp) 声明
3. 在 `APPLET_REGISTRY` 添加条目

详见 [CONTRIBUTING.md](../CONTRIBUTING.md)。

## 测试体系

### 单元测试（108 个用例）

基于 GoogleTest（通过 CPM 获取），位于 [tests/unit/](../tests/unit/)：

- [test_capture.hpp](../tests/unit/test_capture.hpp) — 测试工具：stdout 捕获、临时目录
- 各 applet 独立测试文件（`test_echo.cpp`、`test_grep.cpp` 等）

运行：`ctest --test-dir build --output-on-failure`

### 集成测试（16 个脚本）

Shell 脚本位于 [tests/integration/](../tests/integration/)，与 GNU coreutils 行为对比：

- [helpers.sh](../tests/integration/helpers.sh) — `assert_output()`、`assert_exit()` 等断言函数
- 各 applet 独立测试脚本

运行：`bash tests/integration/run_all.sh`
