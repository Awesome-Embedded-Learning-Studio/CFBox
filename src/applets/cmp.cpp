#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/error.hpp>

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
        CFBOX_ERR("cmp", "missing operand");
        return 2;
    }

    auto f1 = cfbox::io::open_file(std::string{pos[0]}, "rb");
    if (!f1) { CFBOX_ERR("cmp", "%s", f1.error().msg.c_str()); return 2; }
    auto f2 = cfbox::io::open_file(std::string{pos[1]}, "rb");
    if (!f2) { CFBOX_ERR("cmp", "%s", f2.error().msg.c_str()); return 2; }

    // Stream both in lockstep: O(1) memory, and stop at the first differing
    // byte instead of reading both files entirely first.
    constexpr std::size_t kChunk = 65536;
    std::vector<std::uint8_t> buf1(kChunk);
    std::vector<std::uint8_t> buf2(kChunk);
    std::size_t base = 0;   // byte offset of the current block
    std::size_t lines = 0;  // newlines seen before the current scan position

    for (;;) {
        std::size_t n1 = std::fread(buf1.data(), 1, kChunk, f1->get());
        std::size_t n2 = std::fread(buf2.data(), 1, kChunk, f2->get());
        std::size_t n = std::min(n1, n2);
        for (std::size_t i = 0; i < n; ++i) {
            if (buf1[i] != buf2[i]) {
                std::printf("%.*s %.*s differ: byte %zu, line %zu\n",
                            static_cast<int>(pos[0].size()), pos[0].data(),
                            static_cast<int>(pos[1].size()), pos[1].data(),
                            base + i + 1, lines + 1);
                return 1;
            }
            if (buf1[i] == '\n') ++lines;
        }
        if (n1 != n2) {
            // common prefix matched but one file is shorter
            std::printf("cmp: EOF on %s\n", n1 < n2 ? pos[0].data() : pos[1].data());
            return 1;
        }
        if (n1 < kChunk) break;  // both at EOF, files identical
        base += n;
    }
    return 0;
}
