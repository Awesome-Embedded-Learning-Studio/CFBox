# 2026-06-28 — find 布尔表达式（Phase 2 批5b）

## 背景
find 是扁平 AND 谓词链（`Predicate` enum + `matches_predicates` 全真折叠），无 -o/!/()/-not，未知谓词静默跳过。改为表达式树 + 递归下降。

## 设计决策
- **AST**（`Node{And,Or,Not,Name,Type,Exec,True}`）：And/Or 持 N 子；Not 持 1 子；Name/Type/Exec/True 叶子。`Node(Kind)` 构造函数（非聚合）规避 designated-init 在 -Werror 下的诊断差异。
- **递归下降**（`Parser` + token 游标，仿 expr.cpp）：`parse_or → parse_and`（隐式 AND：相邻 primary 自动 AND）`→ parse_not → parse_primary`。优先级 AND 高于 OR（`-name a -o -name b -type f` = `a OR (b AND f)`）。
- **-maxdepth 提为 global option**：parse 遇 `-maxdepth N` 设 `parser.maxdepth`，返回 `True` 占位（不进求值树，作用于遍历 `disable_recursion_pending`）。
- **-exec 作 action 叶子**：eval 时执行 + 返回 true；短路求值下 `-name x -o -exec ...` 的 exec 在 OR 左真时不执行（符合 GNU）。
- **PATH vs 表达式消歧**：argv[1] 若以 `-`/`(`/`)` 开头或为 `!`，则是表达式（PATH 默认 `.`），否则是 PATH。
- **错误处理**：未知谓词/缺操作数/括号不匹配 → `failed=true` → exit 1（GNU find 语义）。

## 验证
- GTest +5（FindByOr/FindByNotType/ExplicitAnd/ParensGrouping/UnknownPredicateExits1），全量 **436/0**。
- 集成 test_find.sh +5（-o、! -type f、\( \)、显式 -a、未知谓词 exit 1），16 passed。
- size-opt **422 KB**（持平——AST 用现有 vector，无新模板膨胀）。

## 陷阱
- 隐式 AND：`-name x -type f` 无 `-a` 也按 AND（GNU 行为）；`parse_and` 循环 `peek != "-o" && peek != ")"` 触发。
- `-exec` 与默认 `-print`：表达式含 Exec 节点时抑制默认打印（GNU）；混合 `-exec ... -print` 显式 print 未实现（后置）。
- 括号在 shell 需转义 `\(` `\)`，经 argv 传入为 `(` `)`（解析器认两种）。
- 未补 -iname/-path/-perm/-mtime/-newer/-prune/-delete（POSIX 子集外，后置）。
- commit: `3e29feb`
