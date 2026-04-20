#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "shuf",
    .version = CFBOX_VERSION_STRING,
    .one_line = "generate random permutations",
    .usage   = "shuf [-n COUNT] [-e] [FILE]",
    .options = "  -n COUNT  output at most COUNT lines\n"
               "  -e        treat each argument as an input line",
    .extra   = "",
};
} // namespace

auto shuf_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true, "head-count"},
        cfbox::args::OptSpec{'e', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int max_count = -1;
    if (auto n = parsed.get_any('n', "head-count")) {
        max_count = std::stoi(std::string{*n});
    }
    bool echo_mode = parsed.has('e');

    std::vector<std::string> lines;
    if (echo_mode) {
        for (auto p : parsed.positional()) {
            lines.emplace_back(p);
        }
    } else {
        const auto& pos = parsed.positional();
        auto path = pos.empty() ? std::string_view{"-"} : pos[0];
        auto result = cfbox::stream::for_each_line(path, [&](const std::string& line, std::size_t) {
            lines.push_back(line);
            return true;
        });
        if (!result) {
            std::fprintf(stderr, "cfbox shuf: %s\n", result.error().msg.c_str());
            return 1;
        }
    }

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(lines.begin(), lines.end(), gen);

    int count = 0;
    for (const auto& line : lines) {
        std::puts(line.c_str());
        ++count;
        if (max_count > 0 && count >= max_count) break;
    }
    return 0;
}
