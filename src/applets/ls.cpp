#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>

namespace {

auto format_size_human(std::uintmax_t bytes) -> std::string {
    const char* units[] = {"", "K", "M", "G", "T"};
    int unit_idx = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024.0 && unit_idx < 4) {
        size /= 1024.0;
        ++unit_idx;
    }
    char buf[32];
    if (unit_idx == 0) {
        std::snprintf(buf, sizeof(buf), "%ju", bytes);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f%s", size, units[unit_idx]);
    }
    return buf;
}

auto format_permissions(std::filesystem::perms p) -> std::string {
    char buf[11];
    buf[0] = '-'; // will be overridden for special types

    buf[1] = (p & std::filesystem::perms::owner_read)  != std::filesystem::perms::none ? 'r' : '-';
    buf[2] = (p & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? 'w' : '-';
    buf[3] = (p & std::filesystem::perms::owner_exec)  != std::filesystem::perms::none ? 'x' : '-';
    buf[4] = (p & std::filesystem::perms::group_read)  != std::filesystem::perms::none ? 'r' : '-';
    buf[5] = (p & std::filesystem::perms::group_write) != std::filesystem::perms::none ? 'w' : '-';
    buf[6] = (p & std::filesystem::perms::group_exec)  != std::filesystem::perms::none ? 'x' : '-';
    buf[7] = (p & std::filesystem::perms::others_read) != std::filesystem::perms::none ? 'r' : '-';
    buf[8] = (p & std::filesystem::perms::others_write)!= std::filesystem::perms::none ? 'w' : '-';
    buf[9] = (p & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? 'x' : '-';
    buf[10] = '\0';
    return std::string{buf, 10};
}

auto format_type_char(std::filesystem::file_type type) -> char {
    switch (type) {
        case std::filesystem::file_type::directory:     return 'd';
        case std::filesystem::file_type::symlink:       return 'l';
        case std::filesystem::file_type::block:         return 'b';
        case std::filesystem::file_type::character:     return 'c';
        case std::filesystem::file_type::fifo:          return 'p';
        case std::filesystem::file_type::socket:        return 's';
        default:                                         return '-';
    }
}

auto format_time(std::filesystem::file_time_type ftime) -> std::string {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    auto time_t_val = std::chrono::system_clock::to_time_t(sctp);
    char buf[64];
    std::strftime(buf, sizeof(buf), "%b %e %H:%M", std::localtime(&time_t_val));
    return buf;
}

struct LsOptions {
    bool all = false;       // -a
    bool long_format = false; // -l
    bool human = false;     // -h
};

auto list_directory(const std::string& path, const LsOptions& opts) -> int {
    auto entries_result = cfbox::fs::directory_entries(path);
    if (!entries_result) {
        std::fprintf(stderr, "cfbox ls: cannot access '%s': %s\n",
                     path.c_str(), entries_result.error().msg.c_str());
        return 1;
    }

    auto& entries = entries_result.value();

    // Filter hidden files if -a not set
    std::vector<std::filesystem::directory_entry> visible;
    for (const auto& e : entries) {
        std::string name = e.path().filename().string();
        if (!opts.all && !name.empty() && name[0] == '.') continue;
        visible.push_back(e);
    }

    // Sort entries
    std::sort(visible.begin(), visible.end(),
              [](const std::filesystem::directory_entry& a,
                 const std::filesystem::directory_entry& b) {
                  return a.path().filename().string() < b.path().filename().string();
              });

    if (opts.long_format) {
        for (const auto& e : visible) {
            auto status_result = cfbox::fs::symlink_status(e.path().string());
            if (!status_result) continue;
            auto& st = status_result.value();

            char type_char = format_type_char(st.type());
            auto perms = format_permissions(st.permissions());
            perms.insert(perms.begin(), type_char);

            auto nlinks_result = cfbox::fs::hard_link_count(e.path().string());
            std::uintmax_t nlinks = nlinks_result.value_or(1);

            std::uintmax_t size = 0;
            if (st.type() == std::filesystem::file_type::regular) {
                auto sz = cfbox::fs::file_size(e.path().string());
                size = sz.value_or(0);
            }

            auto time_result = cfbox::fs::last_write_time(e.path().string());
            std::string time_str = time_result ? format_time(*time_result) : "";

            std::string size_str;
            if (opts.human) {
                size_str = format_size_human(size);
            } else {
                size_str = std::to_string(size);
            }

            std::string name = e.path().filename().string();
            if (st.type() == std::filesystem::file_type::symlink) {
                std::error_code ec;
                auto target = std::filesystem::read_symlink(e.path(), ec);
                if (!ec) {
                    name += " -> " + target.string();
                }
            }

            std::printf("%s %3ju %-8s %-8s %*s %s %s\n",
                        perms.c_str(),
                        static_cast<std::uintmax_t>(nlinks),
                        "", // owner (placeholder)
                        "", // group (placeholder)
                        opts.human ? 5 : 8,
                        size_str.c_str(),
                        time_str.c_str(),
                        name.c_str());
        }
    } else {
        for (const auto& e : visible) {
            std::printf("%s\n", e.path().filename().string().c_str());
        }
    }

    return 0;
}

auto list_path(const std::string& path, const LsOptions& opts, bool show_header) -> int {
    if (!cfbox::fs::exists(path)) {
        std::fprintf(stderr, "cfbox ls: cannot access '%s': No such file or directory\n",
                     path.c_str());
        return 1;
    }

    if (!cfbox::fs::is_directory(path)) {
        // Single file
        if (opts.long_format) {
            auto status_result = cfbox::fs::symlink_status(path);
            if (!status_result) {
                std::fprintf(stderr, "cfbox ls: %s\n", status_result.error().msg.c_str());
                return 1;
            }
            auto& st = status_result.value();
            char type_char = format_type_char(st.type());
            auto perms = format_permissions(st.permissions());
            perms.insert(perms.begin(), type_char);

            auto nlinks_result = cfbox::fs::hard_link_count(path);
            std::uintmax_t nlinks = nlinks_result.value_or(1);

            std::uintmax_t size = 0;
            if (st.type() == std::filesystem::file_type::regular) {
                auto sz = cfbox::fs::file_size(path);
                size = sz.value_or(0);
            }

            auto time_result = cfbox::fs::last_write_time(path);
            std::string time_str = time_result ? format_time(*time_result) : "";

            std::string size_str = opts.human ? format_size_human(size) : std::to_string(size);
            std::string name = std::filesystem::path{path}.filename().string();

            std::printf("%s %3ju %-8s %-8s %*s %s %s\n",
                        perms.c_str(),
                        static_cast<std::uintmax_t>(nlinks),
                        "", "",
                        opts.human ? 5 : 8,
                        size_str.c_str(),
                        time_str.c_str(),
                        name.c_str());
        } else {
            std::printf("%s\n", std::filesystem::path{path}.filename().string().c_str());
        }
        return 0;
    }

    if (show_header) {
        std::printf("%s:\n", path.c_str());
    }
    return list_directory(path, opts);
}

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "ls",
    .version = CFBOX_VERSION_STRING,
    .one_line = "list directory contents",
    .usage   = "ls [OPTIONS] [FILE]...",
    .options = "  -a     do not ignore entries starting with .\n"
               "  -l     use a long listing format\n"
               "  -h     print sizes in human readable format",
    .extra   = "",
};

} // namespace

auto ls_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'a', false, "all"},
        cfbox::args::OptSpec{'l', false, "long"},
        cfbox::args::OptSpec{'h', false, "human-readable"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    LsOptions opts;
    opts.all = parsed.has('a');
    opts.long_format = parsed.has('l');
    opts.human = parsed.has('h');

    const auto& pos = parsed.positional();
    bool multi = pos.size() > 1;

    if (pos.empty()) {
        return list_path(".", opts, false);
    }

    int rc = 0;
    for (const auto& p : pos) {
        if (list_path(std::string{p}, opts, multi) != 0) {
            rc = 1;
        }
    }
    return rc;
}
