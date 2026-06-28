#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <grp.h>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
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
    char buf[10];
    buf[0] = (p & std::filesystem::perms::owner_read) != std::filesystem::perms::none ? 'r' : '-';
    buf[1] = (p & std::filesystem::perms::owner_write) != std::filesystem::perms::none ? 'w' : '-';
    buf[2] = (p & std::filesystem::perms::owner_exec) != std::filesystem::perms::none ? 'x' : '-';
    buf[3] = (p & std::filesystem::perms::group_read) != std::filesystem::perms::none ? 'r' : '-';
    buf[4] = (p & std::filesystem::perms::group_write) != std::filesystem::perms::none ? 'w' : '-';
    buf[5] = (p & std::filesystem::perms::group_exec) != std::filesystem::perms::none ? 'x' : '-';
    buf[6] = (p & std::filesystem::perms::others_read) != std::filesystem::perms::none ? 'r' : '-';
    buf[7] = (p & std::filesystem::perms::others_write) != std::filesystem::perms::none ? 'w' : '-';
    buf[8] = (p & std::filesystem::perms::others_exec) != std::filesystem::perms::none ? 'x' : '-';
    buf[9] = '\0';
    return std::string{buf, 9};  // type char is prepended by the caller
}

auto format_type_char(std::filesystem::file_type type) -> char {
    switch (type) {
        case std::filesystem::file_type::directory:
            return 'd';
        case std::filesystem::file_type::symlink:
            return 'l';
        case std::filesystem::file_type::block:
            return 'b';
        case std::filesystem::file_type::character:
            return 'c';
        case std::filesystem::file_type::fifo:
            return 'p';
        case std::filesystem::file_type::socket:
            return 's';
        default:
            return '-';
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

// Resolve uid/gid to a name; fall back to the numeric id when NSS cannot
// resolve it (a statically linked cfbox on a minimal rootfs has no NSS libs,
// so names silently fail — show the number instead of a blank field).
auto owner_of(uid_t uid) -> std::string {
    if (auto* pw = getpwuid(uid))
        return pw->pw_name;
    return std::to_string(uid);
}

auto group_of(gid_t gid) -> std::string {
    if (auto* gr = getgrgid(gid))
        return gr->gr_name;
    return std::to_string(gid);
}

enum class ColorMode { Never, Auto, Always };

struct LsOptions {
    bool all = false;         // -a
    bool long_format = false; // -l
    bool human = false;       // -h
    bool recursive = false;   // -R
};

// ANSI SGR code for a file type, or nullptr when no coloring applies. Type-based
// fixed colors only (LS_COLORS glob parsing is deferred to control binary size).
auto color_code(std::filesystem::file_type type, std::filesystem::perms perms, bool use_color) -> const char* {
    if (!use_color) return nullptr;
    using ft = std::filesystem::file_type;
    using pm = std::filesystem::perms;
    switch (type) {
        case ft::directory: return "01;34";  // bold blue
        case ft::symlink:   return "01;36";  // bold cyan
        case ft::fifo:      return "33";     // yellow
        case ft::socket:    return "01;35";  // bold magenta
        case ft::block:
        case ft::character: return "01;33";  // bold yellow
        case ft::regular: {
            bool exec = (perms & (pm::owner_exec | pm::group_exec | pm::others_exec)) != pm::none;
            return exec ? "01;32" : nullptr;  // bold green
        }
        default: return nullptr;
    }
}

auto colorize(const std::string& name, std::filesystem::file_type type,
              std::filesystem::perms perms, bool use_color) -> std::string {
    const char* code = color_code(type, perms, use_color);
    if (!code) return name;
    return "\033[" + std::string{code} + "m" + name + "\033[0m";
}

// Read a directory, drop hidden entries (unless -a), and sort by name.
auto collect_visible(const std::string& path, const LsOptions& opts)
    -> cfbox::base::Result<std::vector<std::filesystem::directory_entry>> {
    auto entries = cfbox::fs::directory_entries(path);
    if (!entries) return std::unexpected(entries.error());
    std::vector<std::filesystem::directory_entry> visible;
    visible.reserve(entries->size());
    for (const auto& e : *entries) {
        std::string name = e.path().filename().string();
        if (!opts.all && !name.empty() && name[0] == '.') continue;
        visible.push_back(e);
    }
    std::sort(visible.begin(), visible.end(),
              [](const std::filesystem::directory_entry& a, const std::filesystem::directory_entry& b) {
                  return a.path().filename().string() < b.path().filename().string();
              });
    return visible;
}

// Print a single entry (file or directory member) in short or long form.
// The name is colorized when use_color is set; the symlink "-> target" suffix
// in long form is appended after the colorized name.
auto print_entry(const std::string& path, const LsOptions& opts, bool use_color) -> void {
    auto st_r = cfbox::fs::symlink_status(path);
    std::filesystem::file_type type = st_r ? st_r->type() : std::filesystem::file_type::none;
    std::filesystem::perms perms = st_r ? st_r->permissions() : std::filesystem::perms::none;
    std::string name = std::filesystem::path{path}.filename().string();
    std::string display = colorize(name, type, perms, use_color);

    if (!opts.long_format) {
        std::printf("%s\n", display.c_str());
        return;
    }

    char type_char = format_type_char(type);
    auto perm_str = format_permissions(perms);
    perm_str.insert(perm_str.begin(), type_char);

    auto nlinks = cfbox::fs::hard_link_count(path).value_or(1);
    std::uintmax_t size = 0;
    if (type == std::filesystem::file_type::regular) {
        size = cfbox::fs::file_size(path).value_or(0);
    }
    auto time_r = cfbox::fs::last_write_time(path);
    std::string time_str = time_r ? format_time(*time_r) : "";
    std::string size_str = opts.human ? format_size_human(size) : std::to_string(size);

    std::string owner = "?";
    std::string group = "?";
    struct stat lst {};
    if (::lstat(path.c_str(), &lst) == 0) {
        owner = owner_of(lst.st_uid);
        group = group_of(lst.st_gid);
    }

    if (type == std::filesystem::file_type::symlink) {
        std::error_code ec;
        auto target = std::filesystem::read_symlink(std::filesystem::path{path}, ec);
        if (!ec) display += " -> " + target.string();
    }

    std::printf("%s %3ju %-8s %-8s %*s %s %s\n", perm_str.c_str(),
                static_cast<std::uintmax_t>(nlinks), owner.c_str(), group.c_str(),
                opts.human ? 5 : 8, size_str.c_str(), time_str.c_str(), display.c_str());
}

auto list_directory(const std::string& path, const LsOptions& opts, bool use_color) -> int {
    auto visible = collect_visible(path, opts);
    if (!visible) {
        CFBOX_ERR("ls", "cannot access '%s': %s", path.c_str(), visible.error().msg.c_str());
        return 1;
    }
    for (const auto& e : *visible) {
        print_entry(e.path().string(), opts, use_color);
    }
    return 0;
}

// GNU-style recursive listing: each directory is its own block with a "path:"
// header, blocks separated by a blank line. Symlinked directories are NOT
// descended into (matches GNU ls and prevents cycles).
auto list_recursive(const std::string& path, const LsOptions& opts, bool use_color, bool leading_blank) -> int {
    if (leading_blank) std::printf("\n");
    std::printf("%s:\n", path.c_str());
    int rc = list_directory(path, opts, use_color);

    auto visible = collect_visible(path, opts);
    if (!visible) return rc;
    for (const auto& e : *visible) {
        auto st = cfbox::fs::symlink_status(e.path().string());
        if (st && st->type() == std::filesystem::file_type::directory) {
            if (list_recursive(e.path().string(), opts, use_color, true) != 0) rc = 1;
        }
    }
    return rc;
}

auto list_path(const std::string& path, const LsOptions& opts, bool use_color, bool show_header) -> int {
    if (!cfbox::fs::exists(path)) {
        CFBOX_ERR("ls", "cannot access '%s': No such file or directory", path.c_str());
        return 1;
    }
    if (!cfbox::fs::is_directory(path)) {
        print_entry(path, opts, use_color);
        return 0;
    }
    if (show_header) std::printf("%s:\n", path.c_str());
    return list_directory(path, opts, use_color);
}

constexpr cfbox::help::HelpEntry HELP = {
    .name = "ls",
    .version = CFBOX_VERSION_STRING,
    .one_line = "list directory contents",
    .usage = "ls [OPTIONS] [FILE]...",
    .options = "  -a, --all          do not ignore entries starting with .\n"
               "  -l, --long         use a long listing format\n"
               "  -h, --human-readable  print sizes in human readable format\n"
               "  -R, --recursive    list subdirectories recursively\n"
               "      --color[=WHEN] colorize output (always/auto/never)",
    .extra = "",
};

} // namespace

auto ls_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv,
                                     {
                                         cfbox::args::OptSpec{'a', false, "all"},
                                         cfbox::args::OptSpec{'l', false, "long"},
                                         cfbox::args::OptSpec{'h', false, "human-readable"},
                                         cfbox::args::OptSpec{'R', false, "recursive"},
                                         // --color is long-only with an optional value; left
                                         // unregistered so parse records it without eating the
                                         // next positional argument as its value.
                                     });

    if (parsed.has_long("help")) {
        cfbox::help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        cfbox::help::print_version(HELP);
        return 0;
    }

    LsOptions opts;
    opts.all = parsed.has('a');
    opts.long_format = parsed.has('l');
    opts.human = parsed.has('h');
    opts.recursive = parsed.has('R') || parsed.has_long("recursive");

    // --color: a bare flag means "always"; otherwise honor always/auto/never.
    ColorMode color = ColorMode::Auto;
    if (parsed.has_long("color")) {
        auto v = parsed.get_long("color");
        if (!v || *v == "always") color = ColorMode::Always;
        else if (*v == "never")   color = ColorMode::Never;
        else if (*v == "auto")    color = ColorMode::Auto;
        else {
            CFBOX_ERR("ls", "invalid argument '%.*s' for '--color'",
                      static_cast<int>(v->size()), v->data());
            return 2;
        }
    }
    bool use_color = (color == ColorMode::Always) ||
                     (color == ColorMode::Auto && ::isatty(STDOUT_FILENO) == 1);

    const auto& pos = parsed.positional();

    if (opts.recursive) {
        if (pos.empty()) {
            return list_recursive(".", opts, use_color, /*leading_blank=*/false);
        }
        int rc = 0;
        bool first = true;
        for (const auto& p : pos) {
            std::string path{p};
            if (!cfbox::fs::exists(path)) {
                CFBOX_ERR("ls", "cannot access '%s': No such file or directory", path.c_str());
                rc = 1;
                first = false;
                continue;
            }
            if (!cfbox::fs::is_directory(path)) {
                print_entry(path, opts, use_color);
            } else {
                if (list_recursive(path, opts, use_color, /*leading_blank=*/!first) != 0) rc = 1;
            }
            first = false;
        }
        return rc;
    }

    bool multi = pos.size() > 1;
    if (pos.empty()) {
        return list_path(".", opts, use_color, false);
    }
    int rc = 0;
    for (const auto& p : pos) {
        if (list_path(std::string{p}, opts, use_color, multi) != 0) rc = 1;
    }
    return rc;
}
