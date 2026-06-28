# 2026-06-28 — sh 深化中频（here-doc/高级${}/break N/read）（Phase 2 批5c 第 4-7 子项）

承接 [sh 深化 P1](2026-06-28-sh-deepen-p1.md)。本批收 here-doc + 高级参数展开 + break N + read 增强。

## 子项与设计决策

### 4. here-doc `<<` / `<<-`（e07de18）
- lexer 读 `<<DELIM`/`<<-DELIM` 时**一次性收集 body**（直到 DELIM 行，`<<-` 剥前导 tab），body 随 DLess/DLessDash token 携带。
- 约束：`<<DELIM` 在命令行末（常见用法）。多 here-doc 同行 / `<<DELIM arg` 后置不支持（罕见）。
- parser → Redir::HereDoc；executor 写 body 到 mkstemp 临时文件 + dup2 重定向 stdin。
- unquoted here-doc body 经 `expand_noglob` 展开 param/arith/command。

### 5. 高级 `${}` 参数展开（f23c2f1）
- `${#VAR}` 长度；`${VAR#pat}`/`${VAR##pat}` 去头（最短/最长前缀 glob）；`${VAR%pat}`/`${VAR%%pat}` 去尾；`${VAR:-def}` 默认；`${VAR:+alt}` 替代。
- glob 前缀/后缀匹配用 fnmatch 递增长度扫描（`glob_prefix_len`/`glob_suffix_len`）。
- `${P##*/}` 经典 basename 技巧可用。
- 未做 `${VAR:=def}`（需修改 const state）与 `${VAR:?err}`（需错误传播），后置。

### 6. break N / continue（4e5814f）
- ShellState `break_loop` bool → `break_depth` int（跨 N 层）。
- execute(AndOr) 检测 break_depth/continue_loop 中断 body（否则 `break` 后 body 剩余命令仍执行）。
- **修两个既存 bug**：① break/continue **完全没有 builtin**（循环标志从无人设，break/continue 静默无效）；② `parse_compound_list` merge 时把 result 最后 entry 的 op 强改 Semi，破坏 `cmd1 && cmd2; cmd3` 的 `&&`（`false && echo M; echo X` 错跑 echo M）。改 merge 为直接拼接。

### 7. read 增强（33bc74f）
- `read -p PROMPT`（stderr 提示）、`read -r`（raw，保留 `\`；当前本就保留，-r 为 no-op 标志）。
- **修既存 bug**：最后变量收前导 IFS（`echo a b c | read x y z` 的 z 是 `" c"`，现 `"c"`）。
- 未做 `-s`/`-n N`/`-t`（termios 静默/字符数/超时，后置）。

## 验证
- 集成 test_sh.sh：41 → 55（+3 here-doc +5 高级${} +3 break +3 read），全绿。
- ctest 436/436（sh 仍零 GTest）。
- size-opt **438 KB**（基线 418，+20 KB 累计 sh：fnmatch + mkstemp + AST 扩展）。

## 陷阱 / 留给后续
- **trap** 唯一剩余项：信号处理（SIGINT/SIGTERM/EXIT）。需 signal handler + 全局 trap 表 + executor 检查执行，与 fork/exec + multi-call binary 交互复杂，留下批 fresh context。
- **既存 keyword 误判**（调试 break 时发现）：`echo done` 的 `done` 被 lexer 标 keyword、parser 当 terminator，导致 `echo done` 输出空。POSIX keyword 应只在命令起始位置识别。待修（影响 `echo fi/done/then` 等参数）。
- sh 仍零 GTest：本批靠集成验证（55 用例）；test_sh.cpp 待建。
- commits: `e07de18`/`f23c2f1`/`4e5814f`/`33bc74f`
