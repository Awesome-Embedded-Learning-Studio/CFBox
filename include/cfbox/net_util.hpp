#pragma once

// net_util.hpp — network interface query helpers shared by ifconfig/ip/route/
// netstat. Data comes from two places: /proc/net/dev for packet counters, and
// ioctl(SIOCGIF*) on a control socket for flags/mtu/hwaddr/ipv4. Errors flow
// through cfbox::base::Result; no exceptions/RTTI. Read-only — mutations
// (setting addr/flags) belong in the applets that need them, not here.

#include <arpa/inet.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cfbox/error.hpp>
#include <cfbox/io.hpp>

namespace cfbox::net {

struct InterfaceInfo {
    std::string name;
    int flags = 0; // raw ifr_flags (IFF_UP | IFF_BROADCAST | ...)
    int mtu = 0;
    std::string ipv4_addr;  // dotted, empty if unassigned
    std::string ipv4_bcast; // dotted, empty if none
    std::string netmask;    // dotted, empty if none
    std::string mac_addr;   // "aa:bb:..", empty if loopback/unset

    std::uint64_t rx_bytes = 0, rx_packets = 0, rx_errors = 0, rx_dropped = 0;
    std::uint64_t tx_bytes = 0, tx_packets = 0, tx_errors = 0, tx_dropped = 0;

    [[nodiscard]] auto up() const -> bool { return flags & IFF_UP; }
    [[nodiscard]] auto loopback() const -> bool { return flags & IFF_LOOPBACK; }
};

// Parse the 16 counters after the ':' on a /proc/net/dev line. Hand-rolled to
// avoid pulling in <sstream> (size cost) — the values fit in uint64.
inline auto parse_dev_fields(std::string_view s) -> std::vector<std::uint64_t> {
    std::vector<std::uint64_t> nums;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i])))
            ++i;
        if (i >= s.size())
            break;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            ++i;
            continue;
        }
        std::uint64_t n = 0;
        while (i < s.size() && std::isdigit(static_cast<unsigned char>(s[i]))) {
            n = n * 10 + static_cast<std::uint64_t>(s[i] - '0');
            ++i;
        }
        nums.push_back(n);
    }
    return nums;
}

// Copy an ioctl-returned sockaddr into a sockaddr_in via memcpy — a direct
// reinterpret_cast would trip -Wcast-align on 32-bit ARM, where sockaddr_in
// requires stricter alignment than sockaddr.
inline auto ipv4_from_ioctl(const sockaddr* sa) -> std::string {
    sockaddr_in in{};
    std::memcpy(&in, sa, sizeof(in));
    char buf[INET_ADDRSTRLEN];
    if (::inet_ntop(AF_INET, &in.sin_addr, buf, sizeof(buf)))
        return buf;
    return {};
}

// Split a line into whitespace-delimited fields (space or tab). Empty fields are
// skipped. Shared by /proc/net/route and /proc/net/{tcp,udp,unix} parsing — kept
// here so every /proc parser uses the same tokenization.
inline auto split_fields(std::string_view line) -> std::vector<std::string> {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t' || c == ' ') {
            if (!cur.empty()) {
                out.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty())
        out.push_back(std::move(cur));
    return out;
}

// Resolve a host to an IPv4 sockaddr_storage (AF_INET only — ping/traceroute
// are IPv4 in this cut). Returns false on resolution failure. Shared by ping,
// traceroute, and any future host→addr applet.
[[nodiscard]] inline auto resolve_ipv4(std::string_view host, sockaddr_storage& out) -> bool {
    char buf[256];
    if (host.size() >= sizeof(buf))
        return false;
    std::memcpy(buf, host.data(), host.size());
    buf[host.size()] = '\0';
    addrinfo hints{};
    hints.ai_family = AF_INET;
    addrinfo* res = nullptr;
    if (::getaddrinfo(buf, nullptr, &hints, &res) != 0)
        return false;
    bool ok = res && res->ai_addrlen <= sizeof(out);
    if (ok)
        std::memcpy(&out, res->ai_addr, res->ai_addrlen); // sockaddr_in via memcpy
    if (res)
        ::freeaddrinfo(res);
    return ok;
}

