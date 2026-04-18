#include <cstdio>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/io.hpp>

namespace {

auto print_visible_char(unsigned char c) -> void {
    if (c >= 128) {
        std::fputs("M-", stdout);
        c -= 128;
    }
    if (c < 32 && c != '\t') {
        std::fputc('^', stdout);
        std::fputc(c + 64, stdout);
    } else if (c == 127) {
        std::fputs("^?", stdout);
    } else {
        std::fputc(c, stdout);
    }
}

auto cat_content(const std::string& content, bool n_flag, bool b_flag, bool A_flag) -> void {
    int line_num = 1;
    bool at_line_start = true;

    for (char ch : content) {
        if (at_line_start && (n_flag || b_flag)) {
            bool non_empty = (ch != '\n');
            if (!b_flag || non_empty) {
                std::printf("%6d\t", line_num++);
            }
        }

        if (ch == '\n') {
            if (A_flag) std::fputc('$', stdout);
            std::fputc('\n', stdout);
            at_line_start = true;
        } else if (A_flag) {
            print_visible_char(static_cast<unsigned char>(ch));
            at_line_start = false;
        } else {
            std::fputc(ch, stdout);
            at_line_start = false;
        }
    }
}

auto cat_file(std::string_view path, bool n_flag, bool b_flag, bool A_flag) -> int {
    bool use_stdin = (path == "-");

    auto result = use_stdin ? cfbox::io::read_all_stdin() : cfbox::io::read_all(path);
    if (!result) {
        std::fprintf(stderr, "cfbox cat: %s\n", result.error().msg.c_str());
        return 1;
    }

    cat_content(result.value(), n_flag, b_flag, A_flag);
    return 0;
}

} // namespace

auto cat_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'b', false},
        cfbox::args::OptSpec{'A', false},
    });

    bool n_flag = parsed.has('n');
    bool b_flag = parsed.has('b');
    bool A_flag = parsed.has('A');

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        return cat_file("-", n_flag, b_flag, A_flag);
    }

    int rc = 0;
    for (const auto& p : pos) {
        if (cat_file(p, n_flag, b_flag, A_flag) != 0) rc = 1;
    }
    return rc;
}
