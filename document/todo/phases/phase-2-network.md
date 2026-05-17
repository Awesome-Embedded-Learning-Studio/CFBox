# Phase 2: 网络最小闭环

## 概述

**目标**：提供足够的网络能力，使 CFBox 在 rescue、container、embedded profile 中能独立完成网络配置、诊断和下载。网络栈覆盖：接口配置（`ip`/`ifconfig`/`route`）、状态查看（`netstat`）、连通性测试（`ping`/`traceroute`）、名称解析（`nslookup`）、文件传输（`wget`/`nc`/`tftp`）。

**进入条件**：Phase 1 完成。二进制体积 ≤ 550 KB。

**前置引用**：兼容性裁决原则见 [兼容性策略](../compatibility-policy.md)；全局阶段顺序和优先级见 [生产化路线图](../production-roadmap.md)。

**退出条件**：
- 11 个网络 applet 实现并通过测试
- HTTP 下载通过 `wget` 可用
- `ping` 在 QEMU user-mode 网络中可用
- `nc` 支持监听和连接模式
- 二进制体积保持在 650 KB 以内（size-opt）
- 所有网络 applet 有 GTest + 集成测试

**体积预算**：full profile static musl ≤ 650 KB（Phase 1 后 ~550 KB，预算新增 ~100 KB）

**原则**：Phase 2 不包含 TLS。HTTP-only 能力先进入；HTTPS 不阻塞网络最小闭环，但阻塞"现代下载工具完整可用"的更高等级声明。TLS 作为可选构建变体 `CFBOX_ENABLE_TLS=ON` 在后续阶段支持。

---

## Part 1: 新增基础设施

### 1.1 `include/cfbox/socket.hpp` — Socket RAII 框架

**动机**：每个网络 applet 都需要 socket 创建、连接、绑定、监听和清理。RAII 包装确保无 fd 泄露，通过 `std::expected` 统一错误处理，为 `nc`、`wget`、`tftp` 和 `netstat` 提供基础。

**接口设计**：

```cpp
namespace cfbox::socket {
    enum class Domain { IPv4, IPv6, Unix };
    enum class Type { Stream, Datagram, Raw };

    class SocketFd {
        int fd_ = -1;
    public:
        SocketFd() = default;
        explicit SocketFd(int fd);
        SocketFd(Domain d, Type t, int protocol = 0);
        ~SocketFd();  // 关闭 fd

        SocketFd(SocketFd&& o) noexcept;
        SocketFd& operator=(SocketFd&& o) noexcept;
        SocketFd(const SocketFd&) = delete;

        auto connect(const std::string& host, uint16_t port) -> base::Result<void>;
        auto bind(const std::string& addr, uint16_t port) -> base::Result<void>;
        auto listen(int backlog = SOMAXCONN) -> base::Result<void>;
        auto accept() -> base::Result<SocketFd>;
        auto send(std::string_view data, int flags = 0) -> base::Result<size_t>;
        auto recv(void* buf, size_t len, int flags = 0) -> base::Result<size_t>;
        auto sendto(std::string_view data, const std::string& addr, uint16_t port)
            -> base::Result<size_t>;
        auto recvfrom(void* buf, size_t len, std::string& src_addr, uint16_t& src_port)
            -> base::Result<size_t>;
        auto set_timeout(int seconds) -> base::Result<void>;
        auto set_reuseaddr(bool on) -> base::Result<void>;
        auto fd() const -> int;
        auto valid() const -> bool;
    };

    auto resolve_host(std::string_view host, uint16_t port,
                      Domain d = Domain::IPv4)
        -> base::Result<std::vector<struct sockaddr_storage>>;
    auto resolve_service(std::string_view name) -> base::Result<uint16_t>;
    auto addr_to_string(const struct sockaddr_storage& addr) -> std::string;
}
```

**系统头文件**：`<sys/socket.h>`、`<netdb.h>`、`<netinet/in.h>`、`<arpa/inet.h>`

**测试要点**：loopback 连接/监听往返、错误路径（连接拒绝、超时）、移动语义。

### 1.2 `include/cfbox/http.hpp` — 最小 HTTP/1.1 Client

**动机**：`wget` 需要 HTTP 响应解析。本模块处理 HTTP 协议，使 `wget` 专注于 CLI 选项和文件管理。

**接口设计**：