// Reverse-DNS (or numeric with `numeric=true`) of a sockaddr. Falls back to a
// "?" literal if getnameinfo fails — callers can use NI_NUMERICHOST for -n.
inline auto name_of(const sockaddr_storage& ss, bool numeric) -> std::string {
    char host[NI_MAXHOST];
    int flags = numeric ? NI_NUMERICHOST : 0;
    if (::getnameinfo(reinterpret_cast<const sockaddr*>(&ss), sizeof(ss), host, sizeof(host),
                      nullptr, 0, flags) == 0)
        return host;
    return "?";
}

// Read all interfaces: /proc/net/dev for counters + ioctl for the rest.
[[nodiscard]] inline auto read_interfaces() -> base::Result<std::vector<InterfaceInfo>> {
    int ctl = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (ctl < 0)
        return std::unexpected(base::Error{errno, "socket() for ioctl failed"});
    io::unique_fd ctl_fd{ctl};

    CFBOX_TRY(content, io::read_all("/proc/net/dev"));
    auto lines = io::split_lines(*content);

    std::vector<InterfaceInfo> out;
    for (std::size_t i = 2; i < lines.size(); ++i) { // skip 2 header lines
        const auto& line = lines[i];
        auto colon = line.find(':');
        if (colon == std::string::npos)
            continue;

        // name = line[0..colon) with whitespace stripped
        std::string name;
        for (std::size_t j = 0; j < colon; ++j) {
            char c = line[j];
            if (!std::isspace(static_cast<unsigned char>(c)))
                name += c;
        }
        if (name.empty())
            continue;

        InterfaceInfo info{};
        info.name = name;
        auto nums = parse_dev_fields(std::string_view(line).substr(colon + 1));
        if (nums.size() >= 12) {
            info.rx_bytes = nums[0];
            info.rx_packets = nums[1];
            info.rx_errors = nums[2];
            info.rx_dropped = nums[3];
            info.tx_bytes = nums[8];
            info.tx_packets = nums[9];
            info.tx_errors = nums[10];
            info.tx_dropped = nums[11];
        }

        struct ifreq ifr{};
        name.copy(ifr.ifr_name, IFNAMSIZ - 1); // ifr zero-init → guaranteed NUL-terminated

        if (::ioctl(ctl_fd.get(), SIOCGIFFLAGS, &ifr) == 0)
            info.flags = ifr.ifr_flags;
        if (::ioctl(ctl_fd.get(), SIOCGIFMTU, &ifr) == 0)
            info.mtu = ifr.ifr_mtu;
        if (::ioctl(ctl_fd.get(), SIOCGIFHWADDR, &ifr) == 0 && !info.loopback()) {
            auto* m = reinterpret_cast<unsigned char*>(ifr.ifr_hwaddr.sa_data);
            char buf[18];
            std::snprintf(buf, sizeof(buf), "%02x:%02x:%02x:%02x:%02x:%02x", m[0], m[1], m[2], m[3],
                          m[4], m[5]);
            info.mac_addr = buf;
        }
        if (::ioctl(ctl_fd.get(), SIOCGIFADDR, &ifr) == 0)
            info.ipv4_addr = ipv4_from_ioctl(&ifr.ifr_addr);
        if (::ioctl(ctl_fd.get(), SIOCGIFNETMASK, &ifr) == 0)
            info.netmask = ipv4_from_ioctl(&ifr.ifr_netmask);
        if (::ioctl(ctl_fd.get(), SIOCGIFBRDADDR, &ifr) == 0)
            info.ipv4_bcast = ipv4_from_ioctl(&ifr.ifr_broadaddr);
        out.push_back(std::move(info));
    }
    return out;
}

// ---- write path (SIOCSIF* ioctls, requires CAP_NET_ADMIN) ----

// Control socket used for both SIOCGIF* (read) and SIOCSIF* (write) ioctls.
[[nodiscard]] inline auto ctl_socket() -> base::Result<io::unique_fd> {
    int fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0)
        return std::unexpected(base::Error{errno, "control socket failed"});
    return io::unique_fd{fd};
}

