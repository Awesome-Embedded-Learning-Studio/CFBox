// ip.cpp — iproute2-style display (addr show subset). Wave 1b scope: only
// `ip addr [show]`. link/route/add/del subcommands come later. Output mirrors
// the BusyBox/iproute2 "ip addr" shape loosely (index, flags, mtu, link, inet).

#include <net/if.h>

#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/net_util.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "ip",
    .version = CFBOX_VERSION_STRING,
    .one_line = "show network addresses (ip addr show subset)",
    .usage = "ip addr [show] [IFACE]",
    .options = "  addr show    list interfaces and their addresses",
    .extra = "",
};

auto flags_angle(const cfbox::net::InterfaceInfo& it) -> std::string {
    std::string s;
    if (it.up())
        s += "UP,";
    if (it.loopback())
        s += "LOOPBACK,";
    if (it.flags & IFF_BROADCAST)
        s += "BROADCAST,";
    if (it.flags & IFF_POINTOPOINT)
        s += "POINTOPOINT,";
    if (it.flags & IFF_RUNNING)
        s += "LOWER_UP,";
    if (it.flags & IFF_MULTICAST)
        s += "MULTICAST,";
    if (!s.empty())
        s.pop_back();
    return s;
}

} // namespace

auto ip_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    // This batch only implements the "addr" subtree.
    if (argc < 2 || (std::string{argv[1]} != "addr" && std::string{argv[1]} != "a")) {
        CFBOX_ERR("ip", "usage: ip addr [show]");
        return 2;
    }

    // optional interface filter as positional after "addr [show]"
    std::string want;
    for (int i = 2; i < argc; ++i) {
        std::string_view a = argv[i];
        if (a == "show")
            continue;
        want = a;
    }

    auto interfaces = net::read_interfaces();
    if (!interfaces) {
        CFBOX_ERR("ip", "%s", interfaces.error().msg.c_str());
        return 1;
    }

    int idx = 1;
    for (const auto& it : *interfaces) {
        if (!want.empty() && it.name != want)
            continue;
        std::printf("%d: %s: <%s> mtu %d state %s\n", idx++, it.name.c_str(),
                    flags_angle(it).c_str(), it.mtu, it.up() ? "UP" : "DOWN");
        std::printf("    link/%s %s\n", it.loopback() ? "loopback" : "ether",
                    it.mac_addr.empty() ? "00:00:00:00:00:00" : it.mac_addr.c_str());
        if (!it.ipv4_addr.empty()) {
            int prefix = net::prefix_len(it.netmask);
            std::printf("    inet %s/%d", it.ipv4_addr.c_str(), prefix);
            if (!it.loopback() && !it.ipv4_bcast.empty())
                std::printf(" brd %s", it.ipv4_bcast.c_str());
            std::printf(" scope %s %s\n", it.loopback() ? "host" : "global", it.name.c_str());
        }
    }
    return 0;
}
