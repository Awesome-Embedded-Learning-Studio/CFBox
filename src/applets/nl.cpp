#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "nl",
    .version = CFBOX_VERSION_STRING,
    .one_line = "number lines of files",
    .usage   = "nl [-b STYLE] [-n FORMAT] [-s SEP] [FILE]...",
    .options = "  -b STYLE  body numbering style: a(all), t(non-empty), n(none)\n"
               "  -n FORMAT line number format: ln, rn, rz\n"
               "  -s SEP    add SEP after line number (default: TAB)",
    .extra   = "",
};
} // namespace

auto nl_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'b', true, "body-numbering"},
        cfbox::args::OptSpec{'n', true, "number-format"},
        cfbox::args::OptSpec{'s', true, "number-separator"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    char body_style = 't';
    if (auto b = parsed.get_any('b', "body-numbering")) {
        body_style = (*b)[0];
    }

    std::string num_fmt = "rn";
    if (auto n = parsed.get_any('n', "number-format")) {
        num_fmt = std::string{*n};
    }

    std::string sep = "\t";
    if (auto s = parsed.get_any('s', "number-separator")) {
        sep = std::string{*s};
    }

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        int line_num = 0;
        auto result = cfbox::stream::for_each_line(p, [&](const std::string& line, std::size_t) {
            bool should_number = false;
            switch (body_style) {
                case 'a': should_number = true; break;
                case 't': should_number = !line.empty(); break;
                case 'n': should_number = false; break;
                default:  should_number = !line.empty(); break;
            }

            if (should_number) {
                ++line_num;
                char numbuf[16];
                if (num_fmt == "ln") {
                    std::snprintf(numbuf, sizeof(numbuf), "%-6d", line_num);
                } else if (num_fmt == "rz") {
                    std::snprintf(numbuf, sizeof(numbuf), "%06d", line_num);
                } else {
                    std::snprintf(numbuf, sizeof(numbuf), "%6d", line_num);
                }
                std::printf("%s%s%s\n", numbuf, sep.c_str(), line.c_str());
            } else {
                std::printf("      %s%s\n", sep.c_str(), line.c_str());
            }
            return true;
        });
        if (!result) {
            std::fprintf(stderr, "cfbox nl: %s\n", result.error().msg.c_str());
            rc = 1;
        }
    }
    return rc;
}
