# 2026-06-28 — grep -A/-B/-C 上下文窗口（Phase 2 批5a）

## 背景
grep 仅即时打印匹配行，无上下文。补 GNU `-A/-B/-C`，核心难点是流式（for_each_line 逐行 fgetc，不可回卷）下的"向前看"与组间分隔。

## 设计决策
- **向前上下文（-B）**：`before_buf` 为容量 B 的 ring（`vector<pair<line_num,content>>`，超容量 erase front）。非匹配、非 after 的行入缓冲；匹配时先 flush 缓冲作为前导上下文。
- **向后上下文（-A）**：匹配后置 `after_pending=A`，后续非匹配行只要 after_pending>0 就 emit 并递减；连续匹配重置 after_pending（合并块）。
- **组间 `--`**：`need_separator`（一块 after 耗尽时置位）+ `last_printed`（连续检测）。emit 前若 `need_separator && ln != last_printed+1` 打 `--`。正确处理"连续块合并不打 --"与"尾部不打 --"。
- **前缀一致性**：`emit` lambda 统一 `path:` + `line_num:` + content，上下文行与匹配行同格式。
- **互斥语义**：`-q/-l/-c` 下上下文不打印（`printing = !quiet && !files_with_matches && !count_only`），但 match_count 仍计数。
- **零回归**：默认（无 -A/-B/-C）after=before=0，不进缓冲逻辑，process_line 走原匹配即打印路径，**字节级不变**。

## 验证
- GTest +7（ContextAfter/Before/Both/Separator/Zero/WithLineNumbers/InvalidNumber），全量 **431/0**。
- 集成 test_grep.sh +4（-A 含 `--`、-B、-C、invalid -A 退 2），16 passed。
- size-opt **422 KB**（持平——vector<pair> 复用既有模板，无新膨胀）。

## 陷阱
- `-C` 覆盖 `-A/-B`（后指定优先，符合 GNU）。
- ring 用 vector erase front 是 O(n)，但 B 通常小；未用 deque 避免模板膨胀。
- 多文件上下文：每个文件独立 grep_file，`--` 不跨文件（GNU 跨文件也用 `--`，但本实现 per-file；多文件 -A 场景罕见，可接受）。
- commit: `b6920c3`
