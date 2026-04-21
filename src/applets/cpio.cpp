#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "cpio",
    .version = CFBOX_VERSION_STRING,
    .one_line = "copy files to and from archives",
    .usage   = "cpio -o|-i|-t [-H newc]",
    .options = "  -o     copy-out mode (create archive)\n"
               "  -i     copy-in mode (extract)\n"
               "  -t     list archive contents\n"
               "  -H FMT archive format (newc, odc)",
    .extra   = "",
};

struct NewcEntry {
    unsigned long dev, ino, mode, uid, gid, nlink, rdev, mtime, filesize;
    unsigned long devmajor, devminor, rdevmajor, rdevminor;
    unsigned long namesize, check;
    std::string name;
    std::string data;
};
} // namespace

auto cpio_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'o', false},
        cfbox::args::OptSpec{'i', false},
        cfbox::args::OptSpec{'t', false},
        cfbox::args::OptSpec{'H', true, "format"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool copy_out = parsed.has('o');
    bool copy_in = parsed.has('i');
    bool list_mode = parsed.has('t');

    if (copy_out) {
        auto input = cfbox::io::read_all_stdin();
        if (!input) { std::fprintf(stderr, "cfbox cpio: read error\n"); return 1; }
        // Read filenames from stdin
        std::vector<std::string> names;
        std::string name;
        for (char c : *input) {
            if (c == '\n') { if (!name.empty()) names.push_back(std::move(name)); name.clear(); }
            else name += c;
        }
        if (!name.empty()) names.push_back(std::move(name));

        for (const auto& fname : names) {
            auto data = cfbox::io::read_all(fname);
            auto fsize = data ? data->size() : 0;
            auto namesize = fname.size() + 1;
            auto hdr_size = 110 + namesize;
            hdr_size = (hdr_size + 3) & ~3u;

            std::printf("070701%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X",
                0u, 0u, 0100644u, 0u, 0u, 1u, 0u,
                static_cast<unsigned>(fsize), 0u, 0u, 0u, 0u,
                static_cast<unsigned>(namesize), 0u);
            std::fputs(fname.c_str(), stdout);
            std::putchar('\0');
            for (auto pad = 110 + namesize; pad < hdr_size; ++pad) std::putchar('\0');

            if (data) std::fwrite(data->data(), 1, data->size(), stdout);
            auto data_pad = (fsize + 3) & ~3u;
            for (auto p = fsize; p < data_pad; ++p) std::putchar('\0');
        }
        // Trailer
        std::printf("070701%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X%08X",
            0u, 0u, 0u, 0u, 0u, 1u, 0u, 0u, 0u, 0u, 0u, 0u, 13u, 0u);
        std::fputs("TRAILER!!!\0", stdout);
        for (int pad = 110 + 13; pad < ((110 + 13 + 3) & ~3); ++pad) std::putchar('\0');
        return 0;
    }

    if (copy_in || list_mode) {
        auto input = cfbox::io::read_all_stdin();
        if (!input) { std::fprintf(stderr, "cfbox cpio: read error\n"); return 1; }
        const auto& data = *input;
        std::size_t offset = 0;
        while (offset + 110 < data.size()) {
            if (data.substr(offset, 6) != "070701") break;
            auto parse_hex = [&](std::size_t pos, int len) -> unsigned long {
                return std::strtoul(data.substr(offset + pos, len).c_str(), nullptr, 16);
            };
            auto namesize = parse_hex(102, 8);
            auto filesize = parse_hex(54, 8);
            auto hdr_size = (110 + namesize + 3) & ~3u;
            auto name = data.substr(offset + 110, namesize - 1);
            if (name == "TRAILER!!!") break;
            if (list_mode) {
                std::puts(name.c_str());
            } else {
                auto content = data.substr(offset + hdr_size, filesize);
                auto wresult = cfbox::io::write_all(name, content);
                if (!wresult) std::fprintf(stderr, "cfbox cpio: %s\n", wresult.error().msg.c_str());
            }
            auto data_padded = (filesize + 3) & ~3u;
            offset += hdr_size + data_padded;
        }
        return 0;
    }

    std::fprintf(stderr, "cfbox cpio: must specify -o, -i, or -t\n");
    return 1;
}
