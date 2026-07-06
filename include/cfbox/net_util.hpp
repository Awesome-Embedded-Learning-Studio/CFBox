#pragma once

// net_util.hpp — network interface query helpers shared by ifconfig/ip/route/
// netstat. Data comes from two places: /proc/net/dev for packet counters, and
// ioctl(SIOCGIF*) on a control socket for flags/mtu/hwaddr/ipv4. Errors flow
// through cfbox::base::Result; no exceptions/RTTI. Read-only — mutations
// (setting addr/flags) belong in the applets that need them, not here.

#include <arpa/inet.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
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

} // namespace cfbox::net
