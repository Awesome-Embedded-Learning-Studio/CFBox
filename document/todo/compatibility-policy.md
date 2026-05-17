# 兼容性策略

CFBox 的目标是成为面向嵌入式、容器和 rescue 场景的 BusyBox 替代品。兼容性策略必须明确，否则每个 applet 都会陷入 POSIX、BusyBox、GNU 行为差异的局部争论。

## 裁决优先级

当 POSIX、BusyBox、GNU 行为冲突时，按以下顺序裁决：

1. **POSIX 语义优先**。
2. **BusyBox 高频行为优先**。
3. **GNU 长选项作为增强**。
4. 体积、复杂度或安全风险过高的扩展可以拒绝。
5. 所有意差异必须文档化。

### 裁决案例

**案例 1：`test -a` / `test -o` 语义**

POSIX 将 `-a`（AND）和 `-o`（OR）标记为 obsolescent。GNU coreutils 和 BusyBox 均支持。裁决：保留支持，因为大量现有脚本依赖此行为；但在文档中标明 POSIX 已弃用。

**案例 2：`sort -h` 人类数字排序**

`-h` 是 GNU 扩展，不在 POSIX 中，BusyBox 支持。裁决：支持，因为它是运维高频需求（`du -h | sort -h`），且实现体积可控。

**案例 3：`echo -n` / `echo -e` 行为**

POSIX 对 `echo` 的 `-n`/`-e` 行为未严格定义，不同实现差异大。裁决：遵循 BusyBox 行为（`-n` 抑制换行，`-e` 解析转义序列），因为这是 BusyBox 替代场景的最高频预期。

## BusyBox 兼容面

CFBox 的直接替代对象是 BusyBox，因此生产 profile 中的高频命令应优先兼容 BusyBox：

- 短选项语义。
- 输出格式。
- 错误信息的关键信息。
- 多文件和部分失败退出码。
- symlink multicall 调用方式。

不要求复制 BusyBox 的历史 bug 或不安全行为。

### 输出格式对齐示例

`ls -l` 输出格式应与 BusyBox 对齐（字段顺序、日期格式、权限字符串），而非 GNU coreutils 的更详细格式。差异应记录在兼容性矩阵中。

## GNU 增强

GNU 长选项和用户体验增强可以加入，但必须满足：

- 不改变 BusyBox 高频短选项行为。
- 可通过 `--help` 清楚展示。
- 不显著增加核心 profile 体积。
- 有测试覆盖。

## POSIX 声明

在通过严肃 POSIX 子集测试前，文档应使用：

- "POSIX-like shell"
- "POSIX-oriented behavior"
- "passes documented POSIX subset"

避免直接声称"POSIX compatible"或"POSIX certified"。

## 不支持项记录

每个 applet 的文档必须列出：

- 已支持选项。
- 不支持的 BusyBox 高频选项。
- 不支持的 GNU 高频选项。
- 已知行为差异。
- 是否属于有意差异。

这比隐性缺口更利于生产用户评估风险。兼容性矩阵的建立和审计流程见 [Phase 0A](phases/phase-0a-baseline-inventory.md)。

## 退出码策略

多目标命令必须统一退出码原则：

- 全部成功：0。
- 部分失败：非 0，并继续处理后续目标，除非命令语义要求立即停止。
- 参数错误：与 BusyBox/POSIX 高频行为对齐。
- 系统调用失败：错误信息包含目标对象和 errno 语义。

### 首批退出码审计对象

| 命令 | 预期退出码语义 | 备注 |
|------|---------------|------|
| `cp` | 0=全部成功，1=部分复制失败 | 继续处理后续文件 |
| `mv` | 0=全部成功，1=部分移动失败 | 跨设备移动时回退到复制+删除 |
| `rm` | 0=全部成功，1=部分删除失败 | `-f` 时不存在文件不报错 |
| `find` | 0=正常，1=遍历错误 | 继续遍历后续目录 |
| `tar` | 0=成功，1=部分失败，2=致命错误 | 区分可恢复和不可恢复 |
| `dd` | 0=成功，1=错误 | 输出传输统计到 stderr |
| `mount` | 0=成功，非 0=各种挂载失败 | 错误信息含设备和挂载点 |
| `umount` | 0=成功，非 0=各种卸载失败 | EBUSY 时建议 `-l` |

## 实验性功能

实验性 applet 或选项必须：

- 不进入 production profile。
- 在文档中标注 maturity level。
- 不作为 v1.0 支持承诺的一部分。
- 在 release notes 中列出行为变化风险。

成熟度分级和升级条件见 [v1.0 生产可用标准](v1-production-criteria.md)。
