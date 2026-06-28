#include <cstdio>
#include <cstring>
#include <string>
#include <sys/resource.h>
#include <unistd.h>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "renice",
    .version = CFBOX_VERSION_STRING,
    .one_line = "alter priority of running processes",
    .usage   = "renice [-n INCREMENT] {-p PID|-g PGRP|-u USER}...",
    .options = "  -n N   increment (default 1)\n"
               "  -p     interpret args as PIDs (default)\n"
               "  -g     interpret args as process groups\n"
               "  -u     interpret args as user names/IDs",
    .extra   = "",
};

} // anonymous namespace

auto renice_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true, "priority"},
        cfbox::args::OptSpec{'p', false, "pid"},
        cfbox::args::OptSpec{'g', false, "pgrp"},
        cfbox::args::OptSpec{'u', false, "user"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int increment = 1;
    if (auto v = parsed.get('n')) {
        auto parsed_inc = cfbox::args::parse_int(*v);
        if (!parsed_inc) {
            CFBOX_ERR("renice", "%s", parsed_inc.error().msg.c_str());
            return 2;
        }
        increment = *parsed_inc;
    }

    const auto& args = parsed.positional();
    if (args.empty()) {
        CFBOX_ERR("renice", "no ID specified");
        return 1;
    }

    int rc = 0;
    for (const auto& id_str : args) {
        auto parsed_id = cfbox::args::parse_int(id_str);
        if (!parsed_id) {
            CFBOX_ERR("renice", "%s", parsed_id.error().msg.c_str());
            return 2;
        }
        auto id = static_cast<id_t>(*parsed_id);

        int which = PRIO_PROCESS;
        if (parsed.has('g') || parsed.has_long("pgrp")) which = PRIO_PGRP;
        if (parsed.has('u') || parsed.has_long("user")) which = PRIO_USER;

        errno = 0;
        auto current = getpriority(which, id);
        if (errno != 0) {
            CFBOX_ERR("renice", "%d: %s", static_cast<int>(id), std::strerror(errno));
            rc = 1;
            continue;
        }

        auto new_pri = current + increment;
        if (setpriority(which, id, new_pri) != 0) {
            CFBOX_ERR("renice", "%d: %s", static_cast<int>(id), std::strerror(errno));
            rc = 1;
        } else {
            std::printf("%d: old priority %d, new priority %d\n", static_cast<int>(id), current, new_pri);
        }
    }

    return rc;
}
