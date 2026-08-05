# CFBox Roadmap — 长弧视图

> Tier 2（里程碑级）。**本文件是薄索引，不复制内容**——人类可读的完整路线图在 [document/todo/production-roadmap.md](../todo/production-roadmap.md)（全局总纲）与 [document/todo/phases/](../todo/phases/)。批级进度见 [PLAN.md](PLAN.md)，铁律见 [DIRECTIVES.md](DIRECTIVES.md)。
> 状态：✅ 完成 / 🔄 进行中 / ⏳ 未启动。数据源 [document/todo/README.md](../todo/README.md)（已核对，2026-06）。

## 阶段总览

| 阶段 | 状态 | 文档 | 目标 |
|------|------|------|------|
| Phase 0 | ✅（lite，并行收尾） | [phase-0a 基线盘点](../todo/phases/phase-0a-baseline-inventory.md) | 文档漂移修复、differential test 骨架、编译零 warning；与 Phase 1 并行 |
| Phase 1 | ✅ | [核心系统](../todo/phases/phase-1-core-system.md) | P0 系统命令（chmod/chown/dd/mount/stty 等 24 个新 applet） |
| Phase 1.5 | ✅ | [代码质量审查](../todo/phases/phase-1.5-code-quality-review.md) | 错误处理一致性、风格、测试覆盖、体积检查（A-G 扫描全过） |
| **Phase 2** | ✅ | [核心深化（同 Phase 1 文档 Part 3）](../todo/phases/phase-1-core-system.md) | tail -f、cp -a、test POSIX、ls -R/--color、grep -A/-B/-C、find 布尔、sh 深化（全完成） |
| Phase 3 | ✅ | [网络最小闭环](../todo/phases/phase-2-network.md) | socket/net_util/icmp 基础设施 + nc/ifconfig/ip/route/hostname/netstat + ifconfig 写 + ping/traceroute（11 applet） |
| Phase 4 | 🔄 | [生产质量门禁深化](../todo/phases/phase-3-quality.md) | 差异测试 + POSIX 标尺（✅ PR#21/#22）→ fuzzing、静态分析、替换测试、发布工程 v0.4.0 RC |
| Phase 5 | ⏳ | [多用户与嵌入式运行时](../todo/phases/phase-4-multiuser.md) | login/getty/syslog/mdev/storage |
| Phase 6 | ⏳ | [长尾完备性](../todo/phases/phase-5-longtail.md) | vi、额外压缩格式、硬件工具、长尾 applet |

## 裁决原则（详见总纲）
- **P0** = v1.0 支持 profile 的阻断项；**P1** = 生产高频项；**P2** = 提升可用性常用项；**P3** = 长尾/领域专用。
- 场景闭环优先于 applet 数量；核心命令深度优先于长尾覆盖；零运行时依赖（可选依赖经构建开关隔离）。
- 兼容性裁决见 [compatibility-policy.md](../todo/compatibility-policy.md)；v1.0 验收边界见 [v1-production-criteria.md](../todo/v1-production-criteria.md)。

## 当前焦点
**Phase 4 生产质量门禁 🔄 推进**（2026-07，PR#21/#22 已合 + `feat/differential-expand` 5 commits 待 push）：批1 POSIX 符合度标尺 + CI gate（baseline 71/97 → 82/97）✅；批2 `dd` + `sh` 10 POSIX builtins + 差分 harness（修 wc/cut）✅；批3 差异测试扩面（framework known_diffs 四级 + `run_diff_fs` 文件系统 + grep/sed/fileops 84 case + 修 sed/ls/mkdir 4 bug + 8 防回归 GTest）✅。当前基线 **493 GTest / 496 KB size-opt / 131 applet / POSIX 84% / 差分 84 case**。之前：Phase 3 网络闭环（11 applet）+ 结构/性能标尺横切（PR#17/#18）。批级记录见 [PLAN.md](PLAN.md) 与 [notes/](../notes/)。
> **下一站**：Phase 4 剩余轨道——Part 1.2 差异测试续扩（tar/gzip 需 framework 多步）、Part 2 fuzzing、Part 4 静态分析、Part 5 Alpine 替换测试、Part 7 发布工程（v0.4.0 RC）；或 Phase 3 增量（route add/del、hostname NAME、netstat -r/-i、IPv6）。

## 当前焦点之后下一个可启动的
**Phase 4 剩余轨道**（fuzzing/静态分析/Alpine 替换测试/release 工程）或 utility 长尾（15 个 POSIX missing，stty/strings/file/logger 先）；或回头补 Phase 3 增量（route add/del、hostname NAME、netstat -r/-i、IPv6）。更远：Phase 5 多用户 → Phase 6 长尾。
