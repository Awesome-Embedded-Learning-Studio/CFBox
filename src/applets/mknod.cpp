#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "mknod",
    .version = CFBOX_VERSION_STRING,
    .one_line = "make block or character special files",
    .usage   = "mknod NAME TYPE [MAJOR MINOR]",
    .options = "",
    .extra   = "TYPE: b for block, c or u for character, p for FIFO",
};
} // namespace

auto mknod_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox mknod: missing operand\n");
        return 1;
    }

    std::string name{pos[0]};
    char type = pos[1][0];

    mode_t mode;
    dev_t dev = 0;

    switch (type) {
        case 'b':
            if (pos.size() < 4) {
                std::fprintf(stderr, "cfbox mknod: missing major/minor for block device\n");
                return 1;
            }
            mode = S_IFBLK | 0660;
            dev = makedev(static_cast<unsigned int>(std::stoul(std::string{pos[2]})),
                          static_cast<unsigned int>(std::stoul(std::string{pos[3]})));
            break;
        case 'c': case 'u':
            if (pos.size() < 4) {
                std::fprintf(stderr, "cfbox mknod: missing major/minor for char device\n");
                return 1;
            }
            mode = S_IFCHR | 0660;
            dev = makedev(static_cast<unsigned int>(std::stoul(std::string{pos[2]})),
                          static_cast<unsigned int>(std::stoul(std::string{pos[3]})));
            break;
        case 'p':
            mode = S_IFIFO | 0666;
            break;
        default:
            std::fprintf(stderr, "cfbox mknod: invalid type '%c'\n", type);
            return 1;
    }

    if (mknod(name.c_str(), mode, dev) != 0) {
        std::fprintf(stderr, "cfbox mknod: cannot create node '%s': %s\n",
                     name.c_str(), std::strerror(errno));
        return 1;
    }
    return 0;
}
