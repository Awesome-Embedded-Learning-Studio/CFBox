#include <cstdio>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "comm",
    .version = CFBOX_VERSION_STRING,
    .one_line = "compare two sorted files line by line",
    .usage   = "comm [-123] FILE1 FILE2",
    .options = "  -1     suppress column 1 (lines unique to FILE1)\n"
               "  -2     suppress column 2 (lines unique to FILE2)\n"
               "  -3     suppress column 3 (lines common to both)",
    .extra   = "",
};
} // namespace

auto comm_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'1', false},
        cfbox::args::OptSpec{'2', false},
        cfbox::args::OptSpec{'3', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool suppress1 = parsed.has('1');
    bool suppress2 = parsed.has('2');
    bool suppress3 = parsed.has('3');

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox comm: missing operand\n");
        return 1;
    }

    auto lines1_result = cfbox::io::read_lines(std::string{pos[0]});
    auto lines2_result = cfbox::io::read_lines(std::string{pos[1]});
    if (!lines1_result) {
        std::fprintf(stderr, "cfbox comm: %s\n", lines1_result.error().msg.c_str());
        return 1;
    }
    if (!lines2_result) {
        std::fprintf(stderr, "cfbox comm: %s\n", lines2_result.error().msg.c_str());
        return 1;
    }

    const auto& lines1 = *lines1_result;
    const auto& lines2 = *lines2_result;

    std::size_t i = 0, j = 0;
    while (i < lines1.size() && j < lines2.size()) {
        if (lines1[i] < lines2[j]) {
            if (!suppress1) std::printf("%s\n", lines1[i].c_str());
            ++i;
        } else if (lines1[i] > lines2[j]) {
            if (!suppress2) std::printf("\t%s\n", lines2[j].c_str());
            ++j;
        } else {
            if (!suppress3) std::printf("\t\t%s\n", lines1[i].c_str());
            ++i; ++j;
        }
    }
    while (i < lines1.size()) {
        if (!suppress1) std::printf("%s\n", lines1[i].c_str());
        ++i;
    }
    while (j < lines2.size()) {
        if (!suppress2) std::printf("\t%s\n", lines2[j].c_str());
        ++j;
    }
    return 0;
}
