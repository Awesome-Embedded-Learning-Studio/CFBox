#include <cstdio>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/escape.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "echo",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display a line of text",
    .usage   = "echo [OPTIONS] [STRING]...",
    .options = "  -n     do not output the trailing newline\n"
               "  -e     enable interpretation of backslash escapes",
    .extra   = "",
};
} // namespace

auto echo_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'e', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool no_newline = parsed.has('n');
    bool interpret = parsed.has('e');

    const auto& pos = parsed.positional();
    for (std::size_t i = 0; i < pos.size(); ++i) {
        if (i > 0) std::fputc(' ', stdout);
        if (interpret) {
            std::fputs(cfbox::util::process_escape(pos[i]).c_str(), stdout);
        } else {
            std::fwrite(pos[i].data(), 1, pos[i].size(), stdout);
        }
    }

    if (!no_newline) std::fputc('\n', stdout);
    return 0;
}
