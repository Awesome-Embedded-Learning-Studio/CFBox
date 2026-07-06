# 2026-07-06 — 结构与性能标尺横切批（PR#17 / PR#18）

## 背景
补档：Phase 2 收尾（2026-06-28 sh trap）后到 Phase 3 启动之间，main 上落了两轮横切改进，但 PLAN/ROADMAP 批次表一度滞后（resume 可信度受损）。本批补文档债 + 留批级记录。

两批均**不增删 applet、不增 GTest**，落的是跨 Phase 的工艺/性能基线（对标 Phase 1.5 代码质量审查，但聚焦结构与性能维度）。

## 批1：结构标尺（PR#17 `feat/test-floor-gates`）

- **季级标尺** [document/ai/STRUCTURE-TASTE.md](../ai/STRUCTURE-TASTE.md)：职责/DRY/边界/机械护栏。
- **refactor**：清 banned-pattern（`using namespace`、`std::endl`、C 风格裸 cast 等）+ 合并散落 helper。
- **机械护栏** [tests/check_structure_gates.sh](../../tests/check_structure_gates.sh)（53 行，banned-pattern grep + 头文件 layering 检查）挂进 [ci.yml](../../.github/workflows/ci.yml)，CI 守护。
- commit：a1c1028（docs）/ 1ef38be（refactor）/ 623fca7（test gate）/ merge fefcbc2。

## 批2：性能基线（PR#18 `feat/performance`）

- **季级标尺** [document/ai/PERFORMANCE.md](../ai/PERFORMANCE.md)：wall-clock 不动输出 / 4 步闭环（基线→改→测→回归）/ Phase 0 基建。
- **google-benchmark harness**（e229f05）：sort baseline，可复现微基准。
- **流式化 5 个 applet**（核心是把"全量缓冲"换成 O(1) 或分块流，对齐 BusyBox 行为）：
  - `io/for_each_line`（58782bf）：块读 + memchr 找换行，~7x 快（grep/wc/cat 已走该路径）。
  - `tar`（64286e6）：流式生成 archive，不再缓冲整档。
  - `cmp`（c9ba7b4）：双文件 lockstep 流，首字节差即早退。
  - `md5sum`（50d1b85）：64 KiB 块流式哈希，O(1) 内存（原 2x file）。
  - `sed`（3479cf1）：substitute regex 每 command 预编译一次，~4x 快。
- **end-to-end timing 脚本**（626749a）：cfbox vs coreutils 端到端计时。
- **交叉编译修**：`parse_int` 改 strtol 甩掉 `<charconv>`（e547e26，CI 交叉编译 OOM）；`tar` `file_size` 取模 cast size_t（4f154e9，armhf 32 位 `-Wconversion`）。
- merge f979b8f。

## 验证（补档时回测，2026-07-06）
- x86 GTest **436/0**（沿用 Phase 2 末，未动）。
- size-opt **439 KB**（v0.3.0 基线 418 KB → +21 KB；主因 io/tar 流式缓冲与 benchmark 链接产物，≤ 550 KB 预算内）。

## 陷阱（留给后续批/维护者）
- **benchmark 与 GTest 是两套**：google-benchmark 不计入 `ctest` 的 436；perf 回归看 benchmark + timing 脚本，不是看 GTest 数。结构与性能改动**不会**自动反映在 GTest 基线上，验收要换标尺。
- **structure gate 是 CI-only**：本地 `ctest` 不跑 [check_structure_gates.sh](../../tests/check_structure_gates.sh)；新增 `using namespace` / 越层 include 在本地绿、CI 红。改公共头后本地最好 `bash tests/check_structure_gates.sh` 先自检。
- **perf 批 armhf 冒烟已补（C 阶段，2026-07-06）**：PR#18 当初只覆盖到 `-Wconversion` 编译错，没跑 qemu 直执行冒烟。本批用 `/opt/arm-gnu-toolchain` static 编译 armhf 32 位干净（无回归），`qemu-arm-static` 跑 perf 批 5 个流式 applet 全绿（io `for_each_line` 经 grep、`md5sum`、`tar -cf`/`-tf`、`sed s/o/0/g`、`cmp` 早退）——确认 PR#18 流式化在 armhf 32 位无回归。环境基线见本地 memory `cfbox-armhf-smoke-environment`。
- 文档债教训：批级工作必须**当批**写进 PLAN 批次表 + notes，否则 `/resume`、`/status` 漂移（本批就是还这个债）。
