#include <cstdio>
#include <cstring>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "od",
    .version = CFBOX_VERSION_STRING,
    .one_line = "dump files in octal and other formats",
    .usage   = "od [-A RADIX] [-t TYPE] [FILE]",
    .options = "  -A RADIX  address radix: d(ecimal), o(ctal), x(hex), n(one)\n"
               "  -t TYPE   output type: o(ctal), x(hex), d(ecimal), u(unsigned), c(har)",
    .extra   = "",
};

static auto print_char(unsigned char c) -> void {
    if (c >= 32 && c < 127) {
        std::printf("   %c", c);
    } else {
        switch (c) {
            case '\0': std::printf("  \\0"); break;
            case '\a': std::printf("  \\a"); break;
            case '\b': std::printf("  \\b"); break;
            case '\f': std::printf("  \\f"); break;
            case '\n': std::printf("  \\n"); break;
            case '\r': std::printf("  \\r"); break;
            case '\t': std::printf("  \\t"); break;
            case '\v': std::printf("  \\v"); break;
            default:   std::printf("%03o", c); break;
        }
    }
}
} // namespace

auto od_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'A', true, "address-radix"},
        cfbox::args::OptSpec{'t', true, "format"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    char addr_radix = 'o';
    if (auto a = parsed.get_any('A', "address-radix")) {
        addr_radix = (*a)[0];
    }

    char out_type = 'o';
    if (auto t = parsed.get_any('t', "format")) {
        out_type = (*t)[0];
    }

    const auto& pos = parsed.positional();
    auto data_result = pos.empty() ? cfbox::io::read_all_stdin() : cfbox::io::read_all(pos[0]);
    if (!data_result) {
        std::fprintf(stderr, "cfbox od: %s\n", data_result.error().msg.c_str());
        return 1;
    }

    const auto& data = *data_result;
    for (std::size_t offset = 0; offset < data.size(); offset += 16) {
        // Print address
        switch (addr_radix) {
            case 'd': std::printf("%07zu", offset); break;
            case 'x': std::printf("%07zx", offset); break;
            case 'n': break;
            default:  std::printf("%07zo", offset); break;
        }

        auto line_len = static_cast<int>(data.size() - offset);
        if (line_len > 16) line_len = 16;

        if (out_type == 'c') {
            for (int i = 0; i < line_len; ++i) {
                print_char(static_cast<unsigned char>(data[offset + i]));
            }
        } else {
            for (int i = 0; i < line_len; ++i) {
                auto b = static_cast<unsigned char>(data[offset + i]);
                switch (out_type) {
                    case 'x': std::printf(" %02x", b); break;
                    case 'd': std::printf(" %4d", static_cast<signed char>(b)); break;
                    case 'u': std::printf(" %3u", b); break;
                    default:  std::printf(" %03o", b); break;
                }
            }
        }
        std::putchar('\n');
    }
    return 0;
}
