#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/checksum.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "md5sum",
    .version = CFBOX_VERSION_STRING,
    .one_line = "compute and check MD5 message digest",
    .usage   = "md5sum [FILE]...",
    .options = "",
    .extra   = "",
};
} // namespace

auto md5sum_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        auto data_result = (p == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(p);
        if (!data_result) {
            std::fprintf(stderr, "cfbox md5sum: %s\n", data_result.error().msg.c_str());
            rc = 1;
            continue;
        }
        auto hash = cfbox::checksum::md5(*data_result);
        auto hex = cfbox::checksum::md5_to_hex(hash);
        std::printf("%s  ", hex.c_str());
        if (p == "-") {
            std::puts("-");
        } else {
            std::printf("%.*s\n", static_cast<int>(p.size()), p.data());
        }
    }
    return rc;
}
