#include <cstdio>
#include <filesystem>
#include <string>
#include <sys/stat.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "du",
    .version = CFBOX_VERSION_STRING,
    .one_line = "estimate file space usage",
    .usage   = "du [OPTIONS] [FILE]...",
    .options = "  -s     display only a total for each argument\n"
               "  -h     print sizes in human readable format\n"
               "  -x     skip directories on different file systems",
    .extra   = "",
};

auto human_size(std::uintmax_t bytes) -> std::string {
    constexpr const char* units[] = {"", "K", "M", "G", "T"};
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024 && unit < 4) {
        size /= 1024;
        ++unit;
    }
    char buf[32];
    if (unit == 0) {
        std::snprintf(buf, sizeof(buf), "%ju", bytes);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f%s", size, units[unit]);
    }
    return buf;
}

} // namespace

auto du_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'s', false},
        cfbox::args::OptSpec{'h', false},
        cfbox::args::OptSpec{'x', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool human = parsed.has('h');
    bool one_fs = parsed.has('x');
    const auto& pos = parsed.positional();

    auto targets = pos.empty() ? std::vector<std::string_view>{"."} : pos;

    int rc = 0;
    for (auto t : targets) {
        std::string path{t};
        std::error_code ec;
        std::uintmax_t total = 0;

        auto start_dev = [&]() -> std::uintmax_t {
            if (!one_fs) return std::numeric_limits<std::uintmax_t>::max();
            struct stat sb;
            if (::stat(path.c_str(), &sb) != 0)
                return std::numeric_limits<std::uintmax_t>::max();
            return sb.st_dev;
        }();

        for (auto it = std::filesystem::recursive_directory_iterator(
                std::filesystem::path{path},
                std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) { ec.clear(); continue; }

            if (one_fs) {
                struct stat sb;
                if (::lstat(it->path().c_str(), &sb) == 0 && static_cast<std::uintmax_t>(sb.st_dev) != start_dev) {
                    it.disable_recursion_pending();
                    continue;
                }
            }

            if (!it->is_directory(ec) && !ec) {
                auto sz = it->file_size(ec);
                if (!ec) total += sz;
            }
        }

        if (human) {
            std::printf("%s\t%s\n", human_size(total).c_str(), path.c_str());
        } else {
            std::printf("%ju\t%s\n", total / 1024 + (total % 1024 ? 1 : 0), path.c_str());
        }
    }
    return rc;
}
