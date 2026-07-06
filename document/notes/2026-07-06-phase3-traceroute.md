# Phase 3 批7（Wave 2）— traceroute + net_util resolve/name 抽取

> 2026-07-06。`traceroute` applet（UDP 高端口探测 + 递增 TTL + 复用 `icmp.hpp` 收
> time-exceeded/port-unreachable）。Phase 3 网络最小闭环收口批。同时把 `resolve_ipv4`/
> `name_of` 从 ping.cpp 抽到 `net_util.hpp`（ping/traceroute 共享，DRY），并加
> `icmp::classify_reply` 纯函数让 traceroute 的 ICMP 判定可单测。

## 触及文件

- [include/cfbox/net_util.hpp](../../include/cfbox/net_util.hpp) — 加 `<netdb.h>` + `resolve_ipv4(sv, sockaddr_storage&) -> bool`（AF_INET，getaddrinfo）+ `name_of(ss, numeric) -> string`（getnameinfo，-n/反查）
- [include/cfbox/icmp.hpp](../../include/cfbox/icmp.hpp) — 加 `enum class reply_class` + `classify_reply(type, code)`（time_exceeded→intermediate / dest_unreach+port→reached / 其他→other）
- [src/applets/traceroute.cpp](../../src/applets/traceroute.cpp) — 新 applet（`-m/-q/-w/-n`，UDP socket + setsockopt IP_TTL 递增 + 每探 bump 目标端口 + recv_icmp match_id=0 收任意 ICMP + classify_reply 判到达 + 本地 sigint_guard）
- [src/applets/ping.cpp](../../src/applets/ping.cpp) — 重构：删本地 resolve_ipv4/name_of，用 `net::` 版（去重）
- 注册三件套（`CFBOX_ENABLE_TRACEROUTE`）
- [tests/unit/test_icmp.cpp](../../tests/unit/test_icmp.cpp) — +4 测 classify_reply（time_exceeded/port_unreach/其他 dest_unreach/echo_reply）
- [tests/integration/test_traceroute.sh](../../tests/integration/test_traceroute.sh) — 3 测（EPERM 或 banner + 无参 exit 非零 + 坏 -m exit 非零）

## 关键设计决策

1. **UDP 探测 + ICMP 收包**（BusyBox 默认）：UDP socket 发探测（递增 TTL + 递增目标端口），
   SOCK_RAW ICMP socket 收 time-exceeded（中间路由器 TTL=0 丢弃）或 port-unreachable
   （到达目的，UDP 高端口无监听）。两条 socket，ICMP 那条需 CAP_NET_RAW。
2. **每探 bump 目标端口**（`33000 + ttl*100 + q`）：避免中间 NAT/conntrack 把同五元组
   折叠，确保每跳每探独立路由。
3. **`recv_icmp` match_id=0**：traceroute 收的 ICMP time-exceeded 内嵌原始 UDP 包的 id，
   不是 traceroute 的 ICMP id（traceroute 不发 ICMP echo）。match_id=0 收任意 ICMP ——
   ping 模式（match_id=id 过滤非己 echo）与 traceroute 模式（match_id=0 收全部）同一 API。
4. **`classify_reply` 抽纯函数**：traceroute 的 ICMP 判定逻辑（time_exceeded→中间跳 /
   port_unreach→到达）是核心业务逻辑，但 CI 无 CAP_NET_RAW 真跑不了 traceroute。
   抽成纯函数 + 4 单测是 CI 唯一覆盖该判定的方式。
5. **`resolve_ipv4`/`name_of` 抽到 net_util**：ping/traceroute 都要 host→sockaddr + 反查。
   抽到 net_util 后 ping.cpp 删本地副本（-30 行），traceroute 直接用。职责归位（IP 地址解析属 net_util）。
6. **`sigint_guard` 保留各自**：ping/traceroute 各一份 sigint_guard（~15 行）。signal 处理
   是 applet 行为，非共享网络基建；若第三个 applet 需要再抽 `signal_guard.hpp`。traceroute.cpp
   注释里标了 lift 机会。
7. **EPERM exit 2 ≠ resolve 失败 exit 1 ≠ 未到达 exit 1**：与 ping 一致（特权/resolve 区分）；
   traceroute 走完 maxttl 未到达也 exit 1（非零）。

## 完成门

- `ctest`：**479/479** 全绿（+4 classify_reply 测）
- `bash tests/integration/run_all.sh`：全绿（traceroute 3/3）
- size-opt：**479 KB**（+0 KB —— traceroute 增量被 ping.cpp 去重 + size-opt 内联抵消）
- armhf 交叉编译 + qemu：traceroute EPERM path 在 32 位正确报错，`--list` 列出全部 4 网络新 applet（ifconfig/netstat/ping/traceroute），无新 cast-align 警告

## Gotcha

- **本机无 CAP_NET_RAW 不能真 trace**：开发机非 root，traceroute 报 EPERM exit 2。
  真验证靠 qemu-system（root）。CI 单测覆盖 classify_reply 纯判定，集成覆盖 EPERM 路径。
- **`setsockopt(IP_TTL)` 是 int**：`int t = ttl; setsockopt(fd, IPPROTO_IP, IP_TTL, &t, sizeof(t))`
  —— int 对齐，无 cast-align 问题（不像 sockaddr*→sockaddr_in*）。
- **127.0.0.1 自环 traceroute**：第一跳 TTL=1 即到目的，收 port-unreachable，reached=true。
  但本机无 CAP 收不到，靠 qemu-system 验证。
- **`classify_reply` 把 echo_reply 归 other**：traceroute 不发 ICMP echo，理论上不会收到
  echo_reply；若收到（主机 ICMP 噪声）归 other 当作该跳响应，不误判到达。
- **hop_name 取首响应**：一跳多探，hop_name 取第一个响应的 from（BusyBox 行为）。
  全 `*` 跳 hop_name 显示 `*`。
