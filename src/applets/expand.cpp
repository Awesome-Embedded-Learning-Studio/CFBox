#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "expand",
    .version = CFBOX_VERSION_STRING,
    .one_line = "convert tabs to spaces",
    .usage   = "expand [-t N] [FILE]...",
    .options = "  -t N   have tabs N characters apart (default 8)",
    .extra   = "",
};
} // namespace

auto expand_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'t', true, "tabs"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int tab_stop = 8;
    if (auto t = parsed.get_any('t', "tabs")) {
        tab_stop = std::stoi(std::string{*t});
        if (tab_stop <= 0) {
            std::fprintf(stderr, "cfbox expand: invalid tab stop: %d\n", tab_stop);
            return 1;
        }
    }

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        auto result = cfbox::stream::for_each_line(p, [&](const std::string& line, std::size_t) {
            int col = 0;
            for (char c : line) {
                if (c == '\t') {
                    int spaces = tab_stop - (col % tab_stop);
                    for (int i = 0; i < spaces; ++i) {
                        std::putchar(' ');
                    }
                    col += spaces;
                } else {
                    std::putchar(c);
                    ++col;
                }
            }
            std::putchar('\n');
            return true;
        });
        if (!result) {
            std::fprintf(stderr, "cfbox expand: %s\n", result.error().msg.c_str());
            rc = 1;
        }
    }
    return rc;
}
