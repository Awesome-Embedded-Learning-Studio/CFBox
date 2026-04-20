#include <cstdio>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "paste",
    .version = CFBOX_VERSION_STRING,
    .one_line = "merge lines of files",
    .usage   = "paste [-d DELIMS] [-s] [FILE]...",
    .options = "  -d DELIMS  use DELIMS instead of TABs\n"
               "  -s         paste one file at a time instead of in parallel",
    .extra   = "",
};
} // namespace

auto paste_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'d', true, "delimiters"},
        cfbox::args::OptSpec{'s', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    std::string delims = "\t";
    if (auto d = parsed.get_any('d', "delimiters")) {
        delims = std::string{*d};
    }
    bool serial = parsed.has('s');

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    auto get_delim = [&](std::size_t i) -> char {
        if (delims.empty()) return '\t';
        return delims[i % delims.size()];
    };

    if (serial) {
        for (auto p : paths) {
            bool first = true;
            std::size_t dindex = 0;
            auto result = cfbox::stream::for_each_line(p, [&](const std::string& line, std::size_t) {
                if (!first) std::putchar(get_delim(dindex++));
                std::fputs(line.c_str(), stdout);
                first = false;
                return true;
            });
            if (!result) {
                std::fprintf(stderr, "cfbox paste: %s\n", result.error().msg.c_str());
                return 1;
            }
            std::putchar('\n');
        }
        return 0;
    }

    // Parallel mode: read all lines from all files
    std::vector<std::vector<std::string>> all_lines;
    std::size_t max_lines = 0;
    for (auto p : paths) {
        std::vector<std::string> lines;
        auto result = cfbox::stream::for_each_line(p, [&](const std::string& line, std::size_t) {
            lines.push_back(line);
            return true;
        });
        if (!result) {
            std::fprintf(stderr, "cfbox paste: %s\n", result.error().msg.c_str());
            return 1;
        }
        if (lines.size() > max_lines) max_lines = lines.size();
        all_lines.push_back(std::move(lines));
    }

    for (std::size_t row = 0; row < max_lines; ++row) {
        for (std::size_t col = 0; col < all_lines.size(); ++col) {
            if (col > 0) std::putchar(get_delim(col - 1));
            if (row < all_lines[col].size()) {
                std::fputs(all_lines[col][row].c_str(), stdout);
            }
        }
        std::putchar('\n');
    }
    return 0;
}
