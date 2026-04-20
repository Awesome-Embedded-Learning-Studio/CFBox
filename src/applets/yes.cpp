#include <cstdio>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "yes",
    .version = CFBOX_VERSION_STRING,
    .one_line = "output a string repeatedly until killed",
    .usage   = "yes [STRING]...",
    .options = "",
    .extra   = "Repeatedly prints 'y' (or the given STRING) until killed.",
};
} // namespace

auto yes_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    while (true) {
        if (!pos.empty()) {
            for (std::size_t i = 0; i < pos.size(); ++i) {
                if (i > 0) std::fputc(' ', stdout);
                std::fwrite(pos[i].data(), 1, pos[i].size(), stdout);
            }
        } else {
            std::fputc('y', stdout);
        }
        std::fputc('\n', stdout);
        if (std::ferror(stdout)) return 1;
    }
}
