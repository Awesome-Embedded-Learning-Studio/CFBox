#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "fold",
    .version = CFBOX_VERSION_STRING,
    .one_line = "wrap each input line to fit in specified width",
    .usage   = "fold [-w WIDTH] [-s] [FILE]...",
    .options = "  -w WIDTH  use WIDTH columns instead of 80\n"
               "  -s        break at spaces",
    .extra   = "",
};
} // namespace

auto fold_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'w', true, "width"},
        cfbox::args::OptSpec{'s', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int width = 80;
    if (auto w = parsed.get_any('w', "width")) {
        width = std::stoi(std::string{*w});
        if (width <= 0) {
            std::fprintf(stderr, "cfbox fold: invalid width: %d\n", width);
            return 1;
        }
    }
    bool break_spaces = parsed.has('s');

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        auto result = cfbox::stream::for_each_line(p, [&](const std::string& line, std::size_t) {
            std::size_t col = 0;
            std::size_t last_space = 0;
            std::string segment;

            for (std::size_t i = 0; i < line.size(); ++i) {
                if (col >= static_cast<std::size_t>(width)) {
                    if (break_spaces && last_space > 0) {
                        // Break at last space
                        auto break_pos = last_space;
                        std::string before = line.substr(i - col, break_pos - (i - col));
                        std::puts(before.c_str());
                        col = i - break_pos;
                        last_space = 0;
                    } else {
                        std::string before = line.substr(i - col, col);
                        std::puts(before.c_str());
                        col = 0;
                    }
                }
                if (line[i] == ' ' || line[i] == '\t') {
                    last_space = col + 1;
                }
                ++col;
            }
            // Print remaining
            if (col > 0) {
                auto start = line.size() - col;
                std::puts(line.substr(start).c_str());
            } else {
                std::putchar('\n');
            }
            return true;
        });
        if (!result) {
            std::fprintf(stderr, "cfbox fold: %s\n", result.error().msg.c_str());
            rc = 1;
        }
    }
    return rc;
}
