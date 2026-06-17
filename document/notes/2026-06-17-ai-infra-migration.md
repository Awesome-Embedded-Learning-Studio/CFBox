# 迁移 CinuxOS AI 基础建设到 CFBox（2026-06-17，基建）

## 背景
CFBox 有静态规划文档（[document/todo/](../todo/) Phase 路线图、[competition/](../../competition/) 差距分析、[changelogs/](../../changelogs/) 发版记录），但缺 agent 工作流层——没有 CLAUDE.md/AGENTS.md 指引、没有 slash 命令驱动的「一批一commit一验证」闭环、没有分层 agent 文档、没有批级工作记录。姊妹项目 `~/CinuxOS` 已沉淀成熟 AI 基础建设（7 命令 + 5 件套分层文档 + Codex 粘贴 prompt + notes），长期验证有效。

## 目标
把 CinuxOS 的 agent 工作流**适配 CFBox 实际后**迁过来，为 Phase 2+ 迭代和讨论提供统一操作模型与文档分层。不照抄——技术栈差异需逐项翻译（见下表）。

## 设计 / 决策
- **文档布局**：新建 [document/ai/](../ai/)（DIRECTIVES/ROADMAP/PLAN/CODING-TASTE/prompts 五件套），ROADMAP 薄索引、转链到既有 [document/todo/](../todo/) 不重复内容。
- **提交署名**：对齐 CinuxOS——commit 纯描述、**不带 Co-Authored-By / AI trailer**（覆盖 Claude Code 出厂默认）。
- **命令范围**：全套 7 命令 + 新增 `/quality`（复用 CFBox 独有的 A-G 七维度扫描）。
- **术语翻译**（CinuxOS → CFBox）：C++17→**C++23**；`ErrorOr<T>`→**`cfbox::base::Result<T>`=std::expected**；`return Error::Xxx`→**`CFBOX_TRY`/`std::unexpected`**；`kprintf`→**`CFBOX_ERR`**；`run-kernel-test`→**`ctest`+`run_all.sh`**；Feature/Milestone→**Phase**。新增头等约束：**体积预算 ≤550 KB**。
- **提交语言**：保留**英文**简述（对齐 cfbox 既有 git 历史 `feat:`/`perf:`/`ci:`/`fix:`），未沿用 CinuxOS 的中文 commit——一致性优先。

## 陷阱（GOTCHA）
- **commit 署名 vs harness 默认冲突**：Claude Code 出厂默认在 commit 末尾加 `Co-Authored-By` trailer；DIRECTIVES L2 明确禁止。后续在 CFBox 提交须遵循项目规范（无署名），不遵循 harness 默认。
- **Phase 文件名偏移**：[document/todo/phases/](../todo/phases/) 里 `phase-2-network.md` 实际对应「Phase 3 网络」，文件名与概念 Phase 号差 2；ROADMAP 链接按真实文件名给，不按概念号。

## 验证
本批是纯文档/配置迁移，无代码改动，无 ctest 验证。迁移后核验：新文档链接指向真实文件；grep 无 `ErrorOr`/`run-kernel-test`/`Cinux-Base` 残留术语；三处（DIRECTIVES/CLAUDE/AGENTS）「无 Co-Authored-By」表述一致；`/status` `/resume` `/quality` 可跑通。

## 后续
- 用 `/next 批1`（tail -f）启动 Phase 2 第一批实战，验证工作流闭环。
- 视使用反馈增删命令 / 调整 PLAN 批顺序。
- CinuxOS 侧若 DIRECTIVES/CODING-TASTE 演进，回流同步到 CFBox 版本。
