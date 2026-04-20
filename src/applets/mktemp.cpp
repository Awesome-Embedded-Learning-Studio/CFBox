#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "mktemp",
    .version = CFBOX_VERSION_STRING,
    .one_line = "create a temporary file or directory",
    .usage   = "mktemp [-d] [-p DIR] [TEMPLATE]",
    .options = "  -d         create a directory instead of a file\n"
               "  -p DIR     create in directory DIR\n"
               "  -u         do not create anything; just print a name",
    .extra   = "TEMPLATE must end in XXXXXX (at least 6 X's). Default: /tmp/tmp.XXXXXX",
};
} // namespace

auto mktemp_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'d', false},
        cfbox::args::OptSpec{'p', true, "tmpdir"},
        cfbox::args::OptSpec{'u', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool make_dir = parsed.has('d');
    bool dry_run = parsed.has('u');
    const auto& pos = parsed.positional();

    std::string tmpl;
    if (pos.empty()) {
        auto prefix = parsed.get_any('p', "tmpdir");
        if (prefix) {
            tmpl = std::string{*prefix} + "/tmp.XXXXXX";
        } else {
            tmpl = "/tmp/tmp.XXXXXX";
        }
    } else {
        tmpl = std::string{pos[0]};
        auto prefix = parsed.get_any('p', "tmpdir");
        if (prefix && tmpl.find('/') == std::string::npos) {
            tmpl = std::string{*prefix} + "/" + tmpl;
        }
    }

    if (dry_run) {
        // Replace trailing X's with random chars
        auto xpos = tmpl.rfind('X');
        if (xpos == std::string::npos || xpos < tmpl.size() - 6) {
            std::fprintf(stderr, "cfbox mktemp: too few X's in template '%s'\n", tmpl.c_str());
            return 1;
        }
        std::puts(tmpl.c_str());
        return 0;
    }

    if (make_dir) {
        char* result = mkdtemp(tmpl.data());
        if (!result) {
            std::fprintf(stderr, "cfbox mktemp: failed to create directory: %s\n",
                         std::strerror(errno));
            return 1;
        }
        std::puts(result);
    } else {
        int fd = mkstemp(tmpl.data());
        if (fd < 0) {
            std::fprintf(stderr, "cfbox mktemp: failed to create file: %s\n",
                         std::strerror(errno));
            return 1;
        }
        ::close(fd);
        std::puts(tmpl.c_str());
    }
    return 0;
}
