#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/checksum.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/error.hpp>

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

    constexpr std::size_t kChunk = 65536;
    int rc = 0;
    for (auto p : paths) {
        cfbox::checksum::MD5 md5;
        std::vector<std::uint8_t> buf(kChunk);
        bool ok = false;
        if (p == "-") {
            ok = true;
            while (std::size_t n = std::fread(buf.data(),1, kChunk, stdin)) {
                md5.update(buf.data(), n);
            }
            if (std::ferror(stdin)) ok = false;
        } else {
            auto fh = cfbox::io::open_file(p, "rb");
            if (!fh) {
                CFBOX_ERR("md5sum", "%s", fh.error().msg.c_str());
                rc = 1;
                continue;
            }
            ok = true;
            while (std::size_t n = std::fread(buf.data(),1, kChunk, fh->get())) {
                md5.update(buf.data(), n);
            }
            if (std::ferror(fh->get())) ok = false;
        }
        if (!ok) {
            CFBOX_ERR("md5sum", "%.*s: read error", static_cast<int>(p.size()), p.data());
            rc = 1;
            continue;
        }
        auto hex = cfbox::checksum::md5_to_hex(md5.finalize());
        std::printf("%s  ", hex.c_str());
        if (p == "-") {
            std::puts("-");
        } else {
            std::printf("%.*s\n", static_cast<int>(p.size()), p.data());
        }
    }
    return rc;
}