```cpp
namespace cfbox::http {
    struct URL {
        std::string scheme;   // "http"
        std::string host;
        uint16_t port = 80;
        std::string path;     // "/" + query + fragment
    };

    auto parse_url(std::string_view url) -> base::Result<URL>;

    struct Response {
        int status_code = 0;
        std::string status_text;
        std::unordered_map<std::string, std::string> headers;
        std::string body;
        size_t content_length = 0;
    };

    struct RequestOptions {
        int max_redirects = 10;
        int timeout_seconds = 30;
        bool verbose = false;
        std::function<void(size_t received, size_t total)> progress_callback;
    };

    auto http_get(const URL& url, RequestOptions opts = {})
        -> base::Result<Response>;
    auto http_get_to_file(const URL& url, const std::string& output_path,
                          RequestOptions opts = {})
        -> base::Result<void>;
}
```

**依赖**：`socket.hpp`。无 TLS。

**测试要点**：URL 解析（各种格式）、响应头解析、重定向跟踪、chunked transfer encoding、超时处理。

### 1.3 `include/cfbox/net_util.hpp` — 网络查询工具

**动机**：`netstat`、`ifconfig`、`route`、`ip` 都需要解析 `/proc/net/` 文件和 `ioctl` 查询。共享模块提供数据结构和解析器。

**接口设计**：

```cpp
namespace cfbox::net {
    struct InterfaceInfo {
        std::string name;
        std::string ipv4_addr;
        std::string ipv6_addr;
        std::string mac_addr;
        std::string broadcast;
        std::string netmask;
        int mtu = 0;
        bool up = false;
        bool loopback = false;
        uint64_t rx_bytes = 0, tx_bytes = 0;
        uint64_t rx_packets = 0, tx_packets = 0;
    };

    struct RouteEntry {
        std::string destination;
        std::string gateway;
        std::string genmask;
        std::string iface;
        int flags = 0;
        int metric = 0;
    };

    struct SocketEntry {
        std::string proto;      // tcp, tcp6, udp, udp6
        std::string local_addr;
        uint16_t local_port = 0;
        std::string remote_addr;
        uint16_t remote_port = 0;
        std::string state;
        pid_t pid = 0;
        std::string program;
    };

    // 读取 /proc/net/dev
    auto read_interfaces() -> base::Result<std::vector<InterfaceInfo>>;
    // 读取 /proc/net/route
    auto read_routes() -> base::Result<std::vector<RouteEntry>>;
    // 读取 /proc/net/tcp, /proc/net/tcp6, /proc/net/udp, /proc/net/udp6
    auto read_sockets() -> base::Result<std::vector<SocketEntry>>;

    // 接口操作
    auto interface_up(std::string_view name) -> base::Result<void>;
    auto interface_down(std::string_view name) -> base::Result<void>;
    auto set_interface_addr(std::string_view name, std::string_view addr,
                            std::string_view netmask) -> base::Result<void>;
    // 路由操作
    auto add_route(std::string_view dest, std::string_view gateway,
                   std::string_view netmask, std::string_view iface)
        -> base::Result<void>;
    auto delete_route(std::string_view dest, std::string_view gateway)
        -> base::Result<void>;
}
```

**数据源**：`/proc/net/dev`、`/proc/net/route`、`/proc/net/tcp`、`/proc/net/udp`、`<sys/ioctl.h>`、`<net/if.h>`

**测试要点**：/proc/net 解析正确性（用样本数据测试）、地址格式化。

### 1.4 `include/cfbox/icmp.hpp` — ICMP 协议支持

**动机**：`ping` 和 `traceroute` 需要 raw ICMP socket 和包构造/解析。

**接口设计**：

```cpp
namespace cfbox::icmp {
    struct IcmpEcho {
        uint8_t type;
        uint8_t code;
        uint16_t checksum;
        uint16_t identifier;
        uint16_t sequence;
        std::chrono::steady_clock::time_point send_time;
    };

    auto create_echo_request(uint16_t id, uint16_t seq) -> std::vector<uint8_t>;
    auto parse_echo_reply(const uint8_t* data, size_t len) -> base::Result<IcmpEcho>;
    auto compute_checksum(const uint8_t* data, size_t len) -> uint16_t;
}
```

**依赖**：`<netinet/ip_icmp.h>`、raw socket 支持（需 `CAP_NET_RAW` 或 root）

