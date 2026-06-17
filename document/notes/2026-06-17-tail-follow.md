# tail -f follow 模式（2026-06-17，Phase 2 批1）

## 背景
`tail.cpp` 此前只有静态 tail（`-n`/`-c`/`+N`），无 follow。日志跟踪是运维高频场景，缺失。PLAN 批1 要求 `-f`（+ `-F` retry + 干净退出）。

## 目标
- `-f` 跟踪文件追加数据
- `-F` 跟踪 + 轮转/truncate 后重开
- SIGINT/SIGTERM 干净退出
- 完成门：ctest + 集成全绿，size-opt ≤ 550 KB

## 设计 / 决策

**follow 方案：外部大模型仲裁后采纳 A′（fd-based stat 轮询），否决 inotify。**

起初拟 `stat(path)` 比 size 读区间，仲裁指出核心缺陷：path 替换竞态 + `-f` 本质是 follow descriptor。修正为：保持 fd 打开、offset 为消费位置；`fstat(fd)` 查同 inode 截断 → `lseek(0)`；`read()` 到 EOF；仅 `-F` `stat(path)` 核 dev/ino 检测轮转。

**否决 inotify**：Linux-only、第二套状态机、体积；1s 轮询是 BusyBox 级取舍（BusyBox tail 本身无 inotify 后端，`inotifyd` 是独立 applet）。inotify 未来只作 wake-hint（构建开关隔离），不维护第二套状态机。

**多文件公平**：round-robin sweep + 每文件每轮 64 KiB quantum + backlog 标志；快写文件达 quantum 未 EOF 则下轮不 sleep，防饥饿。

**-F retry**：初始缺失 → fd=-1 每 interval 重试、首次出现从字节 0 读（非尾部 N 行，免丢早期内容）；文件消失 → 保留旧 fd（被 unlink 仍可能被写）+ pathname 重试；inode 变化 → open pending_fd → 旧 fd drain 到 EOF → 切换（比立即 close 更贴轮转实际）；同 inode 截断 → `fstat` size<offset → `lseek(0)`。

**等待原语**：`nanosleep`+EINTR（第一版）。仲裁指出 flag+nanosleep 检查-睡眠间有微小竞态（最坏多等一个 interval）；ppoll 原子 mask 可闭，但代码量大，第一版接受 nanosleep，ppoll 后置。

**信号**：`sigaction`（非 `signal`，可移植语义弱）+ RAII guard 恢复旧 handler（CFBox 是 multi-call binary，必须不污染同进程后续 applet）。handler 只设 `volatile sig_atomic_t`。退出码 0 是 CFBox 产品语义——**非** coreutils 兼容（GNU tail 默认终止，shell 得 130/143）；装 handler 的真实理由是干净 flush stdout。

**CFBox 特化适配**：
- 新建 `unique_fd`（io.hpp）：fd 是值非指针，用轻量 class 而非 `unique_ptr<int,...>`（免堆分配，全 noexcept 兼容禁异常）。
- stat 走 `<sys/stat.h>` raw，**不走 fs_util**（fs_util::status 拉 `std::filesystem`，体积红线）；follow 是字节流消费状态机，非通用文件系统操作，不违反 DIRECTIVES A 维度。
- 初始 tail 用同一 fd（read_fd_all）避免 reopen 竞态丢字节。

## 陷阱（GOTCHA）
- **poll 对磁盘文件无效**（POLLIN 永远就绪）：tui.hpp 的 poll 封装是 stdin 按键，不能套用文件 follow。必须 stat 轮询或 inotify。
- **轮询无法可靠检测 truncate 后一周期内长过旧 offset**（`st_size<offset` 启发式失效）——固有限制，非否决项。
- **multi-call binary 改全局信号必须 RAII 恢复**（已回填 PLAN GOTCHA #6）。
- **初始 tail 全量读**（read_fd_all）：沿用现有 tail 行为，大文件内存风险（与现有 tail 同，后续可优化尾部读）。

## 验证
- ctest：**381/381** 全绿（379 原有 + 2 新 `TailFollow.NoFilesErrors` / `BadIntervalErrors`）。
- 集成 run_all.sh：**All passed**，含 tail follow 4 用例（-f 追加输出 / -f SIGINT 退出 0 / -F retry / 多文件 header）。
- size-opt：**410 KB** stripped（基线 406，+4 KB，预算 550）。

## 后续
- **后置（P2/下批可选）**：inotify wake-hint（构建开关）、ppoll 严格即时退出、`-F` 错误日志去重打磨、stdin follow、初始 tail 尾部读优化（免大文件全量内存）。
- 衔接：Phase 2 批2 `cp -a`。
