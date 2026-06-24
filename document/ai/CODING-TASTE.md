# CFBox Coding Taste

> 代码风格的**单一权威**。[DIRECTIVES.md](DIRECTIVES.md) B 与 `CLAUDE.md`/`AGENTS.md` 指向本文。
> **铁律**：机械风格（缩进/换行/对齐/指针位置/include 排序）以 [.clang-format](../../.clang-format) 为准——**用 clang-format 格式化，不手调**；本文只管 clang-format 管不到的：命名、惯用法、注释、结构。

## 0. 权威层级
1. [.clang-format](../../.clang-format) — 机械风格，跑工具。
2. 本文档 — 命名/注释/惯用法/结构。
3. [DIRECTIVES.md](DIRECTIVES.md) A — 语言级铁律（C++23、无异常、无 RTTI、`Result<T>`、单二进制分发、体积预算）。

## 1. 语言约束（见 DIRECTIVES A）
C++23（ranges/concepts/`std::expected` 可用）。**禁异常**（`-fno-exceptions`，错误走 `Result<T>`）、**禁 RTTI**（`-fno-rtti`）。错误一律 `cfbox::base::Result<T>`（`std::expected<T, Error>`）。

## 2. 命名

**文件**：`snake_case.hpp` / `snake_case.cpp`；applet 实现 `src/applets/<name>.cpp`，入口 `auto <name>_main(int argc, char* argv[]) -> int`；测试 `tests/unit/test_<module>.cpp`。

| 元素 | 规则 | 示例 |
|------|------|------|
| 类/结构/联合 | PascalCase | `RawMode`、`ScreenBuffer`、`TuiApp`、`AppEntry`、`BitReader` |
| 函数/变量/参数 | snake_case，优先 trailing return | `auto exists(std::string_view path) -> bool` |
| 私有成员 | snake_case + 尾随 `_`（**必须**），类内初始化 | `int fd_;`、`bool valid_ = false;`、`int rows_ = 0;` |
| 公开结构体字段 | snake_case，无后缀 | `AppEntry{name, fn, desc}` |
| 命名空间 | `cfbox::<sub>` | `base`/`fs`/`io`/`args`/`stream`/`applet`/`help`/`term`/`terminal`/`tui`/`proc`/`compress`/`deflate`/`inflate`/`checksum`/`awk`/`sh`/`init`/`utf`/`util` |
| 宏 | `CFBOX_UPPER_SNAKE` | `CFBOX_ERR`、`CFBOX_TRY`、`CFBOX_ENABLE_CAT` |

> 注：cfbox 公共工具多为**头文件内 inline 自由函数**（见 [fs_util.hpp](../../include/cfbox/fs_util.hpp)、[io.hpp](../../include/cfbox/io.hpp)），非成员函数。新代码沿用此风格；有非平凡不变量/私有态才用 `class`。

## 3. 注释
- 注释一律**英文**；解释**为什么**而非是什么。cfbox 现有注释密度偏低，新增公共 API 宜补一句职责说明。
- **TODO/FIXME**：带实现思路，不只是 `// TODO`。
- 机械格式（include 分块、指针左贴、100 列、K&R）交给 clang-format，不手调。

## 4. 格式化（clang-format 权威）
跑 `clang-format -i` 即可。要点：4 空格、K&R、`ColumnLimit: 100`、指针左贴（`char* p`）、namespace 内容不缩进、include 分块重排。**提交前跑 clang-format**；CI 也跑。

## 5. Result 惯用法（取自真实 [error.hpp](../../include/cfbox/error.hpp)）
```cpp
// error.hpp 核心定义
namespace cfbox::base {
struct Error { int code; std::string msg; };
template <typename T> using Result = std::expected<T, Error>;
}

// CFBOX_TRY(var, expr) 宏展开：auto var = (expr); if (!var) return std::unexpected(std::move(var).error());
auto content = CFBOX_TRY(content, cfbox::io::read_all(path));  // 失败自动 return unexpected

// 失败构造
return std::unexpected(base::make_error(ec, "msg"));
// 成功
return value;        // Result<T>
return {};           // Result<void>

// 错误报告（到 stderr，格式统一 "cfbox <cmd>: <msg>"）
CFBOX_ERR("rm", "cannot remove '%s': %s", path, reason);
```
- applet 入口 `<name>_main` 返回 **`int`**（退出码），内部失败路径用 `Result` 传递后翻译成退出码；**不要**在 applet 内抛异常。
- 勿把裸 `-1`/errno 当成功语义；POSIX 调用经 `cfbox::fs::*` 封装成 `Result`（质量扫描 A 维度）。

## 6. 类 / 结构布局
- `struct` = 聚合/POD（公开字段，如 `AppEntry`、`Key`、`Error`）；`class` = 有封装（私有状态 + 方法，如 `RawMode`、`TuiApp`）。
- 成员优先**类内初始化**（`bool valid_ = false;`）。
- `[[nodiscard]]`/`noexcept`/`constexpr` 按既有头文件惯例标注（见 [utf8.hpp](../../include/cfbox/utf8.hpp) 全 `constexpr` + `static_assert`）。

## 7. 头文件
- `#pragma once`（cfbox 自有头一律用，见 [error.hpp](../../include/cfbox/error.hpp)）。
- 公共工具放 `include/cfbox/`，实现可内联或对应 `src/`；applet 实现放 `src/applets/`。

## 8. 测试
- 单元测试：[tests/unit/test_*.cpp](../../tests/unit/)，GTest，共 98 文件 / 399 用例；改公共头/分发逻辑后跑 `ctest --test-dir build --output-on-failure`。
- 集成测试：[tests/integration/test_*.sh](../../tests/integration/)，54 脚本；跑 `bash tests/integration/run_all.sh`。
- 新增 applet 必须带对应 `test_<name>.cpp`（质量扫描 F 维度盯缺测）。

## 9. 提交前 checklist
- [ ] 跑过 clang-format；命名合规（PascalCase/snake_case/`_`/`CFBOX_*`）
- [ ] 无异常/RTTI；错误走 `Result` + `CFBOX_TRY`；错误报告用 `CFBOX_ERR`
- [ ] POSIX 调用经 `cfbox::fs::*`；无各 applet 重复造轮子（遍历/错误格式）
- [ ] 新 applet 同步改 `APPLET_REGISTRY` + CMake `CFBOX_ENABLE_*` 开关
- [ ] `cmake --build build -j$(nproc)` 零 warning；`ctest` + `run_all.sh` 全绿
- [ ] 体积相关改动跑 size-opt 核查 ≤ 550 KB
- [ ] commit message 纯描述、英文、**无 Co-Authored-By / AI 署名**
