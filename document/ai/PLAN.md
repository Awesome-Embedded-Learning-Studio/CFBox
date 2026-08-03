# CFBox — 当前焦点（批级进度）

> Tier 3（批级，易变）。单一事实源（批级）。全树见 [ROADMAP.md](ROADMAP.md)，铁律见 [DIRECTIVES.md](DIRECTIVES.md)；结构标尺见 [STRUCTURE-TASTE.md](STRUCTURE-TASTE.md)，性能标尺见 [PERFORMANCE.md](PERFORMANCE.md)。
> **v0.3.0 已发布**：L2 rootfs 启动骨架（init/mount/mdev/umount/swapoff/reboot/poweroff，117→123 applet）+ tail -f —— cfbox 在 i.MX6ULL 上作为 PID 1 替代 BusyBox。
> ✅ **Phase 2 全部完成**（cp/test/ls/grep/find + sh 全收 8 项）+ ✅ **结构与性能标尺横切批**（PR#17/#18）—— STRUCTURE-TASTE + banned-pattern/layering gate、PERFORMANCE + io/tar/cmp/md5sum/sed 流式化 + google-benchmark 脚手架。当前基线 **436 GTest / 439 KB size-opt / 123 applet**。
> ✅ **Phase 3 网络最小闭环完成**：批1（`socket.hpp` + `nc`）✅、批2（`net_util.hpp` + `ifconfig` 显示）✅、批3（`ip`/`route`/`hostname` 深化）✅、批4（`netstat` 只读 + `split_fields` 抽取）✅、批5（`ifconfig` 写操作 SIOCSIF*）✅、批6（`icmp.hpp` + `ping`）✅、批7（`traceroute` UDP 探测 + ICMP 收包）✅ —— 基线 **479 GTest / 479 KB / 130 applet**。详见 [phase-2-network.md](../todo/phases/phase-2-network.md)。
> 🔄 **Phase 4（生产质量门禁）已起步**：批1（POSIX 符合度覆盖率标尺 `tests/posix/coverage.sh` + CI only-up gate，baseline 71/97 → 82/97）✅、批2（`dd` applet + `sh` 10 POSIX builtins + 差分测试 harness cfbox-vs-busybox，修 wc/cut）✅ —— **当前基线 489 GTest / 495 KB size-opt / 131 applet / POSIX 84%**。剩余轨道见下 Phase 4 表。详见 [phase-3-quality.md](../todo/phases/phase-3-quality.md)。
> 状态：✅ DONE / 🔄 NEXT / ⏳ PENDING / ⛔ BLOCKED。每批≈一 commit，完成门 `cmake --build build -j$(nproc) && ctest --test-dir build --output-on-failure` 全绿 + `bash tests/integration/run_all.sh`。

## ✅ Phase 1.5（代码质量审查）已完成 — 2026-05-26

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| A-G 扫描 | 七维度质量审查（架构/体积/安全/鲁棒性/工具链/测试/注释） | ✅ | (见 changelogs/v0.2.0.md) | 379/0 |
| 收尾 | 体积 -14%、消 iostream/stoi、统一错误宏、fs 封装扩展 | ✅ | a96ec85(merge) | 379/0 |

## 🔄 Phase 2（核心命令深化）— 进行中

> 目标：让高频核心命令的功能深度到位（场景闭环优先于 applet 数量）。每批≈一 commit + 完成门。批级 propose/脚手架用 `/next`，验证收尾用 `/done`。

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| 批1 | `tail -f/-F`（fd-based follow：fstat 轮询 + 64KiB quantum + -F drain-switch + SIGINT 退出 0） | ✅ | bff34e9 | 381/0 |
| 批2 | `cp -a`（归档模式：保权限/属主/时间戳/symlink/递归） | ✅ | a3b89ed | 406/0 |
| 批3 | `test` POSIX 子集（文件测试/字符串/整数/复合表达式，退出码语义） | ✅ | 0f9b3cb | 417/0 |
| 批4 | `ls -R` 递归 + `--color`（LS_COLORS 感知、递归缩进） | ✅ | 6dfe329 | 424/0 |
| 批5a | `grep -A/-B/-C` 上下文（ring 向前 + after_pending 向后 + 组间 `--`） | ✅ | b6920c3 | 431/0 |
| 批5b | `find` 布尔表达式（AST + -a/-o/-not/!/括号 + 递归下降） | ✅ | 3e29feb | 436/0 |
| 批5c | `sh` 深化（算术/case/函数+return+local/here-doc/高级`${}`/break N/read/trap 全收） | ✅ | 46c3657 | 436/57 |

