# Phase 3 网络最小闭环 — 收口总结

> 2026-07-06。Phase 3 全部 7 批完成，网络最小闭环到位。本文是高层回顾；
> 细节见各 wave note（socket-nc / ifconfig / ip-route-hostname / netstat /
> ifconfig-write / icmp-ping / traceroute）。

## 范围

**11 applet + 3 基础设施头**：

| 基础设施 | 提供能力 | 服务 applet |
|----------|----------|-------------|
| `include/cfbox/socket.hpp` | TCP/UDP socket（make/resolve/dial/listen/accept/format_addr） | nc |
| `include/cfbox/net_util.hpp` | 接口/路由/socket 表查询（/proc + ioctl）+ 写 ioctl + `resolve_ipv4`/`name_of` + `split_fields` | ifconfig, ip, route, netstat, ping, traceroute |
| `include/cfbox/icmp.hpp` | ICMP echo（checksum/build/parse）+ SOCK_RAW open + `recv_icmp`（match_id 双模式）+ `classify_reply` | ping, traceroute |

applet：`nc`（Wave 0）、`ifconfig`（读 Wave 1a + 写 Wave 1c）、`ip addr show`、`route -n`、`hostname -i/-f/-d`（Wave 1b）、`netstat`（Wave 1c）、`ping`、`traceroute`（Wave 2）。

## 7 批一览

| 批 | Wave | 范围 | 测试增量 |
|----|------|------|----------|
| 1 | 0 | socket.hpp + nc | +440→ 440/1 |
| 2 | 1a | net_util 读 + ifconfig 显示 | +6 |
| 3 | 1b | ip/route/hostname 深化 | +6 |
| 4 | 1c | split_fields + netstat | +10 |
| 5 | 1c | ifconfig 写（SIOCSIF*） | +2 |
| 6 | 2 | icmp.hpp + ping | +11 |
| 7 | 2 | traceroute + resolve/name 抽取 | +4 |

最终基线：**130 applet / 479 GTest / 479 KB** size-opt（起点 127/452/463）。

## 关键设计经验

1. **CI 测不真桥接**：网络代码 3/4 批需特权（CAP_NET_ADMIN/CAP_NET_RAW），CI native/qemu-user 跑不了真功能。三招组合：
   - 纯函数单测（checksum / parse_icmp / classify_reply / parse_inet_sockets / format_*）—— 解析/格式/判定逻辑剥离成纯函数喂假数据，CI 全覆盖。
   - 集成 skip guard（EPERM→exit 0，参照 test_nc.sh port-busy 跳过）—— 无特权阶段测「报错路径正确」。
   - qemu-system 阶段（CFBox 当 PID 1，唯一有 root 的 CI 阶段）真跑特权路径。
2. **`recv_icmp` match_id 双模式**：一个 API 服务 ping（match_id=id 过滤非己 echo）和 traceroute（match_id=0 收任意 ICMP，因为 UDP 探测的 time-exceeded 内嵌 UDP id 不是自己的）。避免两套收包逻辑。
3. **memcpy 解 cast-align（对称模式）**：`ipv4_from_ioctl`（读：sockaddr*→sockaddr_in memcpy）+ `detail::ifreq_with_addr`（写：sockaddr_in memcpy 进 ifr.ifr_addr）—— armhf 32 位 `-Wcast-align` 的标准解法。Wave 1c/2 四批本地 armhf 全过，零新 cast-align bug。
4. **可测性抽取**：`parse_inet_sockets(content)` 接收字符串而非读 /proc（可喂假数据单测）；`classify_reply` 把 traceroute 判定抽离成纯函数；`ctl_socket`/`split_fields`/`resolve_ipv4`/`name_of` 抽公共 helper（DRY，ping/traceroute/ifconfig 共享）。
5. **错误诊断友好**：`detail::ioctl_error` 把 strerror 拼进 msg（"SIOCSIFADDR failed: Operation not permitted"），用户能区分 EPERM/ENODEV。既有 `socket.hpp` 错误不含 strerror 的弱点在写 path 改善。

## 验证矩阵

每批完成门：`cmake --build` + `ctest`（479/479 全绿）+ `run_all.sh`（全绿）+ size-opt ≤ 550 KB + 本地 armhf 交叉编译 + qemu `--list`。

armhf 32 位核查全程无新 cast-align 警告（`/opt/arm-gnu-toolchain` + `qemu-arm-static`）。

## 下一站候选

Phase 3 增量：route add/del（SIOCADDRT/DELRT，同 ifconfig 写模式）、hostname NAME（sethostname）、netstat -r/-i（复用 read_routes/read_interfaces，几乎免费）、IPv6 ping/traceroute（AF_INET6）。

或转 Phase 4：fuzzing、benchmark 深化、POSIX 子集、release 工程。
