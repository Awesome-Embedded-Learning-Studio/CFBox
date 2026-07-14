# Phase 4 起步 — POSIX 符合度覆盖率标尺

> 2026-07-06。Phase 4（生产质量门禁）第一块基建：`tests/posix/coverage.sh` ——
> 客观度量 cfbox 对 POSIX.1-2017 XCU 的符合度，CI gate「只升不降」。Phase 3 网络
> 闭环合并后立即开建，作为 Phase 4「POSIX 子集验收」的第一步。

## 触及文件

- [tests/posix/coverage.sh](../../tests/posix/coverage.sh) — 双维度度量脚本
- [tests/posix/baseline](../../tests/posix/baseline) — 当前覆盖率下限（utility 61 / builtin 10）
- [.github/workflows/ci.yml](../../.github/workflows/ci.yml) — native 阶段加 POSIX coverage gate（structure gates 后）

## 双维度方法论

1. **utility 维度（静态）**：POSIX.1-2017 XCU mandatory utility list（剔除 shell builtin 后 77 个）
   vs `cfbox --list`。独立 applet 实现的覆盖。
2. **builtin 维度（动态）**：POSIX mandatory shell builtin（20 个）每个跑最小行为探针
   （`cfbox sh -c '<probe>'`），判输出是否含 `"<name>: command not found"`。
   **不靠声明，靠真实行为** —— 避免「applet list 有但行为缺」的假绿。

## 当前覆盖率（baseline）

| 维度 | 覆盖 | % |
|------|------|---|
| utility (applet) | 61/77 | 79% |
| builtin (sh) | 10/20 | 50% |
| **total** | **71/97** | **73%** |

## 高价值发现（gap，单独排批）

**utility 缺（16）**：
- **`dd`** —— P0 高频 POSIX mandatory，coreutils 必备。**真硬伤**，优先补。
- `stty file logger csplit getconf pr pathchk strings strip tput unexpand uudecode uuencode locale what`
  —— 长尾，按场景驱动排期。

**builtin 缺（10）**：`command type exec getopts hash umask ulimit wait times unalias`
- `type` / `command` —— POSIX sh 高频 builtin，脚本里 `type ls` / `command -v` 用得多
- `exec` / `wait` —— shell 核心 builtin，影响脚本语义
- `umask` / `ulimit` —— 嵌入式/资源控制常用
- `getopts` —— 脚本选项解析标配
- `hash` / `times` / `unalias` —— 较低频

注：探针确认了 `read`（管道）、`pwd`、`cd`、`export`、`unset`、`shift`、`eval`、`trap`、
`return`、`exit`、`set` 工作（10 个支持）。

## CI gate 机制

`tests/posix/coverage.sh` 退出码：
- 当前覆盖 ≥ baseline → exit 0（PASS）
- 任一维度 < baseline → exit 1（**退化报警，CI 红**）
- 高于 baseline → exit 0 + 提示「run with --update-baseline to lock in」

补 applet/builtin 后跑 `tests/posix/coverage.sh --update-baseline` 锁新下限。

## 后续排批建议（Phase 4 增量）

1. **dd**（P0，独立批次，~1 commit）—— bs/conv/iflag 非平凡，但高频值得做
2. **sh builtin 扩展**（command/type/exec/getopts/hash/umask/ulimit/wait/times/unalias）
   —— cfbox sh 一批补齐 10 个 builtin，builtin 覆盖率 50%→100%
3. **行为符合度 spot check**（Phase 4 Part 1 差异测试）—— 对 P0 utility 跑 BusyBox
   testsuite 的 `.tests` 文件（`competition/busybox/testsuite/`，206 个），对照输出
4. **XSI/optional utility** 扩展（awk/bc/vi/yacc 等，扩大 list）

baseline 先锁 73%，每补一项只升不降。
