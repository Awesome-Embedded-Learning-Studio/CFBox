# tests/differential — 对照测试（cfbox vs busybox）

把 cfbox 的输出和 BusyBox oracle 逐字节比，抓"输出漂移"。整体方法和判定口径见 [document/ai/COVERAGE.md](../../document/ai/COVERAGE.md)。

## 跑

```bash
bash tests/differential/run_all.sh
# 或指定被测二进制 / oracle：
CFBOX=/path/to/cfbox BUSYBOX=/path/to/busybox bash tests/differential/run_all.sh
```

需要预编的 busybox oracle（默认 `competition/busybox/busybox`；在 `competition/busybox` 里 `make`）。

## 机制

[framework.sh](framework.sh) 提供：

- `run_diff "DESC" -- APPLET ARGS…` — 命令行参数输入
- `run_diff_in "DESC" "INPUT" -- APPLET ARGS…` — stdin 输入
- `diff_summary "name"` — 汇总，有未登记新差异则退出 1

每个测试文件（`test_diff_*.sh`）source framework，堆 run_diff 用例；`run_all.sh` 跑全部 `test_diff_*.sh`。

## 结果标签

- **MATCH** — stdout + 退出码与 busybox 全一致。
- **ACCEPTABLE** — 已登记、可接受的差异（格式风格 / POSIX 允许的行为差）。
- **DEFECT** — 已登记、待修的缺陷（修好后从 `known_diffs` 删掉那一行）。
- **NEW_DIFF** — 没见过的差异 → `diff_summary` 报错退出，提醒 triage。

## 加用例 / triage

- 加用例：在某个 `test_diff_*.sh` 里加一行 `run_diff "DESC" -- APPLET ARGS`。
- triage：把 NEW_DIFF 对应的 `STATUS DESC...` 写进 [known_diffs](known_diffs)（ACCEPTABLE 或 DEFECT）。
- 修好一个 DEFECT 后，从 `known_diffs` 删掉那一行——对照测试会自动重新开始盯它。

## 当前覆盖

[test_diff_coreutils.sh](test_diff_coreutils.sh)：14 个文本处理 applet（echo / cat / wc / head / tail / sort / uniq / rev / basename / dirname / tr / cut / seq / fold / expand / printf / true / false），33 case。扩面到 grep / sed / tar / cp / ls / find 等见 [document/ai/PLAN.md](../../document/ai/PLAN.md) Phase 4。