---

## Part 2: 新增 Applet（按依赖排序分波）

### Wave 1: 配置与状态（依赖 `socket.hpp` 和 `net_util.hpp`）

#### 2.1 `ip` — `src/applets/ip/`（多文件）

**文件结构**：
```
src/applets/ip/
├── ip.hpp          // 内部共享定义
├── ip_main.cpp     // 入口点、子命令分发
├── ip_addr.cpp     // addr show/add/del
├── ip_link.cpp     // link show/set
└── ip_route.cpp    // route show/add/del
```

**子命令（Phase 2 范围）**：
| 子命令 | 操作 |
|--------|------|
| `addr show` | 列出地址 |
| `addr add ADDR dev IFACE` | 添加地址 |
| `addr del ADDR dev IFACE` | 删除地址 |
| `link show` | 列出接口 |
| `link set IFACE up/down` | 启停接口 |
| `route show` | 列出路由 |
| `route add` | 添加路由 |
| `route del` | 删除路由 |

**选项**：`-4`（仅 IPv4）、`-6`（仅 IPv6）、`-s`（统计）、`-br`（简洁）

**依赖**：`net_util.hpp`

**注意**：BusyBox `ip` 支持很多子命令；Phase 2 只聚焦 `addr`、`link`、`route`。

#### 2.2 `ifconfig` — `src/applets/ifconfig.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-a` | 显示所有接口 |
| `IFACE` | 指定接口 |
| `ADDR` | 设置 IP 地址 |
| `netmask MASK` | 设置掩码 |
| `broadcast ADDR` | 设置广播地址 |
| `up` | 启动接口 |
| `down` | 关闭接口 |
| `mtu N` | 设置 MTU |
| `hw ether ADDR` | 设置 MAC 地址 |

**依赖**：`net_util.hpp`

#### 2.3 `route` — `src/applets/route.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-n` | 数字地址 |
| `add` | 添加路由 |
| `del` | 删除路由 |
| `-net` / `-host` | 网络或主机路由 |
| `gw GATEWAY` | 网关 |
| `netmask MASK` | 掩码 |
| `dev IFACE` | 指定接口 |
| `default` | 默认路由 |

**依赖**：`net_util.hpp`

#### 2.4 `netstat` — `src/applets/netstat.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-t` | TCP 连接 |
| `-u` | UDP 连接 |
| `-l` | 仅监听 |
| `-n` | 数字地址 |
| `-p` | 显示 PID/程序 |
| `-a` | 所有 |
| `-r` | 路由表 |

**依赖**：`net_util.hpp`

### Wave 2: 诊断（依赖 `icmp.hpp`）

#### 2.5 `ping` — `src/applets/ping.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-c COUNT` | 发送次数 |
| `-i INTERVAL` | 发送间隔（秒） |
| `-s PACKETSZ` | 数据大小 |
| `-W TIMEOUT` | 超时（秒） |
| `-q` | 静默 |
| `-n` | 数字地址 |
| `-4` / `-6` | IPv4/IPv6 |

**依赖**：`icmp.hpp`、`socket.hpp`（raw ICMP socket）

**实现要点**：
- 发送 ICMP echo request，接收 reply
- 计算 RTT 统计（min/avg/max）
- 进度显示：每收到一个 reply 打印一行
- 结束时显示统计摘要

**权限说明**：需 `CAP_NET_RAW` 或 root。文档必须说明。

#### 2.6 `traceroute` — `src/applets/traceroute.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-n` | 数字地址 |
| `-m MAX_TTL` | 最大 TTL（默认 30） |
| `-w TIMEOUT` | 超时（秒） |
| `-q QUERIES` | 每跳查询数 |
| `-p PORT` | 目标端口 |

**依赖**：`icmp.hpp`、`socket.hpp`

**实现**：发送 UDP/ICMP 包递增 TTL，监听 ICMP time exceeded。

#### 2.7 `nslookup` — `src/applets/nslookup.cpp`

**选项**：可选 server 地址

**依赖**：`<netdb.h>` 的 `getaddrinfo()`、`getnameinfo()`

**实现**：查询 DNS A/AAAA 记录，显示结果。不需要完整 DNS 解析器库。

### Wave 3: 传输与调试（依赖 `socket.hpp` 和 `http.hpp`）

