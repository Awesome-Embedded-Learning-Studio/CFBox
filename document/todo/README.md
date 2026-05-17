# CFBox 生产化升级路线图

本文档集用于承接 CFBox 从 v0.1.0 走向 v1.0 生产可用的升级路线。它取代根目录旧路线图，作为后续 Phase 0 到 v1.0 的执行入口。

## 当前状态

| 项目 | 状态 |
|------|------|
| 版本 | v0.1.0 |
| Applet | 109 个 |
| 体积 | 约 446 KB（size-opt, LTO + strip） |
| 测试 | 331 个 GTest 单元测试 + 56 个集成脚本 |
| 已完成阶段 | 历史 Phase 0-4：构建、POSIX-like shell、coreutils、归档压缩、文本处理、procps、init |
| 主要缺口 | 权限/挂载/`dd`、网络、登录管理、系统日志、核心命令功能深度、生产发布工程 |

## 文档索引

| 文档 | 用途 |
|------|------|
| [生产化路线图](production-roadmap.md) | **全局总纲**：Phase 0-5 阶段顺序、核心 Applet 优先级、质量工程策略、发布与分发规范 |
| [v1.0 生产可用标准](v1-production-criteria.md) | v1.0 发布门槛：profile 标准、成熟度、测试、发布矩阵、体积预算 |
| [兼容性策略](compatibility-policy.md) | POSIX/BusyBox/GNU 行为冲突的裁决原则、退出码策略 |
| [VitePress 文档站](documentation-site.md) | 文档站信息架构、applet reference、cookbook |
| [Phase 0A-0F 前置门禁](phases/phase-0a-baseline-inventory.md) | 进入 Phase 1 前必须完成的质量、体积、IO、安全和工程地基 |
| [Phase 1 核心系统](phases/phase-1-core-system.md) | 权限、挂载、dd、stty、核心命令深化 |
| [Phase 2 网络](phases/phase-2-network.md) | 基础网络配置、诊断、下载 |
| [Phase 3 质量](phases/phase-3-quality.md) | fuzzing、benchmark、POSIX 子集、release 工程 |
| [Phase 4 多用户](phases/phase-4-multiuser.md) | login/getty/syslog/mdev/storage |
| [Phase 5 长尾](phases/phase-5-longtail.md) | vi、额外压缩格式、硬件工具 |

## 推荐阅读顺序

1. 先读 [v1.0 生产可用标准](v1-production-criteria.md)，明确最终验收边界。
2. 再读 [生产化路线图](production-roadmap.md)，确认阶段顺序、优先级和全局策略。
3. 进入任何新增 applet 工作前，完成 [Phase 0A-0F 前置门禁](phases/phase-0a-baseline-inventory.md)。
4. 实现 applet 前读对应 Phase 文档。
5. 改动兼容性相关行为前读 [兼容性策略](compatibility-policy.md)。
6. 编写用户文档前读 [VitePress 文档站](documentation-site.md)。

## 路线原则

- 场景闭环优先于 applet 数量。
- 核心命令功能深度优先于长尾命令覆盖率。
- 默认零运行时依赖，可选依赖必须通过构建开关隔离。
- 兼容性要可说明、可测试、可接受；不支持项必须公开记录。
- 每个阶段都必须同时交付实现、测试、文档和体积报告。
- Phase 0A-0F 是硬门禁；未达成确定指标时，不继续扩展 applet 数量。
