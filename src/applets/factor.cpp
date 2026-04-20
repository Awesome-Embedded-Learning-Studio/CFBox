#include <cstdio>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "factor",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print the prime factors of numbers",
    .usage   = "factor [NUMBER]...",
    .options = "",
    .extra   = "If no NUMBER is given, read from stdin.",
};
} // namespace

static auto factor_number(unsigned long long n) -> void {
    std::printf("%llu:", n);
    for (unsigned long long d = 2; d * d <= n; ++d) {
        while (n % d == 0) {
            std::printf(" %llu", d);
            n /= d;
        }
    }
    if (n > 1) std::printf(" %llu", n);
    std::putchar('\n');
}

auto factor_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        auto input = cfbox::io::read_all_stdin();
        if (!input) {
            std::fprintf(stderr, "cfbox factor: %s\n", input.error().msg.c_str());
            return 1;
        }
        char* str = input->data();
        char* end = str + input->size();
        while (str < end) {
            char* num_end = nullptr;
            auto n = std::strtoull(str, &num_end, 10);
            if (num_end == str) { ++str; continue; }
            factor_number(n);
            str = num_end;
        }
    } else {
        for (auto p : pos) {
            auto n = std::strtoull(std::string{p}.c_str(), nullptr, 10);
            factor_number(n);
        }
    }
    return 0;
}
