# 2026-07-06 — Phase 3 批3：ip + route + hostname 深化（Wave 1b）

## 背景
Wave 1b：`ip addr show` + `route -n` + `hostname` 深化（`-i`/`-f`/`-d`）。三个都消费 `net_util.hpp`（read_interfaces / read_routes）或 `<netdb.h>`。Wave 1 显示路径基本完整；写操作 + netstat 留 1c。

## 设计决策

- **net_util 扩展 read_routes**：`/proc/net/route` 是 tab 分隔、地址为 host-endian hex。手写 tab-split + `hex_to_ipv4`（strtoul→uint32→直接赋 s_addr，little-endian 目标）。`format_route_table` BusyBox route -n 列布局（Destination/Gateway/Genmask/Flags/Metric/Ref/Use/Iface），flags 位→字母（U/G/H/R/D/M）。
- **prefix_len** 从 netmask 算（inet_pton → ntohl → 数高位连续 1）给 ip 的 `/24` 用。
- **ip addr show**：iproute2 风格（`N: name: <FLAGS> mtu N` + `link/encap` + `inet addr/prefix [brd] scope name`）。flags_angle 把 IFF_* 转 `UP,LOOPBACK,LOWER_UP,...`。本批只 `addr` 子树（`a` 缩写支持），link/route/add 后续。
- **route**：只显示（-n 是默认行为，cfbox 永远 numeric 不查 DNS）。add/del 显式拒绝（exit 2 + 报错）。
- **hostname -i/-f/-d**：`getaddrinfo` 解析本机名得 IPv4 列表（`-i`）/ canonical name（`-f`，`AI_CANONNAME`）；`-d` = FQDN 去短名部分。

## 踩坑

1. **`strtoul` 返回 `unsigned long`→`uint32_t` 截断**（IDE `-Wshorten-64-to-32` 抓到）：x86-64 `unsigned long` 64 位，赋 uint32 隐式截断。`hex_to_ipv4` 加 `static_cast<std::uint32_t>`。32 位 armhf 无此问题（unsigned long 32 位）但显式 cast 两端安全。
2. **getaddrinfo `ai_addr` cast-align**：`sockaddr*`→`sockaddr_in*` 同 [批2 ipv4_from_ioctl](2026-07-06-phase3-ifconfig.md) 的盲区。hostname 的 `resolve_ipv4` 用 `memcpy(&sa, p->ai_addr, sizeof(sa))` 而非 reinterpret_cast，对齐安全。**复用同一模式**。

## 验证
- GTest **452/452**（+9：NetUtil HexToIpv4RoundTrip/PrefixLen/FormatRouteTableHasHeader + IpTest AddrShowListsLoopback/AddrAbbrevWorks/NoSubcommandExits2）。
- 集成 **58 脚本**全绿（test_ip.sh + test_route.sh 新增）。
- structure gate PASS。
- size-opt **463 KB**（455 → +8 KB，hostname 深化 + ip + route + net_util route 扩展）。
- armhf static **1247 KB**（1243 → +4 KB）；qemu-arm-static `ip addr show lo` / `route -n` / `hostname -s` 输出正确。
- commit: `f0ff0db`

## 陷阱（留给后续批/维护者）
- **ip 只 addr 子树**：`ip link`/`ip route`/`ip addr add` 未实现（HELP 只列 addr show）。Wave 1c 或后续补。多文件拆分（文档建议 `src/applets/ip/`）目前单文件够用。
- **route 只读**：add/del（SIOCADDRT/SIOCDELRT）未实现，exit 2。需 root。
- **ifconfig 写操作仍未做**：Wave 1a 留的 gap，1c 补（SIOCSIFADDR/FLAGS，需 root）。
- **hostname -i 依赖 /etc/hosts 解析**：WSL 上返回 127.0.1.1（Debian/Ubuntu 习惯），板子上若 /etc/hosts 配置不同会不同。非 bug，行为同 BusyBox。
- **netstat 未做**：需 net_util `read_sockets`（/proc/net/tcp/udp 解析），Wave 1c。
- **cast-align/shorten 模式复用**：后续 netstat/wget/ping 遇 `sockaddr*`→`sockaddr_in*` 或长整型→uint32 一律 memcpy + static_cast，别直接 cast。
