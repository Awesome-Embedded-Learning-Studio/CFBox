#include <cstdio>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/escape.hpp>

auto echo_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'e', false},
    });

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
