# 2026-06-28 — test POSIX 三态 + 递归下降（Phase 2 批3）

## 背景
test 退出码偏离 POSIX：`to_int` 用 strtol 不校验，`abc -eq abc` 误判 0（应 2）；裸 `-z` 无操作数当单参非空→true（应 2）；文件操作符仅 8 个；解析靠"线性扫第一个 -o/-a 切分"，对复合表达式脆弱。

## 设计决策
- **递归下降解析器**（`Evaluator` struct + token 游标 `p`）：`parse_or → parse_and`（隐式 AND：相邻 primary 自动 AND）`→ parse_not → parse_primary`。三态 int 返回（0 true / 1 false / 2 error），OR/AND/NOT 各自合并规则。
- **整数严格校验**：弃 strtol，改 `std::from_chars` + 全串消费检查。`abc`/`5x`/空 → nullopt → exit 2。
- **arity/语法错误统一 exit 2**：裸一元操作符无操作数、未知 `-X`(alpha) 操作符、未配对 `(`/多余 `)`、表达式尾部残留 token。
- **操作符扩充**：文件 `-h`(=-L 别名)/`-b`/`-c`/`-p`/`-S`；文件比较 `-nt`/`-ot`(mtime)/`-ef`(同 dev+inode)；字符串 `<`/`>`(字节序)。
- **`!` 的双语义**：有后继时是否定；无后继（`test !`）是非空字符串操作数→true（POSIX）。
- **顶层空表达式**（`[ ]`/裸 test）→ false(1)，非 error；子表达式空（`()`）→ error(2)。

## 验证
- GTest +11，全量 **417/0**。
- 集成 test_test.sh 改造：引入 `assert_exit` 精确断 0/1/2（原 `&&/||` 会吞 exit 2），+14 case，31 passed。
- size-opt **422 KB**（持平——递归下降替代线性切分抵消新操作符体积）。

## 陷阱
- `test -5`（负数单参）→ true(0)：第二字符非 alpha 不触发"未知操作符"，当非空字符串（符合 coreutils）。负数作整数操作数 `-5 -eq 5` 正常（二元检测在先）。
- `<`/`>` 在 shell 需转义/引号，是 shell 词法非 test 职责；操作数恰好是 `-a`/`-o`/`-e` 等需引号（POSIX test 已知限制）。
- 未补 `-g/-u/-k/-O/-G/-t`（setuid 位/tty，低频），按 POSIX 子集后置。
- commit: `0f9b3cb`
