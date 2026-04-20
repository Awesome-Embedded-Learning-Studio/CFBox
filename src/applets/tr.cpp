#include <cstdio>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "tr",
    .version = CFBOX_VERSION_STRING,
    .one_line = "translate, squeeze, and/or delete characters",
    .usage   = "tr [-c] [-d] [-s] SET1 [SET2]",
    .options = "  -c     use the complement of SET1\n"
               "  -d     delete characters in SET1\n"
               "  -s     squeeze repeated characters",
    .extra   = "Ranges like a-z and A-Z are supported.",
};
} // namespace

static auto expand_set(const std::string& s) -> std::string {
    std::string result;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i + 2 < s.size() && s[i + 1] == '-') {
            for (unsigned char c = static_cast<unsigned char>(s[i]);
                 c <= static_cast<unsigned char>(s[i + 2]); ++c) {
                result += static_cast<char>(c);
            }
            i += 2;
        } else {
            result += s[i];
        }
    }
    return result;
}

auto tr_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'c', false, "complement"},
        cfbox::args::OptSpec{'d', false, "delete"},
        cfbox::args::OptSpec{'s', false, "squeeze-repeats"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool complement = parsed.has('c') || parsed.has_long("complement");
    bool del_mode = parsed.has('d') || parsed.has_long("delete");
    bool squeeze = parsed.has('s') || parsed.has_long("squeeze-repeats");

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox tr: missing operand\n");
        return 1;
    }

    std::string set1 = expand_set(std::string{pos[0]});
    std::string set2;
    if (pos.size() > 1) {
        set2 = expand_set(std::string{pos[1]});
    }

    // Build lookup for complement mode
    bool in_set1[256] = {};
    for (char c : set1) {
        in_set1[static_cast<unsigned char>(c)] = true;
    }

    // Build translation map
    char translate[256];
    for (int i = 0; i < 256; ++i) translate[i] = static_cast<char>(i);
    if (!del_mode && !set2.empty()) {
        if (!complement) {
            for (std::size_t i = 0; i < set1.size(); ++i) {
                auto idx = static_cast<unsigned char>(set1[i]);
                translate[idx] = (i < set2.size()) ? set2[i] : set2.back();
            }
        } else {
            std::size_t j = 0;
            for (int i = 0; i < 256; ++i) {
                if (!in_set1[i]) {
                    if (j < set2.size()) {
                        translate[i] = set2[j];
                    } else {
                        translate[i] = set2.back();
                    }
                    ++j;
                }
            }
        }
    }

    auto input = cfbox::io::read_all_stdin();
    if (!input) {
        std::fprintf(stderr, "cfbox tr: %s\n", input.error().msg.c_str());
        return 1;
    }

    char prev = '\0';
    for (char c : *input) {
        auto uc = static_cast<unsigned char>(c);
        bool match = complement ? !in_set1[uc] : in_set1[uc];

        if (del_mode) {
            if (match) continue;
            if (squeeze && c == prev) continue;
            prev = c;
            std::putchar(c);
        } else {
            char out = translate[uc];
            if (squeeze && out == prev) continue;
            prev = out;
            std::putchar(out);
        }
    }
    return 0;
}
