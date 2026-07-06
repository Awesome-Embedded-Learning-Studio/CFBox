# CFBox — 当前焦点（批级进度）

> Tier 3（批级，易变）。单一事实源（批级）。全树见 [ROADMAP.md](ROADMAP.md)，铁律见 [DIRECTIVES.md](DIRECTIVES.md)；结构标尺见 [STRUCTURE-TASTE.md](STRUCTURE-TASTE.md)，性能标尺见 [PERFORMANCE.md](PERFORMANCE.md)。
> **v0.3.0 已发布**：L2 rootfs 启动骨架（init/mount/mdev/umount/swapoff/reboot/poweroff，117→123 applet）+ tail -f —— cfbox 在 i.MX6ULL 上作为 PID 1 替代 BusyBox。
> ✅ **Phase 2 全部完成**（cp/test/ls/grep/find + sh 全收 8 项）+ ✅ **结构与性能标尺横切批**（PR#17/#18）—— STRUCTURE-TASTE + banned-pattern/layering gate、PERFORMANCE + io/tar/cmp/md5sum/sed 流式化 + google-benchmark 脚手架。当前基线 **436 GTest / 439 KB size-opt / 123 applet**。
> 🔄 **Phase 3 网络最小闭环进行中**：批1（`socket.hpp` + `nc`）✅、批2（`net_util.hpp` + `ifconfig` 显示）✅、批3（`ip`/`route`/`hostname` 深化）✅ —— 当前基线 **452 GTest / 463 KB / 127 applet**。详见 [phase-2-network.md](../todo/phases/phase-2-network.md)。
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

## 🔄 Phase 3（网络最小闭环）— 进行中

> 目标：socket/http/net_util/icmp 基础设施 + ip/ifconfig/route/netstat/ping/traceroute/nslookup/wget/nc/tftp + hostname 深化（11 applet）。详见 [phase-2-network.md](../todo/phases/phase-2-network.md)。每批≈一 commit + 完成门。

| 批 | 范围 | 状态 | Commit | 测试 |
|----|------|------|--------|------|
| 批1（Wave 0） | `include/cfbox/socket.hpp` 基础设施（复用 `io::unique_fd`：make/resolve/dial/listen_on/accept_one/format_addr，双栈、header-only）+ `nc` applet（connect/listen 模式 + poll 双向 relay stdin→sock/sock→stdout + SHUT_WR 半关） | ✅ | 10f811f | 440/1 |
| 批2（Wave 1a） | `include/cfbox/net_util.hpp` 基础设施（`read_interfaces` 解析 /proc/net/dev + ioctl SIOCGIF* 取 flags/mtu/hwaddr/ipv4；`format_ifconfig` BusyBox 多行格式；`ipv4_from_ioctl` memcpy 解 cast-align）+ `ifconfig` applet（`-a`/`IFACE` 只读显示） | ✅ | f4279d6 | 446/1 |
| 批3（Wave 1b） | `net_util.hpp` 扩展（`read_routes` 解析 /proc/net/route tab-hex + `format_route_table` BusyBox route -n + `hex_to_ipv4`/`prefix_len`）+ `ip addr show`（iproute2 风格 index/flags/mtu/link/inet）+ `route -n`（显示）+ `hostname` 深化（`-i`/`-f`/`-d` getaddrinfo + memcpy 解 cast-align） | ✅ | f0ff0db | 452/2 |

> 下一批：Wave 1c `ifconfig` 写操作（ADDR/netmask/up/down/mtu，SIOCSIF*，需 root）+ `netstat`（依赖 net_util `read_sockets` 待建）；或转 Wave 2（`ping`/`traceroute`，需 `icmp.hpp` raw socket）。批级记录见 [notes/2026-07-06-phase3-socket-nc.md](../notes/2026-07-06-phase3-socket-nc.md) / [notes/2026-07-06-phase3-ifconfig.md](../notes/2026-07-06-phase3-ifconfig.md) / [notes/2026-07-06-phase3-ip-route-hostname.md](../notes/2026-07-06-phase3-ip-route-hostname.md)。

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
