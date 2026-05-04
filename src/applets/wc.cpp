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

auto wc_count(std::FILE* f) -> WcCounts {
    WcCounts c;
    char buf[4096];
    bool in_word = false;
    while (auto n = std::fread(buf, 1, sizeof(buf), f)) {
        c.bytes += static_cast<long>(n);
        for (std::size_t i = 0; i < n; ++i) {
            unsigned char ch = static_cast<unsigned char>(buf[i]);
            if (ch == '\n') ++c.lines;
            if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v') {
                in_word = false;
            } else {
                if (!in_word) { ++c.words; in_word = true; }
            }
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

auto wc_file(std::string_view path) -> cfbox::base::Result<WcCounts> {
    if (path == "-") {
        return wc_count(stdin);
    }
    CFBOX_TRY(f, cfbox::io::open_file(path, "rb"));
    return wc_count(f->get());
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
        auto result = wc_file("-");
        if (!result) {
            std::fprintf(stderr, "cfbox wc: %s\n", result.error().msg.c_str());
            return 1;
        }
        print_counts(*result, show_lines, show_words, show_bytes, all);
        std::putchar('\n');
        return 0;
    }

    if (pos.size() == 1) {
        auto result = wc_file(pos[0]);
        if (!result) {
            std::fprintf(stderr, "cfbox wc: %s\n", result.error().msg.c_str());
            return 1;
        }
        print_counts(*result, show_lines, show_words, show_bytes, all);
        std::printf(" %s\n", std::string{pos[0]}.c_str());
        return 0;
    }

    // Multiple files — per-file + total
    WcCounts total;
    int rc = 0;
    for (const auto& p : pos) {
        auto result = wc_file(p);
        if (!result) {
            std::fprintf(stderr, "cfbox wc: %s\n", result.error().msg.c_str());
            rc = 1;
            continue;
        }
        total.lines += result->lines;
        total.words += result->words;
        total.bytes += result->bytes;
        print_counts(*result, show_lines, show_words, show_bytes, all);
        std::printf(" %s\n", std::string{p}.c_str());
    }

    print_counts(total, show_lines, show_words, show_bytes, all);
    std::puts(" total");
    return rc;
}
