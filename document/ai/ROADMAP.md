# CFBox Roadmap — 长弧视图

> Tier 2（里程碑级）。**本文件是薄索引，不复制内容**——人类可读的完整路线图在 [document/todo/production-roadmap.md](../todo/production-roadmap.md)（全局总纲）与 [document/todo/phases/](../todo/phases/)。批级进度见 [PLAN.md](PLAN.md)，铁律见 [DIRECTIVES.md](DIRECTIVES.md)。
> 状态：✅ 完成 / 🔄 进行中 / ⏳ 未启动。数据源 [document/todo/README.md](../todo/README.md)（已核对，2026-06）。

## 阶段总览

| 阶段 | 状态 | 文档 | 目标 |
|------|------|------|------|
| Phase 0 | ✅（lite，并行收尾） | [phase-0a 基线盘点](../todo/phases/phase-0a-baseline-inventory.md) | 文档漂移修复、differential test 骨架、编译零 warning；与 Phase 1 并行 |
| Phase 1 | ✅ | [核心系统](../todo/phases/phase-1-core-system.md) | P0 系统命令（chmod/chown/dd/mount/stty 等 24 个新 applet） |
| Phase 1.5 | ✅ | [代码质量审查](../todo/phases/phase-1.5-code-quality-review.md) | 错误处理一致性、风格、测试覆盖、体积检查（A-G 扫描全过） |
| **Phase 2** | 🔄 **当前焦点** | [核心深化（同 Phase 1 文档 Part 3）](../todo/phases/phase-1-core-system.md) | tail -f、cp -a、test POSIX、ls -R/--color、grep -A/-B/-C、find 布尔、sh 深化 |
| Phase 3 | ⏳ | [网络最小闭环](../todo/phases/phase-2-network.md) | 基础网络配置、诊断、下载、连接调试 |
| Phase 4 | ⏳ | [生产质量门禁深化](../todo/phases/phase-3-quality.md) | fuzzing、benchmark、POSIX 子集、release 工程 |
| Phase 5 | ⏳ | [多用户与嵌入式运行时](../todo/phases/phase-4-multiuser.md) | login/getty/syslog/mdev/storage |
| Phase 6 | ⏳ | [长尾完备性](../todo/phases/phase-5-longtail.md) | vi、额外压缩格式、硬件工具、长尾 applet |

## 裁决原则（详见总纲）
- **P0** = v1.0 支持 profile 的阻断项；**P1** = 生产高频项；**P2** = 提升可用性常用项；**P3** = 长尾/领域专用。
- 场景闭环优先于 applet 数量；核心命令深度优先于长尾覆盖；零运行时依赖（可选依赖经构建开关隔离）。
- 兼容性裁决见 [compatibility-policy.md](../todo/compatibility-policy.md)；v1.0 验收边界见 [v1-production-criteria.md](../todo/v1-production-criteria.md)。

## 当前焦点
**Phase 2 核心命令深化** 🔄（批级进度见 [PLAN.md](PLAN.md)）。基线 399 GTest + 54 集成脚本，123 applet，418 KB（size-opt）。
> **v0.3.0 已发布**：L2 rootfs 启动骨架（`init` askfirst / `mount` / `mdev` / `umount` / `swapoff` / `reboot`·`poweroff`，117→123 applet）+ `tail -f` —— cfbox 在 i.MX6ULL 上作为 PID 1 替代 BusyBox，端到端实测。详见 [changelogs/v0.3.0.md](../../changelogs/v0.3.0.md)。
> 焦点回到 Phase 2 核心深化：批2 `cp -a`（归档模式）→ `test` POSIX → `ls -R`/`--color`。

## 当前焦点之后下一个可启动的
**Phase 3 网络最小闭环**（基础网络配置/诊断/下载）——Phase 2 核心命令深度到位后启动。更远：Phase 4 质量门禁（fuzzing/release 工程）→ Phase 5 多用户 → Phase 6 长尾。
