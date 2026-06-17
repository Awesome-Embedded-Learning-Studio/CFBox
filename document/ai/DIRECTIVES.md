# CFBox DIRECTIVES — 架构铁律 / 约定 / 操作模型

> Tier 1（年级稳定，改动稀少）。事实来源：[document/architecture.md](../architecture.md)、[.clang-format](../../.clang-format)、真实头文件（已核对）。`[待补]`=需填实。改本文件前按操作模型 L4 查牵连。

## A. 架构不变量（跨切面，所有代码适用）

- **C++23**（`CMAKE_CXX_STANDARD 23 REQUIRED`，见 [cmake/compile/CompilerFlag.cmake](../../cmake/compile/CompilerFlag.cmake)）；可用 ranges/concepts/`std::expected` 等现代特性。
- **禁异常、禁 RTTI**：全局 `-fno-exceptions -fno-rtti`。错误一律经 **`cfbox::base::Result<T>` = `std::expected<T, Error>`**（[include/cfbox/error.hpp](../../include/cfbox/error.hpp)）；`Error = { int code; std::string msg; }`。
- **错误传播**：用 `CFBOX_TRY(var, expr)` 解包，失败 `return std::unexpected(...)`；**不抛异常、不返回裸 `-1`/errno 当成功**。错误统一经 `CFBOX_ERR(cmd, fmt, ...)` 报到 stderr（格式 `cfbox <cmd>: <msg>`）。
- **单二进制 applet 分发**：所有命令编译进同一个 `cfbox` 可执行文件，经 `argv[0]` 符号链接检测或子命令分发（[src/main.cpp](../../src/main.cpp)）。每个 applet 由 `APPLET_REGISTRY` 注册（[include/cfbox/applets.hpp](../../include/cfbox/applets.hpp)），逐个受 `CFBOX_ENABLE_<APPLET>` 宏守卫（CMake 生成于 [include/cfbox/applet_config.hpp.in](../../include/cfbox/applet_config.hpp.in)）——**新增/裁剪 applet 必须同时改注册表 + CMake 开关**。
- **公共基础设施复用**：文件系统操作走 `cfbox::fs::*`（返回 `Result<T>`），I/O 走 `cfbox::io::*`（流式 `for_each_line()` 优先于 `read_all()`），参数解析走 `cfbox::args`，流处理走 `cfbox::stream`。**勿在各 applet 里重复造递归遍历/错误格式/POSIX 封装**（质量扫描 A 维度盯这些）。
- **RAII 资源管理**：`unique_file`（FILE*）、`scoped_regex`（regex_t）、`unique_pipe`（popen/pclose）—— ASan 验证零泄漏。**禁止裸 FILE*/regfree 漏 close 路径**。
- **零外部依赖**：手写轻量 deflate/inflate 替代 zlib（[deflate.hpp](../../include/cfbox/deflate.hpp)/[inflate.hpp](../../include/cfbox/inflate.hpp)）；regex 走 POSIX `regex_t` 而非 `std::regex`。新增可选依赖必须经构建开关隔离。
- **体积预算（头等约束）**：size-opt 构建（`-Os` + LTO + strip）目标 **≤ 550 KB**，当前 406 KB。新增代码须核查体积影响；优先流式处理、避免 iostream/`<filesystem>` 滥用、控制 inline 膨胀（质量扫描 B 维度）。

## B. 编码 / 注释约定

详见 [CODING-TASTE.md](CODING-TASTE.md)（单一权威：命名/注释/Result 惯用法/clang-format/测试）。要点：

- 命名：类型 `PascalCase`、函数/变量 `snake_case`、私有成员后缀 `_`、命名空间 `cfbox::*`（`base`/`fs`/`io`/`args`/`stream`/`applet`/`help`/`term`/`tui`/`proc`/`compress`/`checksum`/`awk`/`sh`/`init`/`utf`/`util`）、宏 `CFBOX_*`（`UPPER_SNAKE`）。
- 注释一律英文；机械风格以 [.clang-format](../../.clang-format) 为准，跑 clang-format 不手调。
- Result：`auto v = CFBOX_TRY(expr)`（宏展开判 `!v` 后 `return std::unexpected`）；成功 `return value;` / `Result<void>` 用 `return {};`；失败 `return std::unexpected(base::make_error(code, msg));`。

## C. 操作模型（长期，Claude 主力开发）

- **L1 一批一commit一验证**：`cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`（379 GTest，93 文件）**且** `bash tests/integration/run_all.sh`（54 脚本）全绿才提交；红则不提交、不更新 PLAN。
- **L2 提交信息** `<type>(<scope>): <简述>`——**英文**简述变更（对齐既有 git 历史），**不带 Co-Authored-By 或任何 AI 署名 trailer**。里程碑/批归属由 [PLAN.md](PLAN.md) 的 commit 列跟踪，不入 commit msg。
- **L3 propose-then-execute**：新 Phase/跨子系统大改，先出草案等确认；已确认的批内可自主推进。
- **L4 改前查牵连**：改任何 applet/头/CMake 前先 grep 引用方（`APPLET_REGISTRY`、`cfbox::fs::`、`CFBOX_ENABLE_*`、被改头文件的 include 方）；ROADMAP/PLAN/`document/todo`/git 状态变更需同步。
- **L5 验证**：本地用 `build/` 目录（`cmake -B build && cmake --build build -j$(nproc)`，native Debug + ASan/UBSan）；**CI 对等盲区**——本地 x86-64 抓不到 aarch64/armhf 交叉编译与 QEMU 用户/系统态，改公共头/分发逻辑/ABI 后 push 前留意 CI 的 cross-compile/qemu 阶段。体积相关改动跑 size-opt：`cmake -B build-size -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON && cmake --build build-size -j$(nproc)` 后 strip 核查 ≤ 550 KB。
- **L6 省 token**：命令与文档保持紧凑，不堆仪式；`CLAUDE.md`/`AGENTS.md` 常驻须薄，重内容按需读。
- **L7 编译并行**：所有 `cmake --build` 带 `-j$(nproc)`。
- **L8 工作记录**：每批 / 每个有意义的迭代，`/done` 写 [document/notes/](../notes/)`<date>-<topic>.md`（背景/目标/设计/决策/陷阱/验证）。**发版级**总结才进 [changelogs/](../../changelogs/)，二者分工见 [document/notes/README.md](../notes/README.md)。
