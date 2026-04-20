#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/checksum.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "sum",
    .version = CFBOX_VERSION_STRING,
    .one_line = "checksum and count the blocks in a file",
    .usage   = "sum [-s] [FILE]...",
    .options = "  -s     use System V sum algorithm (512-byte blocks)",
    .extra   = "",
};
} // namespace

auto sum_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool sysv = parsed.has('s');
    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        auto data_result = (p == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(p);
        if (!data_result) {
            std::fprintf(stderr, "cfbox sum: %s\n", data_result.error().msg.c_str());
            rc = 1;
            continue;
        }

        if (sysv) {
            auto result = cfbox::checksum::sysv_sum(*data_result);
            std::printf("%d %d", result.checksum, result.blocks);
        } else {
            auto result = cfbox::checksum::bsd_sum(*data_result);
            std::printf("%05d %5d", result.checksum, result.blocks);
        }
        if (p != "-") std::printf(" %.*s", static_cast<int>(p.size()), p.data());
        std::putchar('\n');
    }
    return rc;
}
