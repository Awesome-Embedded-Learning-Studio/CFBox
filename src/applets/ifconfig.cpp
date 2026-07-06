// ifconfig.cpp — display or configure network interfaces.
//
// Read:  `ifconfig` (up interfaces), `ifconfig -a` (all), `ifconfig IFACE`.
// Write: `ifconfig IFACE ADDR [netmask NM] [broadcast BC] [mtu N] [up|down]` —
//        issues SIOCSIF* ioctls on one control socket; needs CAP_NET_ADMIN.
//        EPERM is reported per-ioctl (this never silently succeeds).
// Output mirrors the BusyBox multi-line format via net::format_ifconfig.

#include <netinet/in.h>

#include <cstdio>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/net_util.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "ifconfig",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display or configure network interfaces",
    .usage = "ifconfig [-a] [IFACE [ADDR [netmask NM] [broadcast BC] [mtu N] [up|down]]]",
    .options =
        "  -a    show all interfaces, including down ones\n"
        "  IFACE ADDR ...   configure the interface (needs CAP_NET_ADMIN)",
    .extra = "",
};

// Apply mutations parsed from positional args (IFACE first, then ADDR/keywords).
auto ifconfig_write(const std::vector<std::string_view>& args) -> int {
    using namespace cfbox;
    auto ctl = net::ctl_socket();
    if (!ctl) {
        CFBOX_ERR("ifconfig", "%s", ctl.error().msg.c_str());
        return 1;
    }
    const std::string_view iface = args[0];

    std::optional<in_addr> addr, mask, bcast;
    std::optional<int> mtu;
    int up = 0; // +1 up, -1 down, 0 unspecified

    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string_view t = args[i];

        // Consume the token after the current one (advances i). Used by value
        // keywords (netmask/broadcast/mtu).
        auto next = [&]() -> std::optional<std::string_view> {
            if (i + 1 >= args.size())
                return std::nullopt;
            return args[++i];
        };
        auto need_ipv4 = [&](std::string_view label) -> std::optional<in_addr> {
            auto v = next();
            if (!v) {
                CFBOX_ERR("ifconfig", "%.*s: missing value", static_cast<int>(label.size()),
                          label.data());
                return std::nullopt;
            }
            auto a = net::parse_ipv4(*v);
            if (!a) {
                CFBOX_ERR("ifconfig", "invalid %.*s '%.*s'", static_cast<int>(label.size()),
                          label.data(), static_cast<int>(v->size()), v->data());
                return std::nullopt;
            }
            return a;
        };

        if (t == "up") {
            up = 1;
        } else if (t == "down") {
            up = -1;
        } else if (t == "netmask") {
            auto v = need_ipv4("netmask");
            if (!v)
                return 1;
            mask = *v;
        } else if (t == "broadcast") {
            auto v = need_ipv4("broadcast");
            if (!v)
                return 1;
            bcast = *v;
        } else if (t == "mtu") {
            auto v = next();
            if (!v) {
                CFBOX_ERR("ifconfig", "mtu: missing value");
                return 1;
            }
            auto n = args::parse_int(*v);
            if (!n) {
                CFBOX_ERR("ifconfig", "invalid mtu '%.*s'", static_cast<int>(v->size()), v->data());
                return 1;
            }
            mtu = *n;
        } else if (!addr) {
            addr = net::parse_ipv4(t);
            if (!addr) {
                CFBOX_ERR("ifconfig", "invalid address '%.*s'", static_cast<int>(t.size()), t.data());
                return 1;
            }
        } else {
            CFBOX_ERR("ifconfig", "unexpected argument '%.*s'", static_cast<int>(t.size()), t.data());
            return 1;
        }
    }

    if (addr) {
        auto r = net::set_ipv4_addr(ctl->get(), iface, *addr);
        if (!r) {
            CFBOX_ERR("ifconfig", "%s", r.error().msg.c_str());
            return 1;
        }
    }
    if (mask) {
        auto r = net::set_netmask(ctl->get(), iface, *mask);
        if (!r) {
            CFBOX_ERR("ifconfig", "%s", r.error().msg.c_str());
            return 1;
        }
    }
    if (bcast) {
        auto r = net::set_broadcast(ctl->get(), iface, *bcast);
        if (!r) {
            CFBOX_ERR("ifconfig", "%s", r.error().msg.c_str());
            return 1;
        }
    }
    if (mtu) {
        auto r = net::set_mtu(ctl->get(), iface, *mtu);
        if (!r) {
            CFBOX_ERR("ifconfig", "%s", r.error().msg.c_str());
            return 1;
        }
    }
    if (up != 0) {
        auto r = net::set_if_up(ctl->get(), iface, up > 0);
        if (!r) {
            CFBOX_ERR("ifconfig", "%s", r.error().msg.c_str());
            return 1;
        }
    }
    return 0;
}

} // namespace

auto ifconfig_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    auto parsed = args::parse(argc, argv,
                              {
                                  args::OptSpec{'a', false, "all"},
                              });

    if (parsed.has_long("help")) {
        help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        help::print_version(HELP);
        return 0;
    }

    const auto& pos = parsed.positional();

    // Write path: IFACE + at least one mutation token.
    if (pos.size() >= 2) {
        return ifconfig_write(pos);
    }

    // Read path (display).
    auto interfaces = net::read_interfaces();
    if (!interfaces) {
        CFBOX_ERR("ifconfig", "%s", interfaces.error().msg.c_str());
        return 1;
    }

    // BusyBox semantics: no args → up interfaces only; -a → all; IFACE → that one
    std::string want;
    if (!pos.empty())
        want = std::string{pos[0]};
    const bool show_all = parsed.has('a') || !want.empty();

    bool any = false;
    for (const auto& it : *interfaces) {
        if (!want.empty() && it.name != want)
            continue;
        if (!show_all && !it.up())
            continue;
        std::fputs(net::format_ifconfig(it).c_str(), stdout);
        any = true;
    }
    if (!any && !want.empty()) {
        CFBOX_ERR("ifconfig", "interface '%s' not found", want.c_str());
        return 1;
    }
    return 0;
}
