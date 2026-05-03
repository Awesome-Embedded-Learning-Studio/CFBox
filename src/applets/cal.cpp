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
    .name    = "cal",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display a calendar",
    .usage   = "cal [[MONTH] YEAR]",
    .options = "  -3     show prev/curr/next month\n"
               "  -y     show whole year",
    .extra   = "",
};

auto days_in_month(int year, int month) -> int {
    // month is 1-12
    static const int days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 2 && (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)))
        return 29;
    return days[month];
}

auto day_of_week(int year, int month, int day) -> int {
    // 0=Sunday, 1=Monday, ..., 6=Saturday (Zeller's congruence simplified)
    struct tm tm{};
    tm.tm_year = year - 1900;
    tm.tm_mon = month - 1;
    tm.tm_mday = day;
    std::mktime(&tm);
    return tm.tm_wday; // 0=Sunday
}

auto month_name(int month) -> const char* {
    static const char* names[] = {
        "", "January", "February", "March", "April", "May", "June",
        "July", "August", "September", "October", "November", "December"
    };
    return names[month];
}

auto print_month(int year, int month) -> void {
    char header[32];
    std::snprintf(header, sizeof(header), "%s %d", month_name(month), year);
    auto header_len = static_cast<int>(std::strlen(header));

    // Center the header over "Su Mo Tu We Th Fr Sa" (20 chars)
    auto pad = (20 - header_len) / 2;
    if (pad < 0) pad = 0;
    std::printf("%*s%s\n", pad, "", header);
    std::printf("Su Mo Tu We Th Fr Sa\n");

    auto first_day = day_of_week(year, month, 1);
    auto days = days_in_month(year, month);

    // Leading spaces
    for (int i = 0; i < first_day; ++i) {
        std::printf("   ");
    }

    for (int d = 1; d <= days; ++d) {
        std::printf("%2d ", d);
        if ((d + first_day) % 7 == 0 || d == days) {
            std::printf("\n");
        }
    }
}

} // anonymous namespace

auto cal_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'3', false, "three"},
        cfbox::args::OptSpec{'y', false, "year"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    int year = tm->tm_year + 1900;
    int month = tm->tm_mon + 1;

    const auto& pos = parsed.positional();
    if (pos.size() == 1) {
        auto val = std::stoi(std::string(pos[0]));
        if (val >= 1 && val <= 12) {
            month = val;
        } else {
            year = val;
            month = 1;
        }
    } else if (pos.size() >= 2) {
        month = std::stoi(std::string(pos[0]));
        year = std::stoi(std::string(pos[1]));
    }

    bool three = parsed.has('3') || parsed.has_long("three");
    bool whole_year = parsed.has('y') || parsed.has_long("year");

    if (whole_year) {
        for (int m = 1; m <= 12; ++m) {
            print_month(year, m);
            std::printf("\n");
        }
    } else if (three) {
        // Previous month
        int pm = month - 1, py = year;
        if (pm < 1) { pm = 12; --py; }
        int nm = month + 1, ny = year;
        if (nm > 12) { nm = 1; ++ny; }

        print_month(py, pm);
        std::printf("\n");
        print_month(year, month);
        std::printf("\n");
        print_month(ny, nm);
    } else {
        print_month(year, month);
    }

    return 0;
}
