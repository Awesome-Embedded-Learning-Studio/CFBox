#include <algorithm>
#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "cmp",
    .version = CFBOX_VERSION_STRING,
    .one_line = "compare two files byte by byte",
    .usage   = "cmp FILE1 FILE2",
    .options = "",
    .extra   = "",
};
} // namespace

auto cmp_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox cmp: missing operand\n");
        return 2;
    }

    auto a_result = cfbox::io::read_all(std::string{pos[0]});
    auto b_result = cfbox::io::read_all(std::string{pos[1]});
    if (!a_result) { std::fprintf(stderr, "cfbox cmp: %s\n", a_result.error().msg.c_str()); return 2; }
    if (!b_result) { std::fprintf(stderr, "cfbox cmp: %s\n", b_result.error().msg.c_str()); return 2; }

    const auto& a = *a_result;
    const auto& b = *b_result;
    auto min_len = std::min(a.size(), b.size());

    for (std::size_t i = 0; i < min_len; ++i) {
        if (a[i] != b[i]) {
            std::printf("%.*s %.*s differ: byte %zu, line %zu\n",
                        static_cast<int>(pos[0].size()), pos[0].data(),
                        static_cast<int>(pos[1].size()), pos[1].data(),
                        i + 1, static_cast<std::size_t>(std::count(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(i), '\n')) + 1);
            return 1;
        }
    }
    if (a.size() != b.size()) {
        std::printf("cmp: EOF on %s\n", a.size() < b.size() ? pos[0].data() : pos[1].data());
        return 1;
    }
    return 0;
}
