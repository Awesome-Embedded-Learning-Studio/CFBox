#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "dmesg",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print kernel ring buffer",
    .usage   = "dmesg [-T]",
    .options = "  -T     show timestamps in human-readable format",
    .extra   = "",
};

auto read_kmsg() -> std::vector<std::string> {
    std::vector<std::string> lines;
    // Try /var/log/dmesg first (works without CAP_SYSLOG)
    FILE* f = std::fopen("/var/log/dmesg", "r");
    if (!f) f = std::fopen("/var/log/kern.log", "r");
    if (!f) {
        std::fprintf(stderr, "cfbox dmesg: cannot open kernel log\n");
        return lines;
    }

    char buf[4096];
    while (std::fgets(buf, sizeof(buf), f)) {
        auto len = std::strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        lines.emplace_back(buf);
    }
    std::fclose(f);
    return lines;
}

} // anonymous namespace

auto dmesg_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'T', false, "time"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool timestamps = parsed.has('T') || parsed.has_long("time");
    auto lines = read_kmsg();

    for (const auto& line : lines) {
        if (timestamps) {
            // Try to extract timestamp from [ 1234.567] format
            auto bracket = line.find('[');
            auto close_bracket = line.find(']');
            if (bracket != std::string::npos && close_bracket != std::string::npos &&
                close_bracket > bracket) {
                auto ts_str = line.substr(bracket + 1, close_bracket - bracket - 1);
                auto ts = std::stod(ts_str);
                auto secs = static_cast<time_t>(ts);
                auto tm = std::localtime(&secs);
                char time_buf[32];
                std::strftime(time_buf, sizeof(time_buf), "%b %d %H:%M:%S", tm);
                std::printf("%s %s\n", time_buf, line.c_str());
            } else {
                std::printf("%s\n", line.c_str());
            }
        } else {
            std::printf("%s\n", line.c_str());
        }
    }

    return 0;
}