#### 2.8 `wget` — `src/applets/wget.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-O FILE` | 输出文件名 |
| `-q` | 静默 |
| `-v` | 详细 |
| `--timeout=N` | 超时 |
| `-c` | 断点续传 |
| `--no-check-certificate` | 忽略证书（Phase 2 无效，占位） |
| `-t TRIES` | 重试次数 |
| `-w WAIT` | 重试等待 |

**依赖**：`http.hpp`、`socket.hpp`

**实现**：
- 解析 URL，HTTP GET，保存到文件
- 支持重定向跟踪
- Phase 2 仅 HTTP，无 TLS
- `--no-check-certificate` 是空操作占位符，文档需说明

**测试场景**：`wget http://example.com/` 可下载文件。

#### 2.9 `nc` (netcat) — `src/applets/nc.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-l` | 监听模式 |
| `-p PORT` | 端口 |
| `-s SOURCE` | 源地址 |
| `-w TIMEOUT` | 超时 |
| `-e PROG` | 执行程序 |
| `-z` | 扫描模式 |

**依赖**：`socket.hpp`

**两种模式**：
1. **连接模式**：`nc HOST PORT` — 连接后中继 stdin/stdout
2. **监听模式**：`nc -l -p PORT` — 绑定后等待连接，中继数据

**测试场景**：`nc -l -p 8080` 与客户端互通。

#### 2.10 `tftp` — `src/applets/tftp.cpp`

**选项**：
| 选项 | 说明 |
|------|------|
| `-g` | 下载（get） |
| `-p` | 上传（put） |
| `-l FILE` | 本地文件 |
| `-r FILE` | 远端文件 |
| `-b BLOCKSIZE` | 块大小 |

**依赖**：`socket.hpp`

**实现**：RFC 1350 TFTP 客户端。基于 UDP。发送 RRQ/WRQ，处理 DATA/ACK 包。

### 2.11 `hostname` 深化 — `src/applets/hostname.cpp`（已存在）

**新增选项**：
| 选项 | 说明 |
|------|------|
| `-s` | 短主机名 |
| `-i` | IP 地址 |
| `-f` | FQDN |
| `-d` | 域名 |

**依赖**：`<netdb.h>` 名称解析

---

## Part 3: 测试

### 3.1 单元测试（GTest）

| 文件 | 测试内容 |
|------|---------|
| `test_socket.cpp` | SocketFd 构造/移动、错误处理（loopback 测试） |
| `test_http.cpp` | URL 解析、响应头解析、重定向处理 |
| `test_net_util.cpp` | /proc/net 解析（样本数据）、接口信息提取 |
| `test_icmp.cpp` | ICMP 校验和、包构造/解析 |
| `test_ip.cpp` | 地址解析、路由条目格式化 |
| `test_wget.cpp` | URL 解析、本地 HTTP 服务器模拟 |
| `test_nc.cpp` | 连接/监听生命周期 |
| `test_ping.cpp` | ICMP 包构造 |

### 3.2 集成测试

| 脚本 | 测试场景 |
|------|---------|
| `test_ping.sh` | `ping -c 1 127.0.0.1` |
| `test_netstat.sh` | `netstat -tlnp`（在 namespace 中） |
| `test_nc.sh` | nc echo 测试（监听 + 连接） |
| `test_wget.sh` | 从本地 python HTTP 服务器下载 |
| `test_ifconfig.sh` | `ifconfig -a` 输出验证 |
| `test_route.sh` | `route -n` 输出验证 |
| `test_nslookup.sh` | 解析 localhost |
| `test_ip.sh` | `ip addr show`、`ip route show` |

### 3.3 QEMU 网络端到端测试

在 CI 中增加 QEMU 网络测试配置：

**配置文件**：`configs/qemu-network-test`
- QEMU 启动参数：`-nic user`（SLIRP 用户模式网络）
- CFBox 作为 init 启动

**测试序列**：
1. `ifconfig -a` — 显示网络接口
2. `ifconfig eth0 10.0.2.15 netmask 255.255.255.0 up` — 配置地址
3. `route add default gw 10.0.2.2` — 添加默认路由
4. `ping -c 3 10.0.2.2` — ping 网关
5. `wget http://10.0.2.2/ -O /tmp/test` — HTTP 下载（如果有测试服务器）

---

## Part 4: 文档

