#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/checksum.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "cksum",
    .version = CFBOX_VERSION_STRING,
    .one_line = "checksum and count the bytes in a file",
    .usage   = "cksum [FILE]...",
    .options = "",
    .extra   = "",
};
} // namespace

auto cksum_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        auto data_result = (p == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(p);
        if (!data_result) {
            std::fprintf(stderr, "cfbox cksum: %s\n", data_result.error().msg.c_str());
            rc = 1;
            continue;
        }
        auto crc = cfbox::checksum::crc32(*data_result);
        std::printf("%u %ju", crc, static_cast<std::uintmax_t>(data_result->size()));
        if (p != "-") std::printf(" %.*s", static_cast<int>(p.size()), p.data());
        std::putchar('\n');
    }
    return rc;
}
