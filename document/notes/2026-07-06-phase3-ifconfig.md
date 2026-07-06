# 2026-07-06 — Phase 3 批2：net_util.hpp + ifconfig 显示（Wave 1a）

## 背景
Phase 3 Wave 1 第一锹。`net_util.hpp` 是 Wave 1（ifconfig/ip/route/netstat）共享的接口查询层；`ifconfig -a` 是第一个 read-only 消费者，闭环可集成测（lo 必有）。

## 设计决策

- **net_util 复用 `io::read_all` + `io::split_lines`** 解 /proc/net/dev（对齐 proc.hpp 的 /proc 解析风格），手写 `parse_dev_fields` 解析 16 列计数器——避免 `<sstream>` 撑体积（[STRUCTURE-TASTE](../ai/STRUCTURE-TASTE.md) 体积敏感）。
- **ioctl control socket**：一个 `AF_INET/SOCK_DGRAM` fd（`io::unique_fd` 守护）跑所有 SIOCGIF* 查询（flags/mtu/hwaddr/addr/netmask/brdaddr）。
- **`format_ifconfig` BusyBox 多行格式**：name 列宽 10、Link encap/HWaddr、inet addr/Bcast/Mask、flags + MTU、RX/TX counters、bytes 人类可读（KB/MB/GB，1024 进制 BusyBox 标签）。
- **本批只做 read**：`net_util` 只实现 `read_interfaces` + `format_ifconfig`。文档 [phase-2-network.md](../todo/phases/phase-2-network.md) 1.3 设计的 `read_routes`/`read_sockets`/写操作（interface_up/set_addr/add_route）留 B3+。ifconfig 写操作（ADDR/up/down）同批或下批补。

## 踩坑

1. **`-Wcast-align` armhf 32 位盲区**（[memory local-cross-compile-selfcheck]）：`reinterpret_cast<sockaddr_in*>(&ifr.ifr_addr)` 在 x86-64 静默，armhf `-Werror=cast-align` 报——sockaddr（2 字节对齐）→ sockaddr_in（4 字节对齐）增加对齐。解：`ipv4_from_ioctl` helper 用 `memcpy` 拷到局部 sockaddr_in（无指针 cast，对齐安全）。**这是 memory 警告的 32 位盲区首次真命中**，证明本地 armhf 交叉核查的价值。注意 `sockaddr_storage*` → `sockaddr*`（bound_port/listen_on）方向对齐减小，不报。
2. **lo 显示 `Bcast:0.0.0.0`**：SIOCGIFBRDADDR 对 loopback 返回 0.0.0.0 而非空。BusyBox ifconfig 对 loopback 不显示 Bcast（语义：loopback 无广播）。`format_ifconfig` 加 `!it.loopback()` 条件。
3. **collisions 行重复 `Metric:1`**：初版写 `collisions:0  Metric:1`，BusyBox 是 `collisions:0  txqueuelen:1000`。改 `txqueuelen:1000`（默认值；完整需 SIOCGIFTXQLEN，后续批补）。

## 验证
- GTest **446/446**（+6：NetUtilTest ReadInterfacesHasLoopback/LoopbackHasLocalAddress/ParseDevFields/ParseDevFieldsEmpty/FormatIfconfigLoopback/FormatIfconfigEthernet）。
- 集成 **56 脚本**全绿（test_ifconfig.sh 新增：-a/no-arg/named/bogus-exit）。
- structure gate PASS。
- size-opt **455 KB**（基线 451 → +4 KB，ifconfig + net_util.hpp）。
- armhf static **1243 KB**（B1 后 1235 → +8 KB）；qemu-arm-static `ifconfig lo` 输出正确（32 位 ioctl + memcpy 解工作）。
- commit: `f4279d6`

## 陷阱（留给后续批/维护者）
- **只读**：`ifconfig ADDR up/down` 等写操作未实现（HELP 只列 `-a`，避免名实不符）。Wave 1b 补 SIOCSIF*（需 root/CAP_NET_ADMIN）。
- **net_util 未完**：`read_routes`/`read_sockets`（route/netstat 要）未实现，下批按需加。
- **cast-align 教训**：网络代码里 `sockaddr*` → `sockaddr_in*` 的 reinterpret_cast 一律走 memcpy helper，别直接 cast。后续 ip/route/netstat/wget 若遇同模式，复用 `ipv4_from_ioctl` 或同理。
- **Metric:1 / txqueuelen:1000 是写死默认**：真实值需额外 ioctl（SIOCGIFMETRIC/SIOCGIFTXQLEN），下批可补，但对显示兼容非阻塞。
