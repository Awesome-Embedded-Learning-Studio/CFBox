# 持续集成（CI）

CI 流水线配置位于 [.github/workflows/ci.yml](../.github/workflows/ci.yml)。

## 触发条件

- **push** 到 `main` 分支
- **pull_request** 目标为 `main` 分支

## 流水线阶段

| 阶段 | 说明 | 触发条件 |
|------|------|----------|
| **native** | 原生 x86-64 构建 + 测试 | 所有 push/PR |
| **release-size** | Release 构建 + 大小报告 | 所有 push/PR |
| **cross-compile** | 交叉编译（aarch64 + armhf） | 所有 push/PR |
| **qemu-user-test** | QEMU 用户模式测试 | 所有 push/PR |
| **qemu-system-test** | QEMU 系统模式引导测试 | 仅 main/release 分支 |

## 阶段详情

### native

- 使用 `g++-13` 构建 Debug 版本
- 自动启用 AddressSanitizer 和 UBSan
- 运行 108 个 GTest 单元测试
- 运行 16 套 Shell 集成测试

### release-size

- 使用 `-Os` + LTO 构建 Release 版本
- 使用 `size` 命令报告各 section 大小
- Strip 后测量最终二进制大小
- 上传 release 二进制为 artifact

### cross-compile

- 矩阵构建：aarch64 + armhf
- 分别编译动态链接和静态链接版本
- 使用 `file` 和 `size` 验证目标架构
- 上传静态二进制供后续 QEMU 测试使用

### qemu-user-test

- 依赖 cross-compile 阶段的静态二进制 artifact
- 安装 `qemu-user-static`
- 运行冒烟测试（`cfbox --list`、`cfbox echo`）
- 创建透明 wrapper 脚本，运行完整集成测试套件

### qemu-system-test

- 依赖 cross-compile 阶段的静态二进制 artifact
- 从 `third_party/linux` 子模块编译 Linux 内核（带缓存）
- 构建 initramfs（CFBox 作为 PID 1）
- QEMU 系统模式引导完整 Linux 内核
- 解析 init applet 输出，统计测试结果
- 仅在 `main` 和 `release-*` 分支上运行
