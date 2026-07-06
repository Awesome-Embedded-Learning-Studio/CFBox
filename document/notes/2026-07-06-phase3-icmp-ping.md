# Phase 3 批6（Wave 2）— icmp.hpp 基础设施 + ping

> 2026-07-06。新建 `icmp.hpp`（raw ICMP 共享层）+ `ping` applet。SOCK_RAW IPPROTO_ICMP
> 需 CAP_NET_RAW —— CI native/qemu-user 无权，集成 skip guard 接受 EPERM 或真 reply 两种结果；
> qemu-system（root）能真跑。纯函数（checksum/build/parse）单测全覆盖。

## 触及文件

- [include/cfbox/icmp.hpp](../../include/cfbox/icmp.hpp) — 新（header-only）：
  - `now_us()`（CLOCK_MONOTONIC 微秒，RTT + poll deadline 共用）
  - `checksum(span<uint8_t>) -> uint16_t`（RFC 1071，纯函数）
  - `build_echo_request(id, seq, payload, out) -> size_t`（type=ICMP_ECHO + 网络序 id/seq + 算后填 checksum）
  - `ParsedIcmp{type,code,id,seq,recv_ttl}` + `parse_icmp(span) -> Result<ParsedIcmp>`（按 IHL 剥 IP 头，recv_ttl 取 IP 头偏移 8）
  - `open_raw() -> Result<unique_fd>`（EPERM→err msg 含 strerror）
  - `RecvResult{from, msg}` + `recv_icmp(fd, match_id, timeout_ms) -> Result<RecvResult>`（match_id≠0 过滤非己 echo，monotonic deadline 循环，EINTR continue）
- [src/applets/ping.cpp](../../src/applets/ping.cpp) — 新 applet（`-c/-i/-W/-s/-q/-n`，getaddrinfo AF_INET resolve，SIGINT RAII guard 打 summary，EPERM exit 2 / resolve 失败 exit 1）
- 注册三件套（`CFBOX_ENABLE_PING`）
- [tests/unit/test_icmp.cpp](../../tests/unit/test_icmp.cpp) — 11 测（checksum 4：全 0/全 1/奇数/carry-fold；build_echo 3：字节布局/checksum 自洽（整体 checksum=0）/out 太小；parse 4：剥 IP 头/IHL≠6 含选项/太短/坏 IHL）
- [tests/integration/test_ping.sh](../../tests/integration/test_ping.sh) — 3 测（EPERM 或 reply 二选一 + 无参 exit 非零 + 坏 -c exit 非零）

## 关键设计决策

1. **`recv_icmp` match_id 双模式**：ping 传 `id`（过滤非己 echo，避免被主机其他 ICMP 干扰），
   traceroute 传 `0`（收任意 ICMP —— time-exceeded/port-unreachable 的 id 对应原始 UDP 包，
   不匹配 ping 的 ICMP id）。一个 API 服务两个 applet。
2. **monotonic deadline 循环**：`recv_icmp` 内 `now_us() + timeout*1000` 算 deadline，每轮 poll
   剩余时间。match_id 不匹配或 malformed 包 continue 不消耗整个 timeout（只减剩余）——
   ping 不会因收到一个无关 echo 就丢掉整个等待窗口。
3. **parse_icmp 按 IHL 剥头**：raw recv 含 IP 头，`raw[0] & 0x0f * 4` 算 IP 头长。
   测了 IHL=5（标准）和 IHL=6（含 IP 选项）两种。recv_ttl 取 IP 头偏移 8（标准 TTL 字节位置）。
4. **checksum 自洽性测试**：build_echo_request 后对整个包跑 checksum 应返回 0（ICMP 校验和的
   定义性质 —— 含 checksum 字段的整体求和为 0xffff，~= 0）。这比对照具体 RFC 向量更强 ——
   它验证了「我们填的 checksum 让接收端校验通过」。
5. **SIGINT RAII guard**：`sa.sa_flags=0`（不放 SA_RESTART）让 usleep/poll 被 SIGINT EINTR 唤醒，
   ping 快速响应 Ctrl-C 打 summary。析构 restore old_act —— multi-call binary 不污染后续 applet。
6. **EPERM exit 2 ≠ resolve 失败 exit 1**：用户能区分「没权限」与「host 不存在」。
7. **RTT 微秒精度**：`now_us()` us，显示 `rtt/1000.0`（3 位小数 ms）。本地 loopback RTT < 1ms，
   ms 整数精度会显示 0.0 ms 误导。
8. **`<span>` 引入**：ICMP 是字节流，`std::span<const std::uint8_t>` 比 pointer+len 更安全现代。
   C++23 标准库支持，项目首次用，编译过 armhf。

## 完成门

- `ctest`：**475/475** 全绿（+11 icmp 测）
- `bash tests/integration/run_all.sh`：全绿（ping 3/3）
- size-opt：**479 KB**（+8 KB，预算 550 KB，余 71 KB）
- armhf 交叉编译 + qemu：ping EPERM path 在 32 位正确报 "socket(SOCK_RAW, ICMP) failed: Operation not permitted"，无新 cast-align 警告（sockaddr_storage*→sockaddr* 对齐兼容，不触发）

## Gotcha

- **本机无 CAP_NET_RAW 不能真 ping**：开发机非 root，real ping smoke 报 EPERM exit 2（设计如此）。
  真验证依赖 qemu-system 阶段（CFBox 当 PID 1 有 root）。本机单测 + 32 位 EPERM 报错路径已覆盖可测部分。
- **checksum 奇数长度**：最后单字节作高位（`data[i] << 8`）。RFC 1071 规定如此。
- **carry 折叠**：用 `while (sum >> 16)` 循环（非一次折叠），处理极端情况（多次进位）。
  测 3×0xffff=0x2fffd → fold 0xffff → ~ = 0x0000。
- **`recvfrom` 缓冲 1500**：足够标准 MTU 的 IP+ICMP。jumbo frame 会截断，但 ping/traceroute
  探测包远小于 1500，不影响。
- **`sa.sa_flags=0` 的代价**：usleep 被 SIGINT 中断后不重睡 —— ping 在 reply 间隔按 Ctrl-C 立即退出。
  但 recv_icmp 内 poll EINTR 后 continue，若 SIGINT 到达在 recv 等待期，最坏延迟到 timeout。
  权衡接受（Ctrl-C 在 sleep 期立即响应；在 recv 期延迟 ≤ timeout）。
- **`build_echo_request` 返回 0 表 out 太小**：调用者应检查。ping.cpp 用固定 1500 缓冲，
  payload ≤ 1400，永不触发。
