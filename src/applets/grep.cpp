// grep — search patterns in text
// Supported flags: -E -i -v -n -r -c -l -q, plus -A/-B/-C context lines.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/regex.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "grep",
    .version = CFBOX_VERSION_STRING,
    .one_line = "search patterns in text",
    .usage   = "grep [OPTIONS] PATTERN [FILE]...",
    .options = "  -E     extended regex\n"
               "  -i     ignore case\n"
               "  -v     invert match\n"
               "  -n     print line numbers\n"
               "  -r     recursive search\n"
               "  -c     print only a count of matching lines\n"
               "  -l     print only names of files with matches\n"
               "  -q     quiet mode\n"
               "  -A NUM lines of trailing context\n"
               "  -B NUM lines of leading context\n"
               "  -C NUM lines of leading and trailing context",
    .extra   = "",
};

struct GrepOptions {
    bool extended = false;
    bool ignore_case = false;
    bool invert = false;
    bool line_numbers = false;
    bool recursive = false;
    bool count_only = false;
    bool files_with_matches = false;
    bool quiet = false;
    int after = 0;
    int before = 0;
};

auto grep_file(const std::string& pattern, const GrepOptions& opts,
               std::string_view path, bool print_filename) -> int {
    int cflags = opts.extended ? REG_EXTENDED : 0;
    if (opts.ignore_case) cflags |= REG_ICASE;

    cfbox::util::scoped_regex re;
    if (re.compile(pattern.c_str(), cflags) != 0) {
        CFBOX_ERR("grep", "invalid regex: %s", pattern.c_str());
        return 2;
    }

    int match_count = 0;
    int found_any = 0;
    std::size_t line_num = 0;

    const int after = opts.after;
    const int before = opts.before;
    const bool printing = !opts.quiet && !opts.files_with_matches && !opts.count_only;

    // Context-window state. before_buf holds up to `before` recent non-matching
    // lines (flushed when a match prints them as leading context); after_pending
    // counts lines still to emit after a match; need_separator + last_printed
    // detect non-contiguous groups so a "--" separator is printed between them.
    std::vector<std::pair<std::size_t, std::string>> before_buf;
    int after_pending = 0;
    bool need_separator = false;
    std::size_t last_printed = 0;

    auto emit = [&](std::size_t ln, const std::string& content) {
        if (need_separator && ln != last_printed + 1) {
            std::printf("--\n");
        }
        if (print_filename) std::printf("%s:", path.data());
        if (opts.line_numbers) std::printf("%zu:", ln);
        std::printf("%s\n", content.c_str());
        last_printed = ln;
        need_separator = false;
    };

    auto process_line = [&](const std::string& line) -> bool {
        ++line_num;
        bool matched = re.exec(line.c_str(), 0, nullptr, 0) == 0;
        if (opts.invert) matched = !matched;

        if (matched) {
            ++match_count;
            found_any = 1;
            if (opts.quiet) return false;
            if (opts.files_with_matches) {
                std::printf("%s\n", path.data());
                return false;
            }
            if (opts.count_only) return true;

            // Leading context: flush buffered previous lines, then the match.
            for (const auto& [ln, s] : before_buf) emit(ln, s);
            before_buf.clear();
            emit(line_num, line);
            after_pending = after;
        } else if (printing) {
            if (after_pending > 0) {
                // Trailing context line after a match.
                emit(line_num, line);
                if (--after_pending == 0) need_separator = true;
            } else if (before > 0) {
                // Stash as potential leading context for a future match.
                before_buf.emplace_back(line_num, line);
                if (before_buf.size() > static_cast<std::size_t>(before)) {
                    before_buf.erase(before_buf.begin());
                }
            }
        }
        return true;
    };

    auto result = cfbox::io::for_each_line(path, process_line);
    if (!result) {
        CFBOX_ERR("grep", "%s", result.error().msg.c_str());
        return 2;
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
        cfbox::args::OptSpec{'E', false, "extended-regexp"},
        cfbox::args::OptSpec{'i', false, "ignore-case"},
        cfbox::args::OptSpec{'v', false, "invert-match"},
        cfbox::args::OptSpec{'n', false, "line-number"},
        cfbox::args::OptSpec{'r', false, "recursive"},
        cfbox::args::OptSpec{'c', false, "count"},
        cfbox::args::OptSpec{'l', false, "files-with-matches"},
        cfbox::args::OptSpec{'q', false, "quiet"},
        cfbox::args::OptSpec{'A', true, "after-context"},
        cfbox::args::OptSpec{'B', true, "before-context"},
        cfbox::args::OptSpec{'C', true, "context"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    GrepOptions opts;
    opts.extended = parsed.has('E');
    opts.ignore_case = parsed.has('i');
    opts.invert = parsed.has('v');
    opts.line_numbers = parsed.has('n');
    opts.recursive = parsed.has('r');
    opts.count_only = parsed.has('c');
    opts.files_with_matches = parsed.has('l');
    opts.quiet = parsed.has('q');

    // Context counts: -A/-B/-C take a non-negative integer; -C sets both.
    bool bad_num = false;
    auto take_num = [&](char f, const char* ln) -> int {
        auto v = parsed.get_any(f, ln);
        if (!v) return -1;  // not specified
        std::string s{*v};
        char* end = nullptr;
        long n = std::strtol(s.c_str(), &end, 10);
        if (s.empty() || *end != '\0' || n < 0) {
            CFBOX_ERR("grep", "invalid context count for -%c: '%s'", f, s.c_str());
            bad_num = true;
            return 0;
        }
        return static_cast<int>(n);
    };
    int a = take_num('A', "after-context");
    int b = take_num('B', "before-context");
    int c = take_num('C', "context");
    if (bad_num) return 2;
    if (c >= 0) { a = c; b = c; }
    opts.after = (a >= 0) ? a : 0;
    opts.before = (b >= 0) ? b : 0;

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        CFBOX_ERR("grep", "missing pattern");
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
