#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "seq",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print a sequence of numbers",
    .usage   = "seq [-s SEP] [-w] [FIRST [INCREMENT]] LAST",
    .options = "  -s SEP   use SEP to separate numbers (default: \\n)\n"
               "  -w       equalize widths by padding with leading zeros",
    .extra   = "",
};
} // namespace

auto seq_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', true, "separator"},
        cfbox::args::OptSpec{'w', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    std::string sep = "\n";
    if (auto s = parsed.get_any('s', "separator")) {
        sep = std::string{*s};
    }
    bool equal_width = parsed.has('w');

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox seq: missing operand\n");
        return 1;
    }

    double first = 1, incr = 1, last;
    if (pos.size() == 1) {
        last = std::stod(std::string{pos[0]});
    } else if (pos.size() == 2) {
        first = std::stod(std::string{pos[0]});
        last = std::stod(std::string{pos[1]});
    } else {
        first = std::stod(std::string{pos[0]});
        incr = std::stod(std::string{pos[1]});
        last = std::stod(std::string{pos[2]});
    }

    if (incr == 0) {
        std::fprintf(stderr, "cfbox seq: zero increment\n");
        return 1;
    }

    // Determine width for -w
    int width = 0;
    if (equal_width) {
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%g", last);
        width = static_cast<int>(std::strlen(buf));
        std::snprintf(buf, sizeof(buf), "%g", first);
        auto w2 = static_cast<int>(std::strlen(buf));
        if (w2 > width) width = w2;
        std::snprintf(buf, sizeof(buf), "%g", incr);
        w2 = static_cast<int>(std::strlen(buf));
        if (w2 > width) width = w2;
    }

    bool first_out = true;
    if (incr > 0) {
        for (double v = first; v <= last + 1e-10; v += incr) {
            if (!first_out) std::fputs(sep.c_str(), stdout);
            if (equal_width) {
                std::printf("%0*g", width, v);
            } else {
                std::printf("%g", v);
            }
            first_out = false;
        }
    } else {
        for (double v = first; v >= last - 1e-10; v += incr) {
            if (!first_out) std::fputs(sep.c_str(), stdout);
            if (equal_width) {
                std::printf("%0*g", width, v);
            } else {
                std::printf("%g", v);
            }
            first_out = false;
        }
    }
    if (!first_out) std::putchar('\n');
    return 0;
}
