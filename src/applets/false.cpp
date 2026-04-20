#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "false",
    .version = CFBOX_VERSION_STRING,
    .one_line = "do nothing, exit with status 1",
    .usage   = "false",
    .options = "",
    .extra   = "",
};
} // namespace

auto false_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }
    return 1;
}
