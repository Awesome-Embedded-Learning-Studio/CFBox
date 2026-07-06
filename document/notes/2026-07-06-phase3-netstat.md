# Phase 3 批4（Wave 1c）— netstat 只读 + split_fields 抽取

> 2026-07-06。`netstat` applet（`-t/-u/-x/-a/-n`）+ `net_util` socket 解析基础设施。全只读，
> CI native 全程可真测，风险最低 —— 作为 Wave 1c/2 四批里的开胃菜，顺手把 `read_routes` 的
> 内联 tab-splitter 抽成公共 helper。

## 触及文件

- [include/cfbox/net_util.hpp](../../include/cfbox/net_util.hpp) —
  - 抽 `split_fields(string_view) -> vector<string>`（公共 tab/space splitter，`read_routes` 改用）
  - 新增 `SocketEntry` / `UnixSocketEntry` 结构
  - `tcp_state_name`/`unix_type_name`/`unix_state_name`（state code → BusyBox 名，返回 `const char*`）
  - `parse_ipv4_port`（`hexIP:hexPort` → `{dotted, port}`，复用 `hex_to_ipv4`）
  - `parse_inet_line`/`parse_inet_sockets`/`parse_unix_line`/`parse_unix_sockets`（**纯函数**，接收 content）
  - `read_tcp_sockets`/`read_udp_sockets`/`read_unix_sockets`（薄包装，读 /proc/net/*）
  - `format_netstat_inet(servers)`/`format_netstat_unix`（BusyBox 风格；LISTEN 默认隐藏、`-a` 显示、remote port 0 → `*`）
- [src/applets/netstat.cpp](../../src/applets/netstat.cpp) — 新 applet（`-t/-u/-x/-a/-n`，无 proto flag 默认 tcp+udp）
- 注册三件套：[cmake/Config.cmake](../../cmake/Config.cmake) `CFBOX_APPLETS` 加 `netstat` + [applet_config.hpp.in](../../include/cfbox/applet_config.hpp.in) `CFBOX_ENABLE_NETSTAT` + [applets.hpp](../../include/cfbox/applets.hpp) extern + registry
- [tests/unit/test_netstat.cpp](../../tests/unit/test_netstat.cpp) — 10 测（split_fields 3 + tcp_state 1 + parse_inet_line 2 + parse_inet_sockets 1 + format_netstat_inet 1 + parse_unix_line 1 + format_netstat_unix 1）
- [tests/integration/test_netstat.sh](../../tests/integration/test_netstat.sh) — 3 测（默认/-a/-x 表头）

## 关键设计决策

1. **纯函数 + 薄包装分离**：`parse_inet_sockets(content, proto)` 接收字符串（可喂假数据单测），
   `read_tcp_sockets()` 只读 `/proc/net/tcp` 再调纯函数。这是既有 `read_interfaces`/`read_routes`
       未做的改造点 —— 直接读 /proc 无法单测解析逻辑。本批为 socket 解析树立了可测模式，
       后续 ifconfig 写、icmp 解析可参照。
2. **`split_fields` DRY**：`read_routes` 原内联 14 行 tab-splitter，抽成公共 helper 后
   `read_routes` + `parse_inet_line` + `parse_unix_line` 三处复用。验证不退化（route 单测仍绿）。
3. **`parse_inet_line` 拒绝非数据行**：检查地址字段含 `:`，挡住 header 行（`local_address` 无冒号）
   和 malformed 行 —— 比单纯字段数检查鲁棒。
4. **UDP 无 State**：`format_netstat_inet` 对 `proto=="udp"` 打印空 State 列（BusyBox 行为）。
5. **state name 返回 `const char*`**：字面量，避免 `string_view::data()` 的 NUL 歧义，直接喂 `%s`。

## 完成门

- `ctest`：**462/462** 全绿（+10 测）
- `bash tests/integration/run_all.sh`：全绿（+3 netstat 测）
- size-opt：**467 KB**（+4 KB，预算 550 KB，余 83 KB）
- armhf 交叉编译 + qemu `--list`：netstat 注册成功，无新 cast-align 警告（本批纯字符串解析，未碰 sockaddr）

## Gotcha

- /proc/net/tcp 地址是 **host-endian hex**（`0100007F` = 127.0.0.1 on little-endian），复用 `hex_to_ipv4`
  的「直接赋给 `in_addr.s_addr`」模式 —— 但这只在 little-endian 目标对（x86/armhf 都是小端）。
  若未来上 big-endian 目标需重审。
- /proc/net/tcp 的 `tx_queue:rx_queue` 是**单字段**（冒号分隔），不是两个空格字段；`f[4]` 整体，
  再二次 split 冒号。Recv-Q = rx（冒号后），Send-Q = tx（冒号前）—— 顺序与 BusyBox 表头一致。
- `format_netstat_inet` 的 `%s` 喂 `std::string{tcp_state_name(...)}.c_str()` 会构造临时 string ——
  改成返回 `const char*` 后直接喂，零临时。
