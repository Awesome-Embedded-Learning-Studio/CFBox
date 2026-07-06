# 2026-07-06 — Phase 3 批1：socket.hpp 基础设施 + nc（Wave 0）

## 背景
Phase 3 网络最小闭环的第一锹。按 [phase-2-network.md](../todo/phases/phase-2-network.md) 依赖图，`socket.hpp` 是所有 Wave 的地基；选 `nc` 作为第一个消费者形成"基础设施 + 可用 applet"闭环（对应文档 3.2 的 nc echo 测试场景）。

## 设计决策

- **socket.hpp 复用 `cfbox::io::unique_fd`，不另造 `SocketFd` 类**（DRY，[STRUCTURE-TASTE](../ai/STRUCTURE-TASTE.md) §一.2）。文档 1.1 建议的 `SocketFd` 是 pre-code 设计；落地时 `io::unique_fd` 已是通用 fd RAII，socket 也是 fd，再包一层是死抽象。落成 header-only inline 自由函数（`make/resolve/dial/listen_on/accept_one/bound_port/format_addr`），对齐 io.hpp 风格。
- **nc relay 单进程 poll(2)，不 fork**（[PLAN GOTCHA #6](../ai/PLAN.md) 多调用 binary 全局状态）。fork 会污染同进程后续 applet 调用；poll 双向中继无副作用，退出干净。
- **SHUT_WR 半关**：stdin EOF 后 `shutdown(sock, SHUT_WR)`，让 request/response peer 仍能回复（传统 netcat 语义）。
- **双栈**：dial 走 `getaddrinfo(AF_UNSPEC)` 自动 v4/v6；listen_on 按 addr 含 `:` 判 v6。format_addr 用 `inet_ntop`（不用 sprintf，过 structure gate 门 1）。

## 踩坑（本批 3 个，都修了）

1. **`CFBOX_TRY` 不能在返回 `int` 的 applet 入口用**：宏展开是 `return std::unexpected(...)`，而 `nc_main` 返回 int → 编译错 "cannot convert unexpected<Error> to int"。`CFBOX_TRY` 只在返回 `Result<T>` 的函数里用（如 socket.hpp 内部）。applet 入口手动 `auto x = parse_int(...); if (!x) { CFBOX_ERR(...); return 2; }`，对齐 hostname.cpp。
2. **relay 初版把 `local` 当既读又写的 fd**：`relay(STDIN_FILENO, conn)` 里读到的 socket 数据 `write(local=fd0)` —— 写到 fd 0（stdin），数据丢失（手动 echo 复测 `out=''`）。netcat 是 stdin(0)→sock、sock→stdout(1) **两个方向四个 fd**。改成 `relay(sock_fd)` 内部固定 stdin/stdout，server 收到的数据正确落到重定向的 out 文件。
3. **test_nc.sh 的 `kill` 在 `set -e` 下炸**：server 在 client 退出后 relay 自然结束、进程已死，`kill $server` 返回非 0 触发 `set -e`，trap rm 跑了但 `echo`/`[[ ]]` 没跑（exit 1 无输出）。加 `|| true`。

## 验证
- GTest **440/440**（+4：SocketTest LoopbackEcho/ResolveLoopback/FormatAddrV4/FormatAddrV6）。
- 集成 **55 脚本**全绿（test_nc.sh 新增）。
- structure gate PASS（0 sprintf/stoi/裸 fopen/新 layering）。
- size-opt **451 KB**（基线 439 → +12 KB，nc + socket.hpp，≤ 550 KB 预算）。
- commit: `10f811f`

## 陷阱（留给后续批/维护者）
- **nc 是最小版**：只支持 `-l/-p/-s`。文档列的 `-w`(timeout)/`-e`(exec)/`-z`(scan)/UDP 模式均未实现（HELP 只列已实现的，避免名实不符）。Wave 3 收尾或后续批补。
- **socket.hpp 无超时/DNS 缓存/keepalive**：后续 wget/traceroute 按需扩。`dial` 是 happy-eyeballs-lite（逐个试 resolved addr），非并行。
- **armhf 已验证（C 阶段）**：`/opt/arm-gnu-toolchain`（arm-none-linux-gnueabihf gcc 15.2）static 编译，32 位 `-Wconversion` 干净；`qemu-arm-static` 冒烟 nc loopback echo 端到端通过（`armhf-nc-works`，server+client 双进程）。armhf static 体积 1235 KB（v0.3.0 基线 1195 → +40 KB，nc + socket.hpp）。
- **net_util.hpp 还没建**：Wave 1（ifconfig/ip/route/netstat）依赖它读 `/proc/net/*` + ioctl，是下一批的前置。
