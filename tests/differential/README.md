# tests/differential — 对照冒烟测试

把 cfbox 的输出和"标准答案"（参考实现）逐字节比，抓"输出漂移"。整体方法和判定口径见 [document/ai/COVERAGE.md](../../document/ai/COVERAGE.md)。

## 跑

```bash
bash tests/differential/run_diff_smoke.sh
# 或指定被测二进制：
CFBOX=/path/to/cfbox bash tests/differential/run_diff_smoke.sh
```

## 结果标签

- **MATCH** — 标准输出 + 退出码与参考实现全一致。
- **ACCEPTABLE** — 已登记、可接受的差异（如格式风格、POSIX 允许的行为差）。
- **DEFECT** — 已登记、待修的缺陷（修好后从 `known_diffs` 删掉那一行）。
- **NEW_DIFF** — 没见过的差异 → 脚本报错退出，提醒 triage。

## 当前覆盖（起步）

5 个流式命令：cat / head / tail / wc / sort，参考答案用系统 coreutils（`type -P` 解析，绕开别名）。

- 输入走 stdin 重定向（暂不测文件名参数 / 多文件，留给后续）。
- 判定口径：标准输出逐字节 + 退出码；stderr 只展示、不卡判定（报错措辞各实现不同，逐字节比是噪声）。
- 全程 `LC_ALL=C`，排除 locale 排序差异。

## 加新用例 / triage 新差异

- 加用例：在 `run_diff_smoke.sh` 里加一行 `compare <applet> <case_id> <fixture> <args…>`。
- triage：把 `NEW_DIFF` 对应的 `STATUS applet case_id` 写进 `known_diffs`（ACCEPTABLE 或 DEFECT）。
- 修好一个 DEFECT 后，从 `known_diffs` 删掉那一行——对照测试会自动重新开始盯它。

## 当前已知差异摘要

见 `known_diffs`。首跑（2026-06-28）三类：
- `head`/`tail` 在结尾无换行的文件上凭空补换行（DEFECT，待修）。
- `sort -rn` 比较器违反 strict-weak-ordering（DEFECT，第 3 批修）。
- `wc` 列宽、`sort -n` 并列稳定性：格式 / POSIX 允许的差异（ACCEPTABLE）。