> 各批细节（触及文件、Result 签名草案、完成门、gotcha）由 `/next <批>` 现场产出脚手架，确认后写入本表 commit/测试列。

## ✅ L2 rootfs 启动骨架（v0.3.0，插队完成）— 已合并 PR #13（2026-06）

> 场景驱动插队（非 Phase 2 原计划）：让 cfbox 在 i.MX6ULL 上替代 BusyBox 当 PID 1，补齐启动闭环全部 applet 缺口。端到端验证：cfbox 当 PID 1 跑 imx-forge rootfs，启动到 console。

| 批 | 范围 | 状态 | Commit |
|----|------|------|--------|
| init askfirst | `init` 支持 `askfirst`（console getty 等回车激活） | ✅ | 4b95168 |
| mount | `mount -a/-t/-o`，读 fstab | ✅ | 65e1f17 |
| mdev | `mdev -s` 冷启动扫 /sys 建 /dev 节点 | ✅ | 6ee0770 |
| 关机集 | `umount -a -r` / `swapoff -a` / `reboot` / `poweroff` | ✅ | de78018 |
| 串口修复 | sh cooked tty + `VERASE`、`ls -l` owner/group、default `PATH` | ✅ | 8045c11 / a803b99 |
| CI 兼容 | gcc-13（unused-result + 显式 `<algorithm>`） | ✅ | d307998 / dc25a09 |
| 展示 | README i.MX6ULL 端到端 + size-table 生成器 | ✅ | 8c45c77 |

> 新增 6 applet（mount/mdev/umount/swapoff/reboot/poweroff），117→123。完整故事见 [changelogs/v0.3.0.md](../../changelogs/v0.3.0.md)。

## ✅ 结构与性能标尺（横切批）— 已合并 PR#17 / PR#18（2026-07）

> Phase 2 收尾后的两轮横切改进，建立两条季级标尺并落地首波优化。非 ROADMAP 某个 Phase，是跨 Phase 的工艺/性能基线（对标 Phase 1.5 的代码质量审查，但聚焦结构与性能维度）。

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| 结构标尺（PR#17 `feat/test-floor-gates`） | [STRUCTURE-TASTE.md](STRUCTURE-TASTE.md) 季级标尺（职责/DRY/边界/机械护栏）+ 清 banned-pattern + 合并 helper + [tests/check_structure_gates.sh](../../tests/check_structure_gates.sh) banned-pattern/layering gate（CI 守护） | ✅ | a1c1028 / 1ef38be / 623fca7（merge fefcbc2） | 436/0（gate 独立于 GTest，CI 跑） |
| 性能基线（PR#18 `feat/performance`） | [PERFORMANCE.md](PERFORMANCE.md) 季级标尺（wall-clock 不动输出/4 步闭环）+ google-benchmark harness + io/tar/cmp/md5sum/sed 流式化（line reader ~7x、tar O(1) 内存、sed 预编译 ~4x、cmp 早退）+ end-to-end timing 脚本 + armhf `-Wconversion`/charconv 修 | ✅ | e229f05 … 4f154e9（merge f979b8f） | 436/0（benchmark 独立于 GTest） |

> 批级记录见 [notes/2026-07-06-structure-performance.md](../notes/2026-07-06-structure-performance.md)。两批均未增删 applet，GTest 基线沿用 Phase 2 末 436；size-opt 体积 v0.3.0 的 418 KB → 439 KB（+21 KB，主因 io/tar 流式缓冲与 benchmark 链接产物，仍在 ≤ 550 KB 预算内）。

## ✅ Phase 3（网络最小闭环）— 已完成

