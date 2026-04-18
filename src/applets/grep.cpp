// grep — search patterns in text
// Supported flags: -E (extended regex), -i (ignore case), -v (invert match),
//                  -n (line numbers), -r (recursive), -c (count only),
//                  -l (files with matches), -q (quiet)
// Known differences from GNU grep: uses std::regex (slower on large files),
// no PCRE2, no color, no context lines.

#include <cstdio>
#include <filesystem>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/io.hpp>

namespace {

struct GrepOptions {
    bool extended = false;
    bool ignore_case = false;
    bool invert = false;
    bool line_numbers = false;
    bool recursive = false;
    bool count_only = false;
    bool files_with_matches = false;
    bool quiet = false;
};

auto grep_file(const std::string& pattern, const GrepOptions& opts,
               std::string_view path, bool print_filename) -> int {
    auto result = (path == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(path);
    if (!result) {
        std::fprintf(stderr, "cfbox grep: %s\n", result.error().msg.c_str());
        return 2;
    }

    auto lines = cfbox::io::split_lines(result.value());

    auto flags = std::regex::ECMAScript;
    if (opts.extended) flags = std::regex::egrep;
    if (opts.ignore_case) flags |= std::regex::icase;

    std::regex re;
    try {
        re = std::regex(pattern, flags);
    } catch (const std::regex_error& e) {
        std::fprintf(stderr, "cfbox grep: invalid regex: %s\n", e.what());
        return 2;
    }

    int match_count = 0;
    int found_any = 0;

    for (std::size_t i = 0; i < lines.size(); ++i) {
        const auto& line = lines[i];
        bool matched = std::regex_search(line, re);
        if (opts.invert) matched = !matched;

        if (matched) {
            ++match_count;
            found_any = 1;

            if (opts.quiet) return 0;
            if (opts.files_with_matches) {
                std::printf("%s\n", std::string{path}.c_str());
                return 0;
            }
            if (!opts.count_only) {
                if (print_filename) {
                    std::printf("%s:", std::string{path}.c_str());
                }
                if (opts.line_numbers) {
                    std::printf("%zu:", i + 1);
                }
                std::printf("%s\n", line.c_str());
            }
        }
    }

    if (opts.count_only) {
        if (print_filename) {
            std::printf("%s:", std::string{path}.c_str());
        }
        std::printf("%d\n", match_count);
    }

    return found_any ? 0 : 1;
}

auto grep_recursive(const std::string& pattern, const GrepOptions& opts,
                    const std::filesystem::path& dir) -> int {
    int final_rc = 1;
    std::error_code ec;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(dir, ec)) {
        if (ec) continue;
        if (!entry.is_regular_file()) continue;
        int rc = grep_file(pattern, opts, entry.path().string(), true);
        if (rc == 2) return 2;
        if (rc == 0) final_rc = 0;
    }
    return final_rc;
}

} // namespace

auto grep_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'E', false},
        cfbox::args::OptSpec{'i', false},
        cfbox::args::OptSpec{'v', false},
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'r', false},
        cfbox::args::OptSpec{'c', false},
        cfbox::args::OptSpec{'l', false},
        cfbox::args::OptSpec{'q', false},
    });

    GrepOptions opts;
    opts.extended = parsed.has('E');
    opts.ignore_case = parsed.has('i');
    opts.invert = parsed.has('v');
    opts.line_numbers = parsed.has('n');
    opts.recursive = parsed.has('r');
    opts.count_only = parsed.has('c');
    opts.files_with_matches = parsed.has('l');
    opts.quiet = parsed.has('q');

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox grep: missing pattern\n");
        return 2;
    }

    std::string pattern{pos[0]};

    if (opts.recursive) {
        if (pos.size() < 2) {
            return grep_recursive(pattern, opts, std::filesystem::current_path());
        }
        int final_rc = 1;
        for (std::size_t i = 1; i < pos.size(); ++i) {
            int rc = grep_recursive(pattern, opts, std::filesystem::path{pos[i]});
            if (rc == 2) return 2;
            if (rc == 0) final_rc = 0;
        }
        return final_rc;
    }

    if (pos.size() < 2) {
        // grep PATTERN (from stdin)
        return grep_file(pattern, opts, "-", false);
    }

    bool multi = pos.size() > 2;
    int final_rc = 1;
    for (std::size_t i = 1; i < pos.size(); ++i) {
        int rc = grep_file(pattern, opts, std::string{pos[i]}, multi);
        if (rc == 2) return 2;
        if (rc == 0) final_rc = 0;
    }
    return final_rc;
}
