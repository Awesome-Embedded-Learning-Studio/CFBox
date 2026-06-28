# 2026-06-28 — sh trap（Phase 2 批5c 第 8 子项，sh 全收收尾）

承接 [sh 中频](2026-06-28-sh-deepen-mid.md)。本项收 trap（信号/EXIT），**sh 全收 8/8 完成，Phase 2 全部完成**。

## 设计决策
- **ShellState `traps_` 表**（int sig → cmd）+ `trap_pending_signal` 全局（`inline volatile std::sig_atomic_t`，sh.hpp）。
- **builtin_trap**：解析信号名（EXIT/INT/TERM/HUP/QUIT/数字），存 `trap[sig]=cmd`；非 EXIT 信号注册 `trap_handler`（只设 flag，async-signal-safe）；`trap - SIG` 清除 + `SIG_DFL`；空 cmd 重置默认。
- **executor 消费**：execute(AndOr) 每 entry 后检查 `trap_pending_signal`，查 `state.get_trap(sig)`，非空则 Lexer/Parser/execute 跑 trap cmd。
- **EXIT trap**：`run_string` / `run_interactive` 退出前 `run_exit_trap`（`get_trap(0)`）。
- **waitpid EINTR 重试**（execute_simple/pipeline/subshell 3 处）：signal 中断 waitpid 返回 EINTR，重试避免 status 未设 UB + 僵尸子进程。这是 trap 信号处理的必要 robustness。

## 验证
- 集成 test_sh.sh：55 → 57（+2 trap EXIT + 清除），全绿。
- ctest 436/436。
- size-opt **438 KB**（持平）。

## 陷阱
- trap handler 只设 flag（async-signal-safe），实际 trap cmd 在 executor 安全校验点跑（非信号上下文）。
- SIGINT/SIGTERM 集成测试需 kill + timing，本批测 EXIT（可靠）；SIGINT 注册了 handler 但集成未覆盖（执行路径同 EXIT）。
- `trap -l`（列信号名）no-op；`trap`（无参）列已设 trap。
- fork child execvp 自动重置 handler 到 SIG_DFL，child 不继承 sh trap handler。
- commit: `46c3657`

## Phase 2 全部完成 ✅
sh 全收 8/8：**算术 / case / 函数+return+local / here-doc / 高级${} / break N+continue / read 增强 / trap**。Phase 2（批2-5c）全部 ✅。基线 436 GTest / 438 KB size-opt（v0.3.0 基线 399/418）。