> 目标：socket/http/net_util/icmp 基础设施 + ip/ifconfig/route/netstat/ping/traceroute/nslookup/wget/nc/tftp + hostname 深化（11 applet）。详见 [phase-2-network.md](../todo/phases/phase-2-network.md)。每批≈一 commit + 完成门。

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| 批1（Wave 0） | `include/cfbox/socket.hpp` 基础设施（复用 `io::unique_fd`：make/resolve/dial/listen_on/accept_one/format_addr，双栈、header-only）+ `nc` applet（connect/listen 模式 + poll 双向 relay stdin→sock/sock→stdout + SHUT_WR 半关） | ✅ | 10f811f | 440/1 |
| 批2（Wave 1a） | `include/cfbox/net_util.hpp` 基础设施（`read_interfaces` 解析 /proc/net/dev + ioctl SIOCGIF* 取 flags/mtu/hwaddr/ipv4；`format_ifconfig` BusyBox 多行格式；`ipv4_from_ioctl` memcpy 解 cast-align）+ `ifconfig` applet（`-a`/`IFACE` 只读显示） | ✅ | f4279d6 | 446/1 |
| 批3（Wave 1b） | `net_util.hpp` 扩展（`read_routes` 解析 /proc/net/route tab-hex + `format_route_table` BusyBox route -n + `hex_to_ipv4`/`prefix_len`）+ `ip addr show`（iproute2 风格 index/flags/mtu/link/inet）+ `route -n`（显示）+ `hostname` 深化（`-i`/`-f`/`-d` getaddrinfo + memcpy 解 cast-align） | ✅ | f0ff0db | 452/2 |
| 批4（Wave 1c） | `net_util.hpp` 抽公共 `split_fields`（`read_routes` 改用，DRY）+ `read_tcp/udp_sockets`（解析 /proc/net/tcp\|udp `hexIP:hexPort` + state code + tx:rx queues）+ `read_unix_sockets`（/proc/net/unix）+ `parse_inet_sockets` 纯函数化（喂假数据可单测）+ `format_netstat_inet/unix`（BusyBox 风格，LISTEN 过滤/-a）+ `netstat` applet（`-t/-u/-x/-a/-n`） | ✅ | (本批) | 462/3 |
| 批5（Wave 1c） | `net_util.hpp` 写 ioctl：抽 `ctl_socket`/`parse_ipv4`/`detail::ifreq_with_addr`（memcpy 写 sockaddr 解 cast-align）/`detail::ioctl_error`（msg 含 strerror）+ `set_ipv4_addr`/`set_netmask`/`set_broadcast`/`set_mtu`/`set_if_up`（read-modify-write IFF_UP）+ `ifconfig` 写 codepath（`IFACE ADDR [netmask NM] [broadcast BC] [mtu N] [up\|down]`，EPERM 不静默） | ✅ | (本批) | 464/5 |
| 批6（Wave 2） | `include/cfbox/icmp.hpp` 新基础设施（`checksum` RFC1071 纯函数 + `build_echo_request` + `parse_icmp` 剥 IP 头按 IHL + `open_raw` SOCK_RAW IPPROTO_ICMP + `recv_icmp` match_id 过滤/monotonic deadline + `now_us`）+ `ping` applet（`-c/-i/-W/-s/-q/-n`，SIGINT RAII 打 summary，EPERM exit 2 区分 resolve 失败 exit 1） | ✅ | (本批) | 475/3 |
| 批7（Wave 2） | `traceroute` applet（UDP 探测 `setsockopt(IP_TTL)` 递增 + 每探 bump 目标端口避 conntrack + 复用 `icmp::open_raw`/`recv_icmp`(match_id=0) 收包 + `icmp::classify_reply` 判 time-exceeded/port-unreach）+ `net_util` 抽 `resolve_ipv4`/`name_of`（ping/traceroute 共享，ping.cpp 重构去重） | ✅ | (本批) | 479/3 |

> **Phase 3 收口**：11 applet 全到位（socket/nc/ifconfig/ip/route/hostname/netstat + ifconfig 写 + ping/traceroute），3 基础设施（socket.hpp/net_util.hpp/icmp.hpp）。下一焦点待定（候选：route add/del、hostname NAME、netstat -r/-i、IPv6、或转 Phase 4 用户/文件权限）。批级记录见 [notes/2026-07-06-phase3-socket-nc.md](../notes/2026-07-06-phase3-socket-nc.md) / [notes/2026-07-06-phase3-ifconfig.md](../notes/2026-07-06-phase3-ifconfig.md) / [notes/2026-07-06-phase3-ip-route-hostname.md](../notes/2026-07-06-phase3-ip-route-hostname.md) / [notes/2026-07-06-phase3-netstat.md](../notes/2026-07-06-phase3-netstat.md) / [notes/2026-07-06-phase3-ifconfig-write.md](../notes/2026-07-06-phase3-ifconfig-write.md) / [notes/2026-07-06-phase3-icmp-ping.md](../notes/2026-07-06-phase3-icmp-ping.md) / [notes/2026-07-06-phase3-traceroute.md](../notes/2026-07-06-phase3-traceroute.md)。

## 🔄 Phase 4（生产质量门禁）— 进行中

