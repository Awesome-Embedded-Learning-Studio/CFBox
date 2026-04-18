#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/io.hpp>

namespace {

struct SortOptions {
    bool reverse = false;
    bool numeric = false;
    bool unique = false;
    int key_field = 0; // 0 = whole line, N = field N (1-based)
};

auto extract_field(const std::string& line, int field) -> std::string {
    if (field <= 0) return line;
    int current = 1;
    std::size_t start = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == ' ' || line[i] == '\t') {
            if (start < i) {
                if (current == field) {
                    return line.substr(start, i - start);
                }
                ++current;
            }
            start = i + 1;
        }
    }
    return "";
}

auto sort_lines(std::vector<std::string>& lines, const SortOptions& opts) -> void {
    auto make_key = [&](const std::string& line) -> std::string {
        return opts.key_field > 0 ? extract_field(line, opts.key_field) : line;
    };

    std::stable_sort(lines.begin(), lines.end(), [&](const std::string& a, const std::string& b) {
        std::string ka = make_key(a);
        std::string kb = make_key(b);

        bool less;
        if (opts.numeric) {
            double da = std::strtod(ka.c_str(), nullptr);
            double db = std::strtod(kb.c_str(), nullptr);
            less = da < db;
        } else {
            less = ka < kb;
        }
        return opts.reverse ? !less && ka != kb : less;
    });

    if (opts.unique) {
        auto it = std::unique(lines.begin(), lines.end());
        lines.erase(it, lines.end());
    }
}

} // namespace

auto sort_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'r', false},
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'u', false},
        cfbox::args::OptSpec{'k', true},
    });

    SortOptions opts;
    opts.reverse = parsed.has('r');
    opts.numeric = parsed.has('n');
    opts.unique = parsed.has('u');

    if (parsed.has('k')) {
        opts.key_field = static_cast<int>(
            std::strtol(std::string{parsed.get('k').value_or("1")}.c_str(), nullptr, 10));
    }

    const auto& pos = parsed.positional();

    std::vector<std::string> all_lines;

    if (pos.empty()) {
        auto result = cfbox::io::read_all_stdin();
        if (!result) {
            std::fprintf(stderr, "cfbox sort: %s\n", result.error().msg.c_str());
            return 1;
        }
        all_lines = cfbox::io::split_lines(result.value());
    } else {
        int rc = 0;
        for (const auto& p : pos) {
            auto result = (p == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(p);
            if (!result) {
                std::fprintf(stderr, "cfbox sort: %s\n", result.error().msg.c_str());
                rc = 1;
                continue;
            }
            auto file_lines = cfbox::io::split_lines(result.value());
            for (auto& l : file_lines) {
                all_lines.push_back(std::move(l));
            }
        }
        if (rc != 0) return rc;
    }

    sort_lines(all_lines, opts);

    for (const auto& line : all_lines) {
        std::printf("%s\n", line.c_str());
    }

    return 0;
}
