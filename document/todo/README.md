# CFBox 生产化升级路线图

本文档集用于承接 CFBox 从 v0.1.0 走向 v1.0 生产可用的升级路线。它取代根目录旧路线图，作为后续 Phase 0 到 v1.0 的执行入口。

## 当前状态

| 项目 | 状态 |
|------|------|
| 版本 | v0.2.0 |
| Applet | 115 个 |
| 体积 | 406 KB（size-opt, LTO + strip, `-fno-exceptions -fno-rtti`） |
| 测试 | 379 个 GTest 单元测试 + 56 个集成脚本 |
| 已完成阶段 | Phase 1 Wave 1（6 新 applet）+ Phase 1.5 代码质量审查（全部通过） |
| **当前阶段** | **Phase 2：核心命令深化**（tail -f、cp -a、test POSIX、ls -R/--color） |
| 主要缺口 | 核心命令功能深度、网络、登录管理、系统日志、生产发布工程 |
| 执行策略 | 按运维频率分批深化，每批发版 |

## 文档索引

| 文档 | 用途 |
|------|------|
| [生产化路线图](production-roadmap.md) | **全局总纲**：阶段顺序、核心 Applet 优先级、质量工程策略、发布与分发规范 |
| [v1.0 生产可用标准](v1-production-criteria.md) | v1.0 发布门槛：profile 标准、成熟度、测试、发布矩阵、体积预算 |
| [兼容性策略](compatibility-policy.md) | POSIX/BusyBox/GNU 行为冲突的裁决原则、退出码策略 |
| [VitePress 文档站](documentation-site.md) | 文档站信息架构、applet reference、cookbook |
| [Phase 0 轻量基建](phases/phase-0a-baseline-inventory.md) | 文档漂移修复、differential test 骨架；与 Phase 1 并行推进 |
| [Phase 1 核心系统](phases/phase-1-core-system.md) | P0 系统命令：chmod/chown/dd/mount/stty 等 24 个新 applet |
| [Phase 1.5 代码质量审查](phases/phase-1.5-code-quality-review.md) | 错误处理一致性、代码风格、测试覆盖、体积检查 |
| [Phase 2 核心深化](phases/phase-1-core-system.md#part-3-现有-applet-深化) | tail -f、cp -a、grep -A/-B/-C、find 布尔表达式、sh 深化 |
| [Phase 3 网络](phases/phase-2-network.md) | 基础网络配置、诊断、下载 |
| [Phase 4 质量](phases/phase-3-quality.md) | fuzzing、benchmark、POSIX 子集、release 工程 |
| [Phase 5 多用户](phases/phase-4-multiuser.md) | login/getty/syslog/mdev/storage |
| [Phase 6 长尾](phases/phase-5-longtail.md) | vi、额外压缩格式、硬件工具 |

## 推荐阅读顺序

1. 先读 [v1.0 生产可用标准](v1-production-criteria.md)，明确最终验收边界和中间里程碑。
2. 再读 [生产化路线图](production-roadmap.md)，确认阶段顺序、优先级和全局策略。
3. 开始 Phase 1 前了解 [Phase 0 轻量基建](phases/phase-0a-baseline-inventory.md) 的并行任务。
4. 实现 applet 前读 [Phase 1 核心系统](phases/phase-1-core-system.md) 的 Wave 分波和验收标准。
5. Phase 1 Wave 3 完成后读 [Phase 1.5 代码质量审查](phases/phase-1.5-code-quality-review.md)。
6. 改动兼容性相关行为前读 [兼容性策略](compatibility-policy.md)。
7. 编写用户文档前读 [VitePress 文档站](documentation-site.md)。

## 路线原则

- 场景闭环优先于 applet 数量。
- 核心命令功能深度优先于长尾命令覆盖率。
- 默认零运行时依赖，可选依赖必须通过构建开关隔离。
- 兼容性要可说明、可测试、可接受；不支持项必须公开记录。
- 每个阶段都必须同时交付实现、测试、文档和体积报告。
- Phase 0 采用轻量基建策略，与 Phase 1 并行推进，不阻塞新功能开发。
- Phase 1.5 质量审查在核心命令到位后执行，确保质量标准统一后再进入深化阶段。
