#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "date",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print or set the system date and time",
    .usage   = "date [+FORMAT]",
    .options = "  -u     print or set Coordinated Universal Time (UTC)\n"
               "  -d STR display time described by STR",
    .extra   = "FORMAT supports: %Y %m %d %H %M %S %a %A %b %B %s %Z %j %W",
};
} // namespace

auto date_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'u', false},
        cfbox::args::OptSpec{'d', true, "date"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool utc = parsed.has('u');
    const auto& pos = parsed.positional();

    std::string format;
    for (auto p : pos) {
        if (!p.empty() && p[0] == '+') {
            format = std::string{p.substr(1)};
        }
    }

    if (format.empty()) {
        format = "%a %b %d %H:%M:%S %Z %Y";
    }

    time_t now = time(nullptr);
    if (auto d = parsed.get_any('d', "date")) {
        struct tm tm_val{};
        auto* result = strptime(std::string{*d}.c_str(), "%Y-%m-%d %H:%M:%S", &tm_val);
        if (!result) {
            result = strptime(std::string{*d}.c_str(), "%Y-%m-%d", &tm_val);
        }
        if (!result) {
            result = strptime(std::string{*d}.c_str(), "%H:%M:%S", &tm_val);
            if (result) {
                auto* now_tm = localtime(&now);
                tm_val.tm_year = now_tm->tm_year;
                tm_val.tm_mon = now_tm->tm_mon;
                tm_val.tm_mday = now_tm->tm_mday;
            }
        }
        if (result) {
            now = mktime(&tm_val);
        } else {
            std::fprintf(stderr, "cfbox date: invalid date '%.*s'\n",
                         static_cast<int>(d->size()), d->data());
            return 1;
        }
    }

    auto* tm = utc ? gmtime(&now) : localtime(&now);
    char buf[256];
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat-nonliteral"
    strftime(buf, sizeof(buf), format.c_str(), tm);
    std::puts(buf);
#pragma GCC diagnostic pop
    return 0;
}