// Parse dotted-quad → in_addr. nullopt on malformed input.
[[nodiscard]] inline auto parse_ipv4(std::string_view s) -> std::optional<in_addr> {
    char buf[INET_ADDRSTRLEN];
    if (s.empty() || s.size() >= sizeof(buf))
        return std::nullopt;
    std::memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';
    in_addr out{};
    if (::inet_pton(AF_INET, buf, &out.s_addr) != 1)
        return std::nullopt;
    return out;
}

namespace detail {

// Build an ifreq with name + an IPv4 sockaddr_in filled from `addr`. The
// sockaddr is written via memcpy into a local then assigned, mirroring
// ipv4_from_ioctl in reverse — a direct cast into ifr_ifru would trip
// -Wcast-align on 32-bit ARM.
inline auto ifreq_with_addr(std::string_view iface, int cmd, in_addr addr) -> ifreq {
    ifreq ifr{};
    iface.copy(ifr.ifr_name, IFNAMSIZ - 1); // zero-init → NUL-terminated
    sockaddr_in sin{};
    sin.sin_family = AF_INET;
    sin.sin_addr = addr;
    std::memcpy(&ifr.ifr_addr, &sin, sizeof(sin));
    (void)cmd;
    return ifr;
}

// Build a diagnostic Error carrying errno's text (e.g. "SIOCSIFADDR failed:
// Operation not permitted") — the bare ioctl name alone is not enough for a
// user to tell EPERM (no CAP_NET_ADMIN) from ENODEV (no such interface).
inline auto ioctl_error(const char* what, int e) -> base::Error {
    return base::Error{e, std::string{what} + ": " + std::strerror(e)};
}

} // namespace detail

[[nodiscard]] inline auto set_ipv4_addr(int ctl, std::string_view iface, in_addr addr)
    -> base::Result<void> {
    auto ifr = detail::ifreq_with_addr(iface, SIOCSIFADDR, addr);
    if (::ioctl(ctl, SIOCSIFADDR, &ifr) < 0)
        return std::unexpected(detail::ioctl_error("SIOCSIFADDR failed", errno));
    return {};
}

[[nodiscard]] inline auto set_netmask(int ctl, std::string_view iface, in_addr mask)
    -> base::Result<void> {
    auto ifr = detail::ifreq_with_addr(iface, SIOCSIFNETMASK, mask);
    if (::ioctl(ctl, SIOCSIFNETMASK, &ifr) < 0)
        return std::unexpected(detail::ioctl_error("SIOCSIFNETMASK failed", errno));
    return {};
}

[[nodiscard]] inline auto set_broadcast(int ctl, std::string_view iface, in_addr bcast)
    -> base::Result<void> {
    auto ifr = detail::ifreq_with_addr(iface, SIOCSIFBRDADDR, bcast);
    if (::ioctl(ctl, SIOCSIFBRDADDR, &ifr) < 0)
        return std::unexpected(detail::ioctl_error("SIOCSIFBRDADDR failed", errno));
    return {};
}

[[nodiscard]] inline auto set_mtu(int ctl, std::string_view iface, int mtu)
    -> base::Result<void> {
    ifreq ifr{};
    iface.copy(ifr.ifr_name, IFNAMSIZ - 1);
    ifr.ifr_mtu = mtu;
    if (::ioctl(ctl, SIOCSIFMTU, &ifr) < 0)
        return std::unexpected(detail::ioctl_error("SIOCSIFMTU failed", errno));
    return {};
}

// Bring iface up/down via read-modify-write on flags (preserve BROADCAST/etc).
[[nodiscard]] inline auto set_if_up(int ctl, std::string_view iface, bool up)
    -> base::Result<void> {
    ifreq ifr{};
    iface.copy(ifr.ifr_name, IFNAMSIZ - 1);
    if (::ioctl(ctl, SIOCGIFFLAGS, &ifr) < 0)
        return std::unexpected(detail::ioctl_error("SIOCGIFFLAGS failed", errno));
    if (up)
        ifr.ifr_flags |= static_cast<short>(IFF_UP);
    else
        ifr.ifr_flags &= ~static_cast<short>(IFF_UP);
    if (::ioctl(ctl, SIOCSIFFLAGS, &ifr) < 0)
        return std::unexpected(detail::ioctl_error("SIOCSIFFLAGS failed", errno));
    return {};
}

