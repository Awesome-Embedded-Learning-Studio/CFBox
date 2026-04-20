#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <pwd.h>
#include <grp.h>
#include <ctime>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "stat",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display file or file system status",
    .usage   = "stat [-c FORMAT] FILE...",
    .options = "  -c FORMAT  use the specified FORMAT instead of the default\n"
               "  -f         display file system status",
    .extra   = "Format escapes: %n name, %s size, %b blocks, %f blocks*512, %F type,\n"
               "  %U user, %G group, %a octal perms, %A human perms, %y mtime",
};
} // namespace

static auto file_type_string(mode_t mode) -> const char* {
    if (S_ISREG(mode))  return "regular file";
    if (S_ISDIR(mode))  return "directory";
    if (S_ISLNK(mode))  return "symbolic link";
    if (S_ISCHR(mode))  return "character special file";
    if (S_ISBLK(mode))  return "block special file";
    if (S_ISFIFO(mode)) return "fifo";
    if (S_ISSOCK(mode)) return "socket";
    return "unknown";
}

static auto format_perms(mode_t mode) -> std::string {
    char buf[12];
    buf[0] = S_ISDIR(mode) ? 'd' : S_ISLNK(mode) ? 'l' : S_ISCHR(mode) ? 'c' :
             S_ISBLK(mode) ? 'b' : S_ISFIFO(mode) ? 'p' : S_ISSOCK(mode) ? 's' : '-';
    buf[1] = (mode & S_IRUSR) ? 'r' : '-';
    buf[2] = (mode & S_IWUSR) ? 'w' : '-';
    buf[3] = (mode & S_IXUSR) ? 'x' : '-';
    buf[4] = (mode & S_IRGRP) ? 'r' : '-';
    buf[5] = (mode & S_IWGRP) ? 'w' : '-';
    buf[6] = (mode & S_IXGRP) ? 'x' : '-';
    buf[7] = (mode & S_IROTH) ? 'r' : '-';
    buf[8] = (mode & S_IWOTH) ? 'w' : '-';
    buf[9] = (mode & S_IXOTH) ? 'x' : '-';
    buf[10] = '\0';
    return buf;
}

static auto format_time(const timespec& ts) -> std::string {
    char buf[64];
    auto* tm = localtime(&ts.tv_sec);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", tm);
    return buf;
}

auto stat_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'c', true, "format"},
        cfbox::args::OptSpec{'f', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    auto format = parsed.get_any('c', "format");
    bool fs_stat = parsed.has('f');
    const auto& pos = parsed.positional();

    if (pos.empty()) {
        std::fprintf(stderr, "cfbox stat: missing operand\n");
        return 1;
    }

    int rc = 0;
    for (auto p : pos) {
        std::string path{p};

        if (fs_stat) {
            struct statvfs vfs;
            if (statvfs(path.c_str(), &vfs) != 0) {
                std::fprintf(stderr, "cfbox stat: cannot stat '%s': %s\n",
                             path.c_str(), std::strerror(errno));
                rc = 1;
                continue;
            }
            std::printf("  File: \"%s\"\n", path.c_str());
            std::printf("    ID: %-12lx Namelen: %-8lu Type: %s\n",
                        static_cast<unsigned long>(vfs.f_fsid),
                        static_cast<unsigned long>(vfs.f_namemax),
                        vfs.f_type == 0xEF53 ? "ext2/ext3" : "unknown");
            std::printf("Block size: %-10lu Total blocks: %-10lu Free blocks: %lu\n",
                        static_cast<unsigned long>(vfs.f_bsize),
                        static_cast<unsigned long>(vfs.f_blocks),
                        static_cast<unsigned long>(vfs.f_bfree));
            continue;
        }

        struct stat st;
        if (lstat(path.c_str(), &st) != 0) {
            std::fprintf(stderr, "cfbox stat: cannot stat '%s': %s\n",
                         path.c_str(), std::strerror(errno));
            rc = 1;
            continue;
        }

        if (format) {
            std::string fmt{*format};
            for (std::size_t i = 0; i < fmt.size(); ++i) {
                if (fmt[i] == '%' && i + 1 < fmt.size()) {
                    char spec = fmt[++i];
                    switch (spec) {
                        case 'n': std::fputs(path.c_str(), stdout); break;
                        case 's': std::printf("%lu", static_cast<unsigned long>(st.st_size)); break;
                        case 'b': std::printf("%lu", static_cast<unsigned long>(st.st_blocks)); break;
                        case 'f': std::printf("%lu", static_cast<unsigned long>(st.st_blocks * 512)); break;
                        case 'F': std::fputs(file_type_string(st.st_mode), stdout); break;
                        case 'U': {
                            auto* pw = getpwuid(st.st_uid);
                            std::fputs(pw ? pw->pw_name : std::to_string(st.st_uid).c_str(), stdout);
                            break;
                        }
                        case 'G': {
                            auto* gr = getgrgid(st.st_gid);
                            std::fputs(gr ? gr->gr_name : std::to_string(st.st_gid).c_str(), stdout);
                            break;
                        }
                        case 'a': std::printf("%o", st.st_mode & 07777); break;
                        case 'A': std::fputs(format_perms(st.st_mode).c_str(), stdout); break;
                        case 'y': {
#if defined(__linux__)
                            std::fputs(format_time(st.st_mtim).c_str(), stdout);
#else
                            std::fputs(format_time({st.st_mtime, 0}).c_str(), stdout);
#endif
                            break;
                        }
                        default: std::putchar('%'); std::putchar(spec); break;
                    }
                } else {
                    std::putchar(fmt[i]);
                }
            }
            std::putchar('\n');
        } else {
            std::printf("  File: %s\n", path.c_str());
            std::printf("  Size: %-15lu Blocks: %-10lu IO Block: %lu  %s\n",
                        static_cast<unsigned long>(st.st_size),
                        static_cast<unsigned long>(st.st_blocks),
                        static_cast<unsigned long>(st.st_blksize),
                        file_type_string(st.st_mode));
            auto* pw = getpwuid(st.st_uid);
            auto* gr = getgrgid(st.st_gid);
            std::printf("Access: (%04o/%s)  Uid: (%5d/%-8s)   Gid: (%5d/%-8s)\n",
                        st.st_mode & 07777,
                        format_perms(st.st_mode).c_str(),
                        st.st_uid, pw ? pw->pw_name : "",
                        st.st_gid, gr ? gr->gr_name : "");
#if defined(__linux__)
            std::printf("Modify: %s\n", format_time(st.st_mtim).c_str());
            std::printf("Change: %s\n", format_time(st.st_ctim).c_str());
#else
            std::printf("Modify: %s\n", format_time({st.st_mtime, 0}).c_str());
#endif
        }
    }
    return rc;
}
