# CFBox — Codex 工作指引

> Codex 自动读本文件；看不到 CLAUDE.md 与 Claude 私有 memory，故本文件自洽。等价命令的粘贴式 prompt 见 [document/ai/prompts.md](document/ai/prompts.md)。

C++23 现代 BusyBox 替代品（单二进制，115 applet，379 GTest，406 KB size-opt）。错误经 `cfbox::base::Result<T>`（`std::expected<T, Error>`）+ `CFBOX_TRY`，禁异常/RTTI。

## 文档分层（按需读）
- [document/ai/DIRECTIVES.md](document/ai/DIRECTIVES.md) — 架构铁律 + 约定 + 操作模型
- [document/ai/ROADMAP.md](document/ai/ROADMAP.md) — Phase 全树 + 状态（薄索引，链向 [document/todo/](document/todo/)）
- [document/ai/PLAN.md](document/ai/PLAN.md) — 当前焦点批级进度
- [document/ai/CODING-TASTE.md](document/ai/CODING-TASTE.md) — 编码/注释风格权威（写代码前读）
- [document/notes/](document/notes/) — 批级工作记录（`<date>-<topic>.md`）

## 始终遵守
- **C++23，禁异常/RTTI**：错误经 `cfbox::base::Result<T>`（`std::expected`）+ `CFBOX_TRY`；错误报告用 `CFBOX_ERR`。勿在各 applet 重写——用 `cfbox::fs::*`/`cfbox::io::*`。
- **验证用** `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`（379 GTest）**且** `bash tests/integration/run_all.sh`（54 脚本）；体积改动跑 size-opt 核查 ≤ 550 KB。
- **一批一commit一验证**；绿才提交。
- **提交信息** `<type>(<scope>): <英文简述>`——纯描述、**不带 Co-Authored-By / AI 署名**。
- **改前查牵连**：grep `APPLET_REGISTRY`/`CFBOX_ENABLE_*`/`cfbox::fs::` 引用方；同步 ROADMAP↔PLAN↔`document/todo`↔git。
- 新增 applet 同步改注册表 + CMake 开关。

## 回到仓库
读 [document/ai/PLAN.md](document/ai/PLAN.md) + `git log --oneline -15`，再决定下一步。粘贴式 prompt 见 [document/ai/prompts.md](document/ai/prompts.md)。