### Applet 文档
- 每个新 applet 标准 `HelpEntry`
- `hostname` 更新 HELP 常量

### 架构文档更新
- `document/architecture.md` 添加新头文件（socket、http、net_util、icmp）

### Cookbook 页面
- 使用 `ifconfig`/`ip` 配置网络接口
- 使用 `ping`/`traceroute` 诊断网络连通性
- 使用 `wget` 下载文件
- 使用 `nc` 调试 TCP/UDP 连接
- 在 QEMU 中配置 CFBox 网络

### TLS 策略文档
- 明确 Phase 2 仅支持 HTTP
- 文档化 TLS 作为未来可选构建变体
- `wget --no-check-certificate` 标注为占位

---

## Part 5: 兼容性

### BusyBox 对齐
- 所有网络 applet 短选项与 BusyBox 一致
- `ip` 只支持最常见的 `ip addr/route/link` 子命令（不是完整 `ip` 命令套件）
- `ifconfig` 输出格式模仿 BusyBox
- `netstat` 输出格式模仿 BusyBox
- `nc` 行为与 BusyBox nc 兼容

### 有意差异记录
- `wget` Phase 2 不支持 HTTPS（文档明确说明）
- `ip` 子命令覆盖范围有限
- `traceroute` 实现简化（可能只支持 UDP 模式）

### 退出码审计
- `ping`：0 = 至少一个 reply，1 = 无 reply，2 = 错误
- `wget`：0 = 成功，1 = 错误，2 = 参数错误
- `nc`：0 = 成功，1 = 连接失败

---

## Part 6: 发布

### CMake 更新
- `CFBOX_APPLETS` 添加 11 个新 applet（ip, ifconfig, route, netstat, ping, traceroute, nslookup, wget, nc, tftp, hostname 已存在）
- `applet_config.hpp.in` 添加新 `#cmakedefine01` 条目
- `ip` 作为多文件 applet 需要特殊 CMake 处理

### Profile 更新
| Profile | 新增网络 applet |
|---------|---------------|
| `rescue` | `ping`、`wget`、`ifconfig`、`route` |
| `container` | 所有网络 applet |
| `embedded` | 所有网络 applet |

### TLS 构建变体
- `CFBOX_ENABLE_TLS=OFF` 作为显式默认
- 文档化未来 TLS 集成计划

### 体积影响评估
| 新增基础设施 | 预估体积增量 |
|-------------|-------------|
| socket.hpp | ~5 KB |
| http.hpp | ~8 KB |
| net_util.hpp | ~6 KB |
| icmp.hpp | ~3 KB |
| 11 个新 applet | ~40 KB |
| **合计** | **~62 KB** |

---

## Part 7: 依赖图与里程碑

### 依赖关系

```
基础设施
├── socket.hpp ───── 所有 Wave
├── net_util.hpp ─── Wave 1 (ip, ifconfig, route, netstat)
├── icmp.hpp ─────── Wave 2 (ping, traceroute)
└── http.hpp ─────── Wave 3 (wget)

Wave 1 ── 配置与状态 (ip, ifconfig, route, netstat, hostname 深化)
Wave 2 ── 诊断 (ping, traceroute, nslookup)
Wave 3 ── 传输 (wget, nc, tftp)
```

### 里程碑

| 里程碑 | 时间 | 内容 |
|--------|------|------|
| M1 | 第 1-2 周 | 基础设施（socket、net_util、icmp）+ 单元测试 + loopback 集成测试 |
| M2 | 第 3-4 周 | Wave 1（ip、ifconfig、route、netstat、hostname 深化） |
| M3 | 第 5-6 周 | Wave 2（ping、traceroute、nslookup）+ QEMU 网络测试 |
| M4 | 第 7-8 周 | Wave 3（wget HTTP、nc、tftp） |
| M5 | 第 9-10 周 | 差异测试 vs BusyBox 网络工具 + 体积优化 + **Release v0.3.0** |

---

## 相关文档

- [生产化路线图](../production-roadmap.md) — 全局阶段顺序和优先级
- [兼容性策略](../compatibility-policy.md) — POSIX/BusyBox/GNU 行为裁决
- [v1.0 生产可用标准](../v1-production-criteria.md) — 最终发布门槛
- [Phase 0 前置门禁](phase-0a-baseline-inventory.md) — Phase 0A-0F 执行细节
