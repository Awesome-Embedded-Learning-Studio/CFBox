// ifconfig.cpp — display network interfaces (read-only subset for now).
//
// Implemented: `ifconfig` (up interfaces), `ifconfig -a` (all), `ifconfig IFACE`
// (one). Output mirrors the BusyBox multi-line format via net::format_ifconfig.
// Mutations (ADDR/netmask/up/down/mtu) are Wave 1 scope but deferred to a
// follow-up batch — this first cut closes the read path shared with ip/netstat.

#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/net_util.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "ifconfig",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display network interfaces (read-only)",
    .usage = "ifconfig [-a] [IFACE]",
    .options = "  -a    show all interfaces, including down ones",
    .extra = "",
};

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

    auto interfaces = net::read_interfaces();
    if (!interfaces) {
        CFBOX_ERR("ifconfig", "%s", interfaces.error().msg.c_str());
        return 1;
    }

    // BusyBox semantics: no args → up interfaces only; -a → all; IFACE → that one
    std::string want;
    if (!parsed.positional().empty())
        want = std::string{parsed.positional()[0]};
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
