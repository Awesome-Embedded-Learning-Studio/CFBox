# CFBox — Claude Code 工作指引

C++23 现代 BusyBox 替代品（单二进制，123 applet，399 GTest，418 KB size-opt）。Claude 是长期主力开发。

## 文档分层（按耐久度，按需读）
- [document/ai/DIRECTIVES.md](document/ai/DIRECTIVES.md) — 架构铁律 + 约定 + 操作模型（年，最稳）
- [document/ai/ROADMAP.md](document/ai/ROADMAP.md) — Phase 全树 + 状态（薄索引，链向 [document/todo/](document/todo/)）
- [document/ai/PLAN.md](document/ai/PLAN.md) — 当前焦点批级进度（批级，最易变）
- [document/ai/CODING-TASTE.md](document/ai/CODING-TASTE.md) — 编码/注释风格权威（写代码前读）
- [document/ai/COVERAGE.md](document/ai/COVERAGE.md) — 测试/正确性/差分标尺（覆盖率怎么算、怎么防退化，季级）
- [document/ai/STRUCTURE-TASTE.md](document/ai/STRUCTURE-TASTE.md) — 结构与工艺标尺（职责/DRY/边界/机械护栏，季级）
- [document/ai/PERFORMANCE.md](document/ai/PERFORMANCE.md) — 性能标尺（wall-clock 不动输出/4步闭环/Phase0 基建，季级）
- [document/notes/](document/notes/) — 批级工作记录（`<date>-<topic>.md`）

## 始终遵守（每条便宜，违规代价大）
- **C++23，禁异常/RTTI**：错误经 `cfbox::base::Result<T>`（`std::expected`）+ `CFBOX_TRY`；错误报告用 `CFBOX_ERR`。
- **验证用** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`（399 GTest）**且** `bash tests/integration/run_all.sh`（54 脚本）；体积改动跑 size-opt 核查 ≤ 550 KB。
- **一批一commit一验证**；绿才提交。
- **提交信息** `<type>(<scope>): <英文简述>`——纯描述、**不带 Co-Authored-By / AI 署名**（覆盖 Claude Code 出厂默认加 trailer 的行为）。
- **改前查牵连**：grep `APPLET_REGISTRY`/`CFBOX_ENABLE_*`/`cfbox::fs::` 引用方；同步 ROADMAP↔PLAN↔`document/todo`↔git。
- 新增 applet 同步改注册表 + CMake 开关；POSIX 调用走 `cfbox::fs::*`，不各 applet 重复造轮子。

## 命令（`.claude/commands/`）
战术：`/resume` `/status` `/next [批]` `/done`
战略：`/roadmap` `/milestone [Phase]` `/audit`
质量：`/quality`（A-G 七维度扫描，CFBox 独有）

## 回到仓库
`/resume`（读 PLAN.md + git log）。Codex 等价粘贴 prompt 见 [document/ai/prompts.md](document/ai/prompts.md)。
