#include <cstdio>
#include <ctime>
#include <string_view>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "uptime",
    .version = CFBOX_VERSION_STRING,
    .one_line = "tell how long the system has been running",
    .usage   = "uptime",
    .options = "",
    .extra   = "",
};

auto format_duration(double seconds) -> std::string {
    auto total_secs = static_cast<long>(seconds);
    auto days = total_secs / 86400;
    auto hours = (total_secs % 86400) / 3600;
    auto mins = (total_secs % 3600) / 60;

    char buf[64];
    if (days > 0) {
        std::snprintf(buf, sizeof(buf), "%ld day%s, %02ld:%02ld",
                      days, days > 1 ? "s" : "", hours, mins);
    } else {
        std::snprintf(buf, sizeof(buf), "%02ld:%02ld", hours, mins);
    }
    return buf;
}

} // anonymous namespace

auto uptime_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    // Current time
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    char time_buf[16];
    std::strftime(time_buf, sizeof(time_buf), " %H:%M:%S", tm);

    // Uptime
    auto up_result = cfbox::proc::read_uptime();
    if (!up_result) {
        std::fprintf(stderr, "cfbox uptime: %s\n", up_result.error().msg.c_str());
        return 1;
    }
    double uptime_secs = up_result->first;

    // Load average
    auto la_result = cfbox::proc::read_loadavg();
    double avg1 = 0, avg5 = 0, avg15 = 0;
    if (la_result) {
        avg1 = la_result->avg1;
        avg5 = la_result->avg5;
        avg15 = la_result->avg15;
    }

    std::printf("%s up %s, load average: %.2f, %.2f, %.2f\n",
                time_buf,
                format_duration(uptime_secs).c_str(),
                avg1, avg5, avg15);

    return 0;
}
