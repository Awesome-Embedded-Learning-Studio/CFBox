#include <cstdio>
#include <string_view>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "wc",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print newline, word, and byte counts",
    .usage   = "wc [OPTIONS] [FILE]...",
    .options = "  -l     print newline counts\n"
               "  -w     print word counts\n"
               "  -c     print byte counts\n"
               "  -m     print character counts (alias for -c)",
    .extra   = "",
};

struct WcCounts {
    long lines = 0;
    long words = 0;
    long bytes = 0;
};

auto count_content(const std::string& content) -> WcCounts {
    WcCounts c;
    c.bytes = static_cast<long>(content.size());

    bool in_word = false;
    for (char ch : content) {
        if (ch == '\n') ++c.lines;
        if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
            in_word = false;
        } else {
            if (!in_word) { ++c.words; in_word = true; }
        }
    }
    return c;
}

auto print_counts(const WcCounts& c, bool show_lines, bool show_words,
                  bool show_bytes, bool all) -> void {
    if (all || show_lines)  std::printf("%8ld", c.lines);
    if (all || show_words)  std::printf("%8ld", c.words);
    if (all || show_bytes)  std::printf("%8ld", c.bytes);
}

auto read_source(std::string_view path) -> cfbox::base::Result<std::string> {
    return (path == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(path);
}

} // namespace

auto wc_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'l', false},
        cfbox::args::OptSpec{'w', false},
        cfbox::args::OptSpec{'c', false},
        cfbox::args::OptSpec{'m', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool show_lines = parsed.has('l');
    bool show_words = parsed.has('w');
    bool show_bytes = parsed.has('c');
    bool show_chars = parsed.has('m');
    bool all = !show_lines && !show_words && !show_bytes && !show_chars;

    // -m is an alias for -c in this simplified impl (no multibyte)
    if (show_chars) show_bytes = true;

    const auto& pos = parsed.positional();

    if (pos.empty()) {
        auto result = read_source("-");
        if (!result) {
            std::fprintf(stderr, "cfbox wc: %s\n", result.error().msg.c_str());
            return 1;
        }
        auto c = count_content(result.value());
        print_counts(c, show_lines, show_words, show_bytes, all);
        std::putchar('\n');
        return 0;
    }

    if (pos.size() == 1) {
        auto result = read_source(pos[0]);
        if (!result) {
            std::fprintf(stderr, "cfbox wc: %s\n", result.error().msg.c_str());
            return 1;
        }
        auto c = count_content(result.value());
        print_counts(c, show_lines, show_words, show_bytes, all);
        std::printf(" %s\n", std::string{pos[0]}.c_str());
        return 0;
    }

    // Multiple files — per-file + total
    WcCounts total;
    int rc = 0;
    for (const auto& p : pos) {
        auto result = read_source(p);
        if (!result) {
            std::fprintf(stderr, "cfbox wc: %s\n", result.error().msg.c_str());
            rc = 1;
            continue;
        }
        auto c = count_content(result.value());
        total.lines += c.lines;
        total.words += c.words;
        total.bytes += c.bytes;
        print_counts(c, show_lines, show_words, show_bytes, all);
        std::printf(" %s\n", std::string{p}.c_str());
    }

    print_counts(total, show_lines, show_words, show_bytes, all);
    std::puts(" total");
    return rc;
}
