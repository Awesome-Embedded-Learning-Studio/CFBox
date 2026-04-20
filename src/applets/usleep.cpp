#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "usleep",
    .version = CFBOX_VERSION_STRING,
    .one_line = "sleep for a specified number of microseconds",
    .usage   = "usleep MICROSECONDS",
    .options = "",
    .extra   = "",
};
} // namespace

auto usleep_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox usleep: missing operand\n");
        return 1;
    }

    char* end = nullptr;
    long usecs = std::strtol(std::string{pos[0]}.c_str(), &end, 10);
    if (end == std::string{pos[0]}.c_str() || usecs < 0) {
        std::fprintf(stderr, "cfbox usleep: invalid time interval '%.*s'\n",
                     static_cast<int>(pos[0].size()), pos[0].data());
        return 1;
    }

    std::this_thread::sleep_for(std::chrono::microseconds(usecs));
    return 0;
}
