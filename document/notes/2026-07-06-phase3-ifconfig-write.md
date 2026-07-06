# Phase 3 批5（Wave 1c）— ifconfig 写操作（SIOCSIF*）

> 2026-07-06。给 `ifconfig` 加写能力：`ifconfig IFACE ADDR [netmask NM] [broadcast BC]
> [mtu N] [up|down]`。`net_util` 加写 ioctl 基础设施（`ctl_socket` + 5 个 `set_*`）。
> 需 CAP_NET_ADMIN —— CI native/qemu-user 无权，靠 skip guard 验证「EPERM 不静默」，
> qemu-system（root）真跑 MTU round-trip。

## 触及文件

- [include/cfbox/net_util.hpp](../../include/cfbox/net_util.hpp) —
  - `ctl_socket() -> Result<unique_fd>`（公共控制套接字，读/写 ioctl 共用）
  - `parse_ipv4(sv) -> optional<in_addr>`（dotted → in_addr，纯函数可单测）
  - `detail::ifreq_with_addr(iface, cmd, addr)`（建 ifreq + memcpy 写 sockaddr_in，解 cast-align）
  - `detail::ioctl_error(what, errno)`（msg 含 strerror —— "SIOCSIFADDR failed: Operation not permitted"）
  - `set_ipv4_addr`/`set_netmask`/`set_broadcast`（SIOCSIFADDR/NETMASK/BRDADDR）
  - `set_mtu`（SIOCSIFMTU）
  - `set_if_up(ctl, iface, bool)`（SIOCGIFFLAGS 读改写 IFF_UP → SIOCSIFFLAGS，保留其他 flag）
- [src/applets/ifconfig.cpp](../../src/applets/ifconfig.cpp) — 加 `ifconfig_write` codepath（positional ≥2 触发），BusyBox 语法解析（`up`/`down`/`netmask`/`broadcast`/`mtu` keyword + 首个非 keyword 为 ADDR），一个 ctl_fd 上原子下发，EPERM 逐 ioctl 报错
- [tests/unit/test_net_util.cpp](../../tests/unit/test_net_util.cpp) — +2 测 `parse_ipv4` valid/invalid
- [tests/integration/test_ifconfig.sh](../../tests/integration/test_ifconfig.sh) — +1 写测试双分支（非 root grep EPERM / root MTU round-trip + 恢复）

## 关键设计决策

1. **写 path 触发条件 `positional().size() >= 2`**：`ifconfig lo`（1 个）仍走读 path 显示；
   `ifconfig lo up`（2 个）走写 path。保持读语义不退化。
2. **`detail::ifreq_with_addr` memcpy 模式**：与 `ipv4_from_ioctl` 对称 —— `sockaddr_in` 填好后
   memcpy 进 `ifr.ifr_addr`，避免 `reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr)` 在 armhf 触发
   `-Wcast-align`（memory 警示的 B2 真 bug 模式）。
3. **`detail::ioctl_error` 含 strerror**：既有 `socket.hpp` 错误不含 strerror（一致性弱点）。
   写 ioctl 是用户高频诊断点（"为什么 ifconfig 失败"），msg 拼 strerror 让 EPERM/ENODEV 可区分。
   读 path 无错误可拼，不影响一致性。
4. **`set_if_up` read-modify-write**：不能直接 `ifr.ifr_flags = IFF_UP`（会清掉 BROADCAST/MULTICAST
   等其他 flag）。先 SIOCGIFFLAGS 读现状，只改 IFF_UP 位，再 SIOCSIFFLAGS 写回。
5. **EPERM 不静默**：applet 逐 ioctl 检查 Result，失败即 `CFBOX_ERR` + return 1。绝不会
   「ioctl 失败但 ifconfig 返回 0」误导用户。
6. **集成测试双分支**：非 root 验证 EPERM 报错（grep "Operation not permitted"），
   root 验证 MTU round-trip（lo 默认 MTU 改 1280 再恢复 —— lo 选作目标因它安全可逆、
   不会丢连接）。改 lo 地址太危险（覆盖 127.0.0.1），故 root 测试只动 MTU。

## 完成门

- `ctest`：**464/464** 全绿（+2 parse_ipv4 测）
- `bash tests/integration/run_all.sh`：全绿（ifconfig 5/5，含写测试）
- size-opt：**471 KB**（+4 KB，预算 550 KB，余 79 KB）
- armhf 交叉编译 + qemu：ifconfig 写 path 在 32 位正确报 "SIOCSIFADDR failed: Operation not permitted"，无新 cast-align 警告

## Gotcha

- **`ifr.ifr_flags` 是 `short`**：`IFF_UP` 是 int，`ifr.ifr_flags |= IFF_UP` 隐式窄化触发
  armhf `-Wconversion`。用 `static_cast<short>(IFF_UP)` 显式窄化。
- **alias 接口 `lo:0`**：`ifr.ifr_name="lo:0"` 对 SIOCSIFADDR 可工作（Linux alias 语义），
  但 `read_interfaces` 读 /proc/net/dev 不显示 alias（只在 ip addr show）。本轮不暴露 alias 入口。
- **broadcast 通常自动算**：设了 ADDR+NETMASK 后 kernel 自动推 BCAST，显式 `broadcast BC`
  仅覆盖。BusyBox 行为一致。
- **测试改 lo 的安全性**：lo 整段 127.0.0.0/8 都通（kernel 特殊处理），改 lo 的 MTU 不会断
  loopback 连接；但改 lo 的主地址会丢 127.0.0.1，故 root 测试只动 MTU。