// Format an interface in the multi-line BusyBox ifconfig style.
inline auto format_ifconfig(const InterfaceInfo& it) -> std::string {
    auto pad = [](const std::string& s, std::size_t w) -> std::string {
        return s.size() >= w ? s + ' ' : s + std::string(w - s.size(), ' ');
    };

    auto human = [](std::uint64_t b) -> std::string {
        constexpr const char* unit[] = {"B", "KB", "MB", "GB", "TB"};
        double d = static_cast<double>(b);
        std::size_t u = 0;
        while (d >= 1024.0 && u < 4) {
            d /= 1024.0;
            ++u;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.1f %s", d, unit[u]);
        return buf;
    };

    auto flags_string = [](int flags, bool lo) -> std::string {
        std::string s;
        if (flags & IFF_UP)
            s += "UP ";
        if (!lo && (flags & IFF_BROADCAST))
            s += "BROADCAST ";
        if (lo)
            s += "LOOPBACK ";
        if (flags & IFF_POINTOPOINT)
            s += "POINTOPOINT ";
        if (flags & IFF_RUNNING)
            s += "RUNNING ";
        if (flags & IFF_NOARP)
            s += "NOARP ";
        if (flags & IFF_PROMISC)
            s += "PROMISC ";
        if (!lo && (flags & IFF_MULTICAST))
            s += "MULTICAST ";
        if (!s.empty())
            s.pop_back();
        return s;
    };

    const std::string indent(10, ' ');
    std::string out = pad(it.name, 10);

    // Line 1: Link encap + HWaddr
    out += "Link encap:";
    out += it.loopback() ? "Local Loopback" : "Ethernet";
    if (!it.mac_addr.empty())
        out += "  HWaddr " + it.mac_addr;
    out += '\n';

    // Line 2: inet addr / Bcast / Mask (loopback has no broadcast)
    if (!it.ipv4_addr.empty()) {
        out += indent + "inet addr:" + it.ipv4_addr;
        if (!it.loopback() && !it.ipv4_bcast.empty())
            out += "  Bcast:" + it.ipv4_bcast;
        if (!it.netmask.empty())
            out += "  Mask:" + it.netmask;
        out += '\n';
    }

    // Line 3: flags + MTU + Metric
    out += indent + flags_string(it.flags, it.loopback());
    out += "  MTU:" + std::to_string(it.mtu);
    out += "  Metric:1\n";

    // Line 4/5: RX/TX counters
    auto count = [](std::string_view dir, std::uint64_t packets, std::uint64_t errs,
                    std::uint64_t dropped, std::string_view tail) -> std::string {
        char buf[160];
        std::snprintf(
            buf, sizeof(buf), "%.*s packets:%llu errors:%llu dropped:%llu overruns:0 %.*s",
            static_cast<int>(dir.size()), dir.data(), static_cast<unsigned long long>(packets),
            static_cast<unsigned long long>(errs), static_cast<unsigned long long>(dropped),
            static_cast<int>(tail.size()), tail.data());
        return buf;
    };
    out += indent + count("RX", it.rx_packets, it.rx_errors, it.rx_dropped, "frame:0") + '\n';
    out += indent + count("TX", it.tx_packets, it.tx_errors, it.tx_dropped, "carrier:0") + '\n';

    // Line 6: collisions + txqueuelen (defaults; full ifconfig queries SIOCGIFTXQLEN)
    out += indent + "collisions:0  txqueuelen:1000\n";

    // Line 7: bytes (human-readable in parens, like BusyBox)
    out += indent + "RX bytes:" + std::to_string(it.rx_bytes) + " (" + human(it.rx_bytes) + ")  ";
    out += "TX bytes:" + std::to_string(it.tx_bytes) + " (" + human(it.tx_bytes) + ")\n";

    return out;
}

// ---- routing table (/proc/net/route) ----

struct RouteEntry {
    std::string destination; // dotted, "0.0.0.0" for default
    std::string gateway;     // dotted, "0.0.0.0" if none (on-link)
    std::string genmask;     // dotted
    std::string iface;
    int flags = 0; // raw RTF_* bits
    int metric = 0;
};

// /proc/net/route prints addresses as host-endian hex; assign straight into
// in_addr.s_addr (the targets we ship are little-endian).
inline auto hex_to_ipv4(std::string_view hex) -> std::string {
    std::uint32_t v =
        static_cast<std::uint32_t>(std::strtoul(std::string{hex}.c_str(), nullptr, 16));
    in_addr a;
    a.s_addr = v;
    char buf[INET_ADDRSTRLEN];
    return ::inet_ntop(AF_INET, &a, buf, sizeof(buf)) ? buf : std::string{};
}

// Bit width of a contiguous-ones netmask (255.255.255.0 -> 24).
inline auto prefix_len(const std::string& mask) -> int {
    in_addr a;
    if (::inet_pton(AF_INET, mask.c_str(), &a) != 1)
        return 0;
    std::uint32_t v = ntohl(a.s_addr);
    int n = 0;
    while (v & 0x80000000u) {
        ++n;
        v <<= 1;
    }
    return n;
}

[[nodiscard]] inline auto read_routes() -> base::Result<std::vector<RouteEntry>> {
    CFBOX_TRY(content, io::read_all("/proc/net/route"));
    auto lines = io::split_lines(*content);
    std::vector<RouteEntry> out;
    for (std::size_t i = 1; i < lines.size(); ++i) { // skip header line
        const auto& line = lines[i];
        if (line.empty())
            continue;
        // tab-separated: Iface Dest GW Flags RefCnt Use Metric Mask MTU Window IRTT
        auto f = split_fields(line);
        if (f.size() < 8)
            continue;

        RouteEntry r;
        r.iface = f[0];
        r.destination = hex_to_ipv4(f[1]);
        r.gateway = (f[2] == "00000000") ? "0.0.0.0" : hex_to_ipv4(f[2]);
        r.flags = static_cast<int>(std::strtoul(f[3].c_str(), nullptr, 16));
        r.metric = static_cast<int>(std::strtoul(f[6].c_str(), nullptr, 10));
        r.genmask = hex_to_ipv4(f[7]);
        out.push_back(std::move(r));
    }
    return out;
}

inline auto format_route_table(const std::vector<RouteEntry>& routes) -> std::string {
    std::string out = "Kernel IP routing table\n";
    char buf[256];
    std::snprintf(buf, sizeof(buf), "%-16s%-16s%-16s%-6s%-7s%-6s%-8s%s\n", "Destination", "Gateway",
                  "Genmask", "Flags", "Metric", "Ref", "Use", "Iface");
    out += buf;
    for (const auto& r : routes) {
        std::string fl;
        if (r.flags & 0x1)
            fl += 'U'; // RTF_UP
        if (r.flags & 0x2)
            fl += 'G'; // RTF_GATEWAY
        if (r.flags & 0x4)
            fl += 'H'; // RTF_HOST
        if (r.flags & 0x8)
            fl += 'R'; // RTF_REINSTATE
        if (r.flags & 0x10)
            fl += 'D'; // RTF_DYNAMIC
        if (r.flags & 0x20)
            fl += 'M'; // RTF_MODIFIED
        std::snprintf(buf, sizeof(buf), "%-16s%-16s%-16s%-6s%-7d%-6d%-8d%s\n",
                      r.destination.c_str(), r.gateway.c_str(), r.genmask.c_str(), fl.c_str(),
                      r.metric, 0, 0, r.iface.c_str());
        out += buf;
    }
    return out;
}

// ---- socket tables (/proc/net/tcp /proc/net/udp /proc/net/unix) ----

struct SocketEntry {
    std::string proto;        // "tcp" / "udp"
    std::string local_addr;   // dotted
    std::uint16_t local_port = 0;
    std::string remote_addr;  // dotted
    std::uint16_t remote_port = 0;
    int state = 0;            // raw st code from /proc/net/tcp
    std::uint64_t tx_queue = 0;
    std::uint64_t rx_queue = 0;
};

// /proc/net/tcp state code → BusyBox name. 0A=LISTEN, 01=ESTABLISHED, ...
inline auto tcp_state_name(int state) -> const char* {
    switch (state) {
        case 0x01: return "ESTABLISHED";
        case 0x02: return "SYN_SENT";
        case 0x03: return "SYN_RECV";
        case 0x04: return "FIN_WAIT1";
        case 0x05: return "FIN_WAIT2";
        case 0x06: return "TIME_WAIT";
        case 0x07: return "CLOSE";
        case 0x08: return "CLOSE_WAIT";
        case 0x09: return "LAST_ACK";
        case 0x0A: return "LISTEN";
        case 0x0B: return "CLOSING";
        default: return "UNKNOWN";
    }
}

// Parse "0100007F:0050" (host-endian hex ip : hex port) → dotted addr + port.
inline auto parse_ipv4_port(std::string_view s) -> std::pair<std::string, std::uint16_t> {
    auto colon = s.find(':');
    if (colon == std::string_view::npos)
        return {};
    std::string addr = hex_to_ipv4(s.substr(0, colon));
    auto port = static_cast<std::uint16_t>(
        std::strtoul(std::string{s.substr(colon + 1)}.c_str(), nullptr, 16));
    return {std::move(addr), port};
}

// Parse one /proc/net/tcp|udp data line. nullopt on header/blank lines.
inline auto parse_inet_line(std::string_view line, std::string_view proto)
    -> std::optional<SocketEntry> {
    auto f = split_fields(line);
    if (f.size() < 4)
        return std::nullopt;
    // Data rows: "N: hexIP:hexPort hexIP:hexPort ST ...". Reject rows whose
    // address fields lack a ':' — that covers the header line ("local_address").
    if (f[1].find(':') == std::string::npos || f[2].find(':') == std::string::npos)
        return std::nullopt;
    // f[0]="N:" sl  f[1]=local  f[2]=remote  f[3]=st  f[4]=txq:rxq queues
    SocketEntry e;
    e.proto = std::string{proto};
    auto [la, lp] = parse_ipv4_port(f[1]);
    e.local_addr = std::move(la);
    e.local_port = lp;
    auto [ra, rp] = parse_ipv4_port(f[2]);
    e.remote_addr = std::move(ra);
    e.remote_port = rp;
    e.state = static_cast<int>(std::strtoul(f[3].c_str(), nullptr, 16));
    if (f.size() > 4) {
        auto c = f[4].find(':');
        if (c != std::string::npos) {
            e.tx_queue = std::strtoull(f[4].substr(0, c).c_str(), nullptr, 16);
            e.rx_queue = std::strtoull(f[4].substr(c + 1).c_str(), nullptr, 16);
        }
    }
    return e;
}

// Parse a full /proc/net/tcp|udp dump (skipping its header line).
inline auto parse_inet_sockets(std::string_view content, std::string_view proto)
    -> std::vector<SocketEntry> {
    auto lines = io::split_lines(content);
    std::vector<SocketEntry> out;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (auto e = parse_inet_line(lines[i], proto))
            out.push_back(*e);
    }
    return out;
}

