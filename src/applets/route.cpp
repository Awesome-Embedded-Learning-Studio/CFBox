// route.cpp — display the kernel IP routing table (route -n style).
// Wave 1b scope: read-only display. add/del (SIOCADDRT/SIOCDELRT) deferred.

#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/net_util.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "route",
    .version = CFBOX_VERSION_STRING,
    .one_line = "show the IP routing table (display only)",
    .usage = "route [-n]",
    .options = "  -n    bypass name resolution (always numeric here)",
    .extra = "",
};

} // namespace

auto route_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    auto parsed = args::parse(argc, argv,
                              {
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

    if (!parsed.positional().empty()) {
        auto sub = parsed.positional()[0];
        if (sub == "add" || sub == "del") {
            CFBOX_ERR("route", "%.*s not supported in this build", static_cast<int>(sub.size()),
                      sub.data());
            return 2;
        }
    }

    auto routes = net::read_routes();
    if (!routes) {
        CFBOX_ERR("route", "%s", routes.error().msg.c_str());
        return 1;
    }
    std::fputs(net::format_route_table(*routes).c_str(), stdout);
    return 0;
}
