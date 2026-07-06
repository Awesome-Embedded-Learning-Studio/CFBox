// netstat.cpp — display network sockets (BusyBox netstat minimal subset).
//
// Wave 1c scope: -t/-u/-x (proto filter), -a (include LISTEN/servers), -n
// (numeric — already numeric; accepted for compat). Read-only. With no proto
// flag, defaults to tcp+udp (BusyBox behavior); unix requires -x.

#include <cstdio>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/net_util.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "netstat",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display network sockets (tcp/udp/unix, read-only)",
    .usage = "netstat [-tuxan]",
    .options =
        "  -t    TCP sockets\n"
        "  -u    UDP sockets\n"
        "  -x    Unix domain sockets\n"
        "  -a    show servers (LISTEN) too\n"
        "  -n    numeric addresses (always numeric here)",
    .extra = "",
};

} // namespace

auto netstat_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    auto parsed = args::parse(argc, argv,
                              {
                                  args::OptSpec{'t', false, "tcp"},
                                  args::OptSpec{'u', false, "udp"},
                                  args::OptSpec{'x', false, "unix"},
                                  args::OptSpec{'a', false, "all"},
                                  args::OptSpec{'n', false, "numeric"},
                              });

    if (parsed.has_long("help")) {
        help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        help::print_version(HELP);
        return 0;
    }

    // No proto flag → tcp+udp (BusyBox default). Any flag narrows to the union requested.
    const bool any_proto = parsed.has('t') || parsed.has('u') || parsed.has('x');
    const bool want_tcp = parsed.has('t') || !any_proto;
    const bool want_udp = parsed.has('u') || !any_proto;
    const bool want_unix = parsed.has('x');
    const bool servers = parsed.has('a');

    if (want_tcp) {
        auto socks = net::read_tcp_sockets();
        if (!socks) {
            CFBOX_ERR("netstat", "%s", socks.error().msg.c_str());
            return 1;
        }
        std::fputs(net::format_netstat_inet(*socks, servers).c_str(), stdout);
    }
    if (want_udp) {
        auto socks = net::read_udp_sockets();
        if (!socks) {
            CFBOX_ERR("netstat", "%s", socks.error().msg.c_str());
            return 1;
        }
        std::fputs(net::format_netstat_inet(*socks, servers).c_str(), stdout);
    }
    if (want_unix) {
        auto socks = net::read_unix_sockets();
        if (!socks) {
            CFBOX_ERR("netstat", "%s", socks.error().msg.c_str());
            return 1;
        }
        std::fputs(net::format_netstat_unix(*socks).c_str(), stdout);
    }
    return 0;
}