[[nodiscard]] inline auto read_tcp_sockets() -> base::Result<std::vector<SocketEntry>> {
    CFBOX_TRY(content, io::read_all("/proc/net/tcp"));
    return parse_inet_sockets(*content, "tcp");
}

[[nodiscard]] inline auto read_udp_sockets() -> base::Result<std::vector<SocketEntry>> {
    CFBOX_TRY(content, io::read_all("/proc/net/udp"));
    return parse_inet_sockets(*content, "udp");
}

// Format inet sockets in BusyBox netstat style. `servers` true → include LISTEN
// (==-a). Remote port 0 prints as "*" (BusyBox convention). UDP has no State.
inline auto format_netstat_inet(const std::vector<SocketEntry>& socks, bool servers)
    -> std::string {
    std::string out = servers ? "Active Internet connections (servers and established)\n"
                              : "Active Internet connections (w/o servers)\n";
    char buf[200];
    std::snprintf(buf, sizeof(buf), "%-5s %-6s %-6s %-23s %-23s %s\n", "Proto", "Recv-Q",
                  "Send-Q", "Local Address", "Foreign Address", "State");
    out += buf;
    auto port_str = [](std::uint16_t p) -> std::string {
        return p == 0 ? "*" : std::to_string(p);
    };
    for (const auto& e : socks) {
        if (!servers && e.state == 0x0A) // skip LISTEN unless -a
            continue;
        std::string local = e.local_addr + ':' + port_str(e.local_port);
        std::string remote = e.remote_addr + ':' + port_str(e.remote_port);
        const char* state = e.proto == "tcp" ? tcp_state_name(e.state) : "";
        std::snprintf(buf, sizeof(buf), "%-5s %-6llu %-6llu %-23s %-23s %s\n", e.proto.c_str(),
                      static_cast<unsigned long long>(e.rx_queue),
                      static_cast<unsigned long long>(e.tx_queue), local.c_str(),
                      remote.c_str(), state);
        out += buf;
    }
    return out;
}

