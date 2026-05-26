#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "sleep",
    .version = CFBOX_VERSION_STRING,
    .one_line = "delay for a specified amount of time",
    .usage   = "sleep NUMBER...",
    .options = "",
    .extra   = "NUMBER may be a fractional floating-point value. Multiple values are summed.",
};
} // namespace

auto sleep_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        CFBOX_ERR("sleep", "missing operand");
        return 1;
    }

    double total = 0.0;
    for (auto arg : pos) {
        auto s = std::string{arg};
        char* end = nullptr;
        double val = std::strtod(s.c_str(), &end);
        if (end == s.c_str() || val < 0) {
            CFBOX_ERR("sleep", "invalid time interval '%.*s'", static_cast<int>(arg.size()), arg.data());
            return 1;
        }
        total += val;
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(total));
    return 0;
}
