#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <string_view>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "hexdump",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display file contents in hexadecimal",
    .usage   = "hexdump [-C] [-n LENGTH] [-s OFFSET] [FILE]",
    .options = "  -C     canonical hex+ASCII display\n"
               "  -n N   interpret only N bytes\n"
               "  -s N   skip first N bytes",
    .extra   = "",
};

auto hexdump_canonical(const unsigned char* data, std::size_t len,
                       std::uint64_t base_offset) -> void {
    for (std::size_t i = 0; i < len; i += 16) {
        auto chunk = static_cast<int>(len - i > 16 ? 16 : len - i);

        // Offset
        std::printf("%08llx  ", static_cast<unsigned long long>(base_offset + i));

        // Hex bytes (two groups of 8)
        for (int j = 0; j < 16; ++j) {
            if (j == 8) std::printf(" ");
            if (j < chunk) {
                std::printf("%02x ", data[i + static_cast<size_t>(j)]);
            } else {
                std::printf("   ");
            }
        }

        // ASCII
        std::printf(" |");
        for (int j = 0; j < chunk; ++j) {
            auto c = data[i + static_cast<size_t>(j)];
            std::putchar(static_cast<int>(c >= 32 && c < 127 ? c : '.'));
        }
        std::printf("|\n");
    }
}

} // anonymous namespace

auto hexdump_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'C', false, "canonical"},
        cfbox::args::OptSpec{'n', true, "length"},
        cfbox::args::OptSpec{'s', true, "skip"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool canonical = parsed.has('C') || parsed.has_long("canonical");
    std::size_t max_bytes = 0;
    std::uint64_t skip_bytes = 0;
    if (auto v = parsed.get('n')) max_bytes = static_cast<std::size_t>(std::stoull(std::string(*v)));
    if (auto v = parsed.get('s')) skip_bytes = std::stoull(std::string(*v));

    const auto& pos = parsed.positional();
    std::string filename = pos.empty() ? "" : std::string(pos[0]);

    auto do_dump = [&](std::FILE* f) -> int {
        if (skip_bytes > 0) {
            std::fseek(f, static_cast<long>(skip_bytes), SEEK_SET);
        }

        unsigned char buf[16];
        std::uint64_t offset = skip_bytes;
        std::size_t total = 0;

        while (true) {
            auto to_read = static_cast<std::size_t>(16);
            if (max_bytes > 0 && total + to_read > max_bytes) {
                to_read = max_bytes - total;
            }
            if (to_read == 0) break;

            auto n = std::fread(buf, 1, to_read, f);
            if (n == 0) break;

            if (canonical) {
                hexdump_canonical(buf, n, offset);
            } else {
                // Default format: similar but without ASCII
                for (std::size_t i = 0; i < n; i += 16) {
                    auto chunk = static_cast<int>(n - i > 16 ? 16 : n - i);
                    std::printf("%08llx  ", static_cast<unsigned long long>(offset + i));
                    for (int j = 0; j < chunk; ++j) {
                        std::printf("%02x ", buf[i + static_cast<size_t>(j)]);
                    }
                    std::printf("\n");
                }
            }

            offset += n;
            total += n;
        }
        return 0;
    };

    if (filename.empty() || filename == "-") {
        return do_dump(stdin);
    }

    std::FILE* f = std::fopen(filename.c_str(), "rb");
    if (!f) {
        std::fprintf(stderr, "cfbox hexdump: cannot open %s\n", filename.c_str());
        return 1;
    }
    auto rc = do_dump(f);
    std::fclose(f);
    return rc;
}
