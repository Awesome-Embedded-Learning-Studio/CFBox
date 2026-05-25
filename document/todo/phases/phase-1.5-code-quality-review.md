# Phase 1.5: 代码质量审查

## 概述

**目标**：在 Phase 1 核心系统命令到位后、Phase 2 核心深化开始前，对所有 P0 applet 和核心基础设施进行一次全面质量审查，确保质量标准统一。

**动机**：Phase 1 新增了 24 个 applet 和多个基础设施头文件（signal.hpp、digest.hpp、base64.hpp、mount.hpp、tty.hpp）。在进入深化阶段之前，需要确认新代码和现有代码都达到统一的质量标准，避免在低质量基础上叠加更多功能。

**进入条件**：
- Phase 1 Wave 3 完成（`dd`、`stty`、`mount`、`umount`、`chroot` 到位并通过测试）。
- Phase 1 Wave 4 可与 Phase 1.5 并行推进。

**退出条件**：
- 所有 P0 applet 错误处理一致性检查通过。
- 代码风格全量检查通过（clang-format）。
- 每个 P0 applet 测试覆盖确认（≥1 GTest + ≥1 集成测试）。
- 二进制体积在预算内（≤550 KB）。
- 最简兼容性矩阵建立。
- 所有发现的问题已记录为 issue 或已修复。

---

## 审查清单

### 1. 错误处理一致性

**检查范围**：所有 P0/P1 applet（Phase 1 新增 24 个 + 原有 13 个核心 applet）。

**审查要点**：

| 检查项 | 标准 | 通过条件 |
|--------|------|----------|
| std::expected 使用 | 所有 applet 使用 `base::Result<T>` + `CFBOX_TRY` | grep 无裸 throw/catch |
| 错误信息格式 | `命令名: 目标: 错误描述` | 格式统一，包含上下文 |
| 退出码对齐 | 0=成功，1=部分失败，2=参数错误 | 与 BusyBox/POSIX 行为一致 |
| 部分失败处理 | 多目标命令继续处理后续目标 | cp/mv/rm/chmod/chown 行为正确 |
| 权限错误报告 | 包含目标路径和 errno 语义 | 用户能理解失败原因 |

**执行方式**：逐文件审查 `src/applets/` 下所有 P0/P1 applet 的错误路径。

### 2. 代码风格统一

**检查范围**：所有新增和修改的文件。

**审查要点**：

| 检查项 | 工具 | 通过条件 |
|--------|------|----------|
| 格式化 | clang-format（已有 .clang-format） | `git diff --check` 无输出 |
| 头文件包含 | 目视检查 | 无多余包含，包含顺序合理 |
| 命名一致性 | 目视检查 | 函数/变量/类型命名与现有代码风格一致 |
| 注释规范 | 目视检查 | 无冗余注释，必要处有 WHY 注释 |

**执行方式**：运行 `clang-format --dry-run --Werror` 检查全量文件。

### 3. 测试覆盖快照

**检查范围**：Phase 1 新增 24 个 applet + 原有 13 个核心 applet。

**审查要点**：

| applet 类别 | 最低要求 |
|-------------|---------|
| P0（chmod/chown/dd/mount/stty 等） | ≥3 GTest + ≥1 集成测试，覆盖正常和错误路径 |
| P1（killall/halt/flock/setsid 等） | ≥1 GTest + ≥1 集成测试 |
| P2（sha*/base64/zcat） | ≥1 GTest + ≥1 集成测试 |
| 核心深化 applet | 新增功能有对应测试 |

**执行方式**：列出每个 applet 对应的测试文件，确认存在且非空。

### 4. 体积检查

**检查范围**：full profile + size-opt + LTO + strip 构建。

**审查要点**：

| 检查项 | 标准 |
|--------|------|
| 总体积 | ≤550 KB |
| Phase 1 增量 | ≤104 KB（从 446 KB 到 550 KB） |
| 单 applet 贡献 | 无异常大的 applet（预期每 applet ~4-6 KB） |
| 基础设施体积 | signal.hpp ~2KB、digest.hpp ~12KB、base64.hpp ~3KB、mount.hpp ~2KB、tty.hpp ~3KB |

**执行方式**：编译 full profile，测量体积，输出 section diff + Top symbols。

### 5. 兼容性记录

**检查范围**：所有 P0 applet（8 个新增 + 原有核心 applet）。

**审查要点**：

| applet | 对比目标 | 差异分类 |
|--------|---------|---------|
| `chmod` | BusyBox chmod | match / acceptable-diff / defect |
| `chown` | BusyBox chown | match / acceptable-diff / defect |
| `chgrp` | BusyBox chgrp | match / acceptable-diff / defect |
| `dd` | POSIX dd | match / acceptable-diff / defect |
| `mount` | BusyBox mount | match / acceptable-diff / defect |
| `umount` | BusyBox umount | match / acceptable-diff / defect |
| `stty` | BusyBox stty | match / acceptable-diff / defect |
| `chroot` | BusyBox chroot | match / acceptable-diff / defect |
| `grep` | BusyBox grep | match / acceptable-diff / defect |
| `sed` | BusyBox sed | match / acceptable-diff / defect |
| `find` | BusyBox find | match / acceptable-diff / defect |
| `tar` | BusyBox tar | match / acceptable-diff / defect |
| `cp` | BusyBox cp | match / acceptable-diff / defect |

**执行方式**：运行 `tests/differential/run_diff.sh`（Phase 0-lite 建立），记录每个 applet 的差异分类。

---

## 产出

Phase 1.5 完成后输出以下文档：

1. **质量审查报告**（`document/todo/phase-1.5-review-report.md`）
   - 每项审查的检查结果
   - 发现的问题列表和分类
   - 已修复/待修复/推迟的状态

2. **兼容性矩阵**（更新到 `document/todo/baseline-inventory.md`）
   - 每个 P0 applet 的差异分类
   - 可接受差异的具体描述

3. **体积报告**（`document/todo/phase-1.5-size-report.md`）
   - 当前体积 vs 基线 vs 预算
   - 每个新增 applet 的体积贡献
   - Top symbols 分析

---

## 与其他阶段的关系

```
Phase 1 (Wave 1-3)  →  Phase 1.5 (质量审查)  →  Phase 2 (核心深化)
                         ↓
                    Phase 1 (Wave 4) 可并行
```

- Phase 1.5 在 Wave 3 完成后触发，不等待 Wave 4。
- Wave 4（SHA/base64/zcat）可与 Phase 1.5 并行推进。
- Phase 2（核心深化）必须等 Phase 1.5 审查通过后才能开始。
- 审查中发现的问题不阻塞 Wave 4，但必须在 Phase 2 开始前修复或记录。