// ---- unix domain sockets (/proc/net/unix) ----

struct UnixSocketEntry {
    int refcount = 0;
    int flags = 0;
    int type = 0;  // SOCK_STREAM=1 SOCK_DGRAM=2 SOCK_SEQPACKET=5
    int state = 0; // 01=LISTENING 03=CONNECTED 07=CLOSED
    std::uint64_t inode = 0;
    std::string path;
};

inline auto unix_type_name(int t) -> const char* {
    switch (t) {
        case 1: return "STREAM";
        case 2: return "DGRAM";
        case 5: return "SEQPACKET";
        default: return "";
    }
}

inline auto unix_state_name(int s) -> const char* {
    switch (s) {
        case 0x01: return "LISTENING";
        case 0x02: return "CONNECTING";
        case 0x03: return "CONNECTED";
        case 0x07: return "CLOSED";
        default: return "";
    }
}

inline auto parse_unix_line(std::string_view line) -> std::optional<UnixSocketEntry> {
    auto f = split_fields(line);
    // Num: RefCount Protocol Flags Type St Inode Path?
    if (f.size() < 7)
        return std::nullopt;
    UnixSocketEntry e;
    e.refcount = static_cast<int>(std::strtoul(f[1].c_str(), nullptr, 16));
    e.flags = static_cast<int>(std::strtoul(f[3].c_str(), nullptr, 16));
    e.type = static_cast<int>(std::strtoul(f[4].c_str(), nullptr, 16));
    e.state = static_cast<int>(std::strtoul(f[5].c_str(), nullptr, 16));
    e.inode = std::strtoull(f[6].c_str(), nullptr, 10);
    if (f.size() > 7)
        e.path = f[7];
    return e;
}