> 目标：从「核心场景功能完整」推进到「可发布、可回归、可审计」—— 差异测试 / fuzzing / benchmark / 静态分析 / 替换测试 / 覆盖率 / 发布工程（Part 1-7 详见 [phase-3-quality.md](../todo/phases/phase-3-quality.md)）。每批≈一 commit + 完成门。批级记录见 [notes/2026-07-06-posix-coverage.md](../notes/2026-07-06-posix-coverage.md)（批1）。

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| 批1（PR#21 `feat/posix-coverage`） | `tests/posix/coverage.sh` 双维度标尺（utility 静态 vs POSIX.1-2017 XCU 77 个 / builtin 动态行为探针 vs POSIX 20 个）+ `tests/posix/baseline` 只升不降 floor + CI gate（native 阶段，structure gates 后）；baseline 锁 71/97（utility 61 / builtin 10） | ✅ | 58f2f6a（merge 7105bae） | 479/0（POSIX gate 独立于 GTest） |
| 批2（PR#22 `feat/phase4-sweep`） | **a** `dd` applet（bs/ibs/obs/count/skip/seek/conv/status）+ armhf `-Wconversion` 修（ibs/obs 用 size_t）｜**b** `sh` 10 POSIX mandatory builtins（type/command/exec/getopts/hash/umask/ulimit/wait/times/unalias，builtin 50%→100%）｜**c** `tests/differential/` 差分 harness（cfbox vs busybox）+ 修 wc/cut 行为差；baseline 升 82/97（utility 62 / builtin 20） | ✅ | 7cd45d4 / 277a093 / e5d4fcf / 31a916a（merge 246b80e） | 489/0（+10：dd + sh builtins 测试） |

> **POSIX 覆盖现状**：82/97（84%）—— utility 62/77（80%，缺 15：csplit/file/getconf/locale/logger/pathchk/pr/strings/strip/stty/tput/unexpand/uudecode/uuencode/what）｜builtin 20/20（100%）。
> **Phase 4 剩余轨道**：①Part 1.2 P0 差异测试扩面（现仅 wc/cut，待覆盖 sh/grep/sed/awk/sort/tar/gzip/…）②Part 2 fuzzing（libFuzzer + 11 target：tar/cpio/ar/unzip/sh_parser/awk_parser/sed_parser/find_expr/patch…）③Part 4 静态分析（clang-tidy 低噪声规则 / cppcheck）④Part 5 替换测试（Alpine minirootfs / container profile；initramfs PID1 已在 v0.3.0）⑤Part 6 行覆盖率（gcov/lcov + 缺口填补；POSIX coverage 是行为覆盖，非行覆盖）⑥Part 7 发布工程（`scripts/release/build_release.sh` + git-cliff changelog + v0.4.0 RC）⑦utility 长尾（15 个 POSIX missing，stty/strings/file/logger 高频先排）。

## OPEN GOTCHAS（跨批陷阱，改前必看）

1. **APPLET_REGISTRY + CMake 开关双改**：新增/裁剪 applet 必须**同时**改 [include/cfbox/applets.hpp](../../include/cfbox/applets.hpp) 注册表与 CMake 的 `CFBOX_ENABLE_*`（生成于 [applet_config.hpp.in](../../include/cfbox/applet_config.hpp.in)）；漏一处 → 编译期静默跳过或链接错。
2. **交叉编译盲区**：本地 `build/`（x86-64 + ASan）抓不到 aarch64/armhf；改公共头/分发逻辑/ABI 后，push 前留意 CI 的 cross-compile + qemu-user/system 阶段是否绿（[document/ci.md](../ci.md)）。
3. **体积回归**：新增 `<filesystem>`/iostream/include 膨胀会撑大 size-opt 二进制（预算 ≤ 550 KB，当前 418 KB）；批量改动后跑 `cmake -B build-size -DCMAKE_BUILD_TYPE=Release -DCFBOX_OPTIMIZE_FOR_SIZE=ON && cmake --build build-size -j$(nproc)` + strip 核查。
4. **TOCTOU / symlink**：cp/rm/mv/chmod 的 check-then-act 与 `-R` 跟随 symlink 是安全高危（质量扫描 C 维度）；改这些 applet 先看既有防御。
5. **流式 vs 全量**：大文件优先 `cfbox::io::for_each_line()`；滥用 `read_all()` 会内存爆炸（grep/cat/wc 已流式化，参考）。
6. **multi-call binary 全局状态**：装 signal handler / 改进程全局状态的 applet 必须 RAII 恢复（如 tail -f 的 sigaction guard），否则污染同进程后续 applet 调用。
7. **底层 fd 操作不走 fs_util**：follow/字节流消费等需 `fstat`/`lseek`/`read` 的场景用 `<sys/stat.h>` raw POSIX（fs_util::status 拉 `std::filesystem` 撑体积）；公共 fs 封装是高层路径操作。

## 回到仓库
`/resume`（读本文件 + `git log --oneline -15`）。Codex 等价粘贴 prompt 见 [prompts.md](prompts.md)。
