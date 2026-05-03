#include <cstdio>
#include <string>
#include <filesystem>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "pwdx",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print working directory of a process",
    .usage   = "pwdx PID...",
    .options = "",
    .extra   = "",
};

} // anonymous namespace

auto pwdx_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& args = parsed.positional();
    if (args.empty()) {
        std::fprintf(stderr, "cfbox pwdx: no PID specified\n");
        return 1;
    }

    int rc = 0;
    for (const auto& arg : args) {
        auto link = "/proc/" + std::string(arg) + "/cwd";
        std::error_code ec;
        auto target = std::filesystem::read_symlink(link, ec);
        if (ec) {
            std::fprintf(stderr, "cfbox pwdx: %.*s: %s\n",
                         static_cast<int>(arg.size()), arg.data(), ec.message().c_str());
            rc = 1;
        } else {
            std::printf("%.*s: %s\n",
                        static_cast<int>(arg.size()), arg.data(), target.c_str());
        }
    }
    return rc;
}
