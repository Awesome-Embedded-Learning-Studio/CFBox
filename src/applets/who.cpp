#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <utmp.h>
#include <time.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "who",
    .version = CFBOX_VERSION_STRING,
    .one_line = "show who is logged on",
    .usage   = "who",
    .options = "",
    .extra   = "",
};
} // namespace

auto who_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    utmp* entry;
    setutent();
    while ((entry = getutent()) != nullptr) {
        if (entry->ut_type != USER_PROCESS) continue;
        char timebuf[32];
        time_t t = static_cast<time_t>(entry->ut_tv.tv_sec);
        auto* tm = localtime(&t);
        strftime(timebuf, sizeof(timebuf), "%b %d %H:%M", tm);
        std::printf("%-8s %-12s %s\n", entry->ut_user, entry->ut_line, timebuf);
    }
    endutent();
    return 0;
}