inline auto parse_unix_sockets(std::string_view content) -> std::vector<UnixSocketEntry> {
    auto lines = io::split_lines(content);
    std::vector<UnixSocketEntry> out;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (auto e = parse_unix_line(lines[i]))
            out.push_back(*e);
    }
    return out;
}

[[nodiscard]] inline auto read_unix_sockets() -> base::Result<std::vector<UnixSocketEntry>> {
    CFBOX_TRY(content, io::read_all("/proc/net/unix"));
    return parse_unix_sockets(*content);
}

inline auto format_netstat_unix(const std::vector<UnixSocketEntry>& socks) -> std::string {
    std::string out = "Active UNIX domain sockets (w/o servers)\n";
    char buf[200];
    std::snprintf(buf, sizeof(buf), "%-5s %-6s %-11s %-10s %-13s %-8s %s\n", "Proto", "RefCnt",
                  "Flags", "Type", "State", "I-Node", "Path");
    out += buf;
    for (const auto& e : socks) {
        std::snprintf(buf, sizeof(buf), "%-5s %-6d %-11s %-10s %-13s %-8llu %s\n", "unix",
                      e.refcount, (e.flags & 0x10000) ? "[ ACC ]" : "", unix_type_name(e.type),
                      unix_state_name(e.state), static_cast<unsigned long long>(e.inode),
                      e.path.c_str());
        out += buf;
    }
    return out;
}

} // namespace cfbox::net
