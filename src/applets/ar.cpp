#include <cstdio>
#include <ctime>
#include <cstring>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "ar",
    .version = CFBOX_VERSION_STRING,
    .one_line = "create, modify, and extract from archives",
    .usage   = "ar -[dmpqrtx] ARCHIVE [FILE]...",
    .options = "  -r     insert or replace files\n"
               "  -t     list contents\n"
               "  -x     extract files",
    .extra   = "",
};
} // namespace

auto ar_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'r', false},
        cfbox::args::OptSpec{'t', false},
        cfbox::args::OptSpec{'x', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool replace = parsed.has('r');
    bool list = parsed.has('t');
    bool extract = parsed.has('x');

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox ar: missing archive name\n");
        return 1;
    }

    std::string archive{pos[0]};

    if (replace) {
        std::string output;
        output.append("!<arch>\n", 8);
        for (std::size_t i = 1; i < pos.size(); ++i) {
            std::string fname{pos[i]};
            auto data = cfbox::io::read_all(fname);
            if (!data) continue;
            char hdr[60];
            std::memset(hdr, ' ', 60);
            std::memcpy(hdr, fname.c_str(), std::min(fname.size(), static_cast<std::size_t>(16)));
            std::snprintf(hdr + 16, 12, "%-10lu", static_cast<unsigned long>(::time(nullptr)));
            std::snprintf(hdr + 28, 12, "%-6u", 0);  // uid
            std::snprintf(hdr + 34, 12, "%-6u", 0);  // gid
            std::snprintf(hdr + 40, 12, "%-8o", 0100644);
            std::snprintf(hdr + 48, 12, "%-10zu", data->size());
            hdr[58] = '`'; hdr[59] = '\n';
            output.append(hdr, 60);
            output.append(*data);
            if (data->size() % 2) output += '\n';
        }
        auto wresult = cfbox::io::write_all(archive, output);
        if (!wresult) {
            std::fprintf(stderr, "cfbox ar: %s\n", wresult.error().msg.c_str());
            return 1;
        }
        return 0;
    }

    if (list || extract) {
        auto input = cfbox::io::read_all(archive);
        if (!input) {
            std::fprintf(stderr, "cfbox ar: %s\n", input.error().msg.c_str());
            return 1;
        }
        const auto& data = *input;
        if (data.size() < 8 || data.substr(0, 8) != "!<arch>\n") {
            std::fprintf(stderr, "cfbox ar: not a valid archive\n");
            return 1;
        }
        std::size_t offset = 8;
        while (offset + 60 <= data.size()) {
            auto name = std::string{data.substr(offset, 16)};
            auto space = name.find(' ');
            if (space != std::string::npos) name = name.substr(0, space);
            auto size_str = std::string{data.substr(offset + 48, 10)};
            auto fsize = std::stoul(size_str);

            if (list) {
                std::puts(name.c_str());
            } else if (extract) {
                auto content = data.substr(offset + 60, fsize);
                if (!cfbox::io::write_all(name, content)) {
                    std::fprintf(stderr, "cfbox ar: write failed: %s\n", name.c_str());
                    return 1;
                }
            }
            offset += 60 + fsize;
            if (fsize % 2) ++offset;
        }
        return 0;
    }

    std::fprintf(stderr, "cfbox ar: must specify -r, -t, or -x\n");
    return 1;
}
