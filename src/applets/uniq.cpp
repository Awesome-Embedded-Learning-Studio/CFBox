#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/io.hpp>

namespace {

struct UniqOptions {
    bool count = false;
    bool repeated = false;
    bool unique_only = false;
};

auto uniq_lines(const std::vector<std::string>& lines, const UniqOptions& opts) -> void {
    if (lines.empty()) return;

    std::size_t i = 0;
    while (i < lines.size()) {
        std::size_t count = 1;
        while (i + count < lines.size() && lines[i] == lines[i + count]) {
            ++count;
        }

        bool is_repeated = (count > 1);

        if (opts.repeated && !is_repeated) {
            // skip: only show repeated
        } else if (opts.unique_only && is_repeated) {
            // skip: only show unique
        } else {
            if (opts.count) {
                std::printf("%7zu %s\n", count, lines[i].c_str());
            } else {
                std::printf("%s\n", lines[i].c_str());
            }
        }

        i += count;
    }
}

} // namespace

auto uniq_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'c', false},
        cfbox::args::OptSpec{'d', false},
        cfbox::args::OptSpec{'u', false},
    });

    UniqOptions opts;
    opts.count = parsed.has('c');
    opts.repeated = parsed.has('d');
    opts.unique_only = parsed.has('u');

    const auto& pos = parsed.positional();

    std::vector<std::string> lines;

    if (pos.empty()) {
        auto result = cfbox::io::read_all_stdin();
        if (!result) {
            std::fprintf(stderr, "cfbox uniq: %s\n", result.error().msg.c_str());
            return 1;
        }
        lines = cfbox::io::split_lines(result.value());
    } else {
        auto result = (pos[0] == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(pos[0]);
        if (!result) {
            std::fprintf(stderr, "cfbox uniq: %s\n", result.error().msg.c_str());
            return 1;
        }
        lines = cfbox::io::split_lines(result.value());
    }

    uniq_lines(lines, opts);
    return 0;
}
