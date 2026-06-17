# Codex 等价命令（粘贴式 prompt）

> Codex / 无 slash 命令的客户端用：复制下列整段进对话即可。这些命令在 Claude Code 里对应 [.claude/commands/](../../.claude/commands/) 下的同名文件。Codex 会自动读根目录 [AGENTS.md](../../AGENTS.md)。
> CFBox 验证基线：`cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`（379 GTest）+ `bash tests/integration/run_all.sh`（54 脚本）。

## /resume
读 document/ai/PLAN.md 与 document/ai/DIRECTIVES.md，跑 `git log --oneline -10`。三行回答：①你在哪(最近✅批+commit) ②下一步(🔄批+范围,grep 定位文件) ③相关陷阱(PLAN 的 OPEN GOTCHAS)。git 与 PLAN 不一致先指出。只读不改。

## /status
读 document/ai/PLAN.md，紧凑打印批表(状态/范围/commit/测试)+最近一条 `git log --oneline -1`。只读。

## /next [批-id]
读 PLAN.md 与 DIRECTIVES.md。为「批<id,留空=🔄NEXT>」产出脚手架：①范围 ②触及文件(grep 给绝对路径;批不存在报错停) ③Result 签名草案(`cfbox::base::Result<T>`/`CFBOX_TRY`) ④完成门 build+ctest+run_all.sh 全绿 ⑤提交草案(英文、无 Co-Auth) ⑥gotcha。停下等确认,不开改。

## /done
跑 `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure`(再补 `bash tests/integration/run_all.sh`)。0 failed=绿。
- 红:列失败项,不改 PLAN,不提交,给方向后停。
- 绿:①更新 PLAN.md(批标✅+commit短hash+测试数,挪 NEXT,必要时同步 ROADMAP) ②起草提交 `<type>(<scope>): <简述>`(英文、无 Co-Auth),不自动提交 ③写 document/notes/<date>-<topic>.md(背景/目标/设计/决策/陷阱/验证,非 diff 复述) ④报告改动+测试+建议 git 命令。体积相关改动另跑 size-opt 核查 ≤550 KB。

## /roadmap
读 document/ai/ROADMAP.md(其链向 document/todo/production-roadmap.md),紧凑打印 Phase 树+状态,指出"当前焦点(Phase 2)之后下一个可启动的 Phase"。只读。

## /milestone [Phase-id]
读 ROADMAP.md 与 DIRECTIVES.md。为「<id,默认下一未启动 Phase>」propose:①目标范围 ②批分解(每批≈一commit+完成门) ③触及 applet/文件(grep 定位) ④与 DIRECTIVES 架构不变量(Result/RAII/体积/分发)契合点 ⑤风险/依赖。草案停下等确认;确认后写入 PLAN 并在 ROADMAP 标🔄。不开改。

## /audit
跑 `git --no-pager diff --stat` 与 `git --no-pager diff`。对照 DIRECTIVES 逐条查:命名/注释/无异常/Result+CFBOX_TRY 用法/APPLET_REGISTRY+CMake 开关一致性/RAII/体积/提交格式(无 Co-Auth)。列违规+定位+建议。只报告(除非要求改)。

## /quality（CFBox 独有）
读 .claude/质量审查需要.md(或 memory 的 cfbox-quality-scan-prompt)。对当前工作树跑 A-G 七维度扫描(架构复用/体积/安全/鲁棒性/工具链/测试覆盖/注释),每维度出表(检查项/状态/发现/严重度),末尾总结:总问题数+优先处理项。只报告(除非要求改)。
