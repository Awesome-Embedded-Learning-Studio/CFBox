#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <grp.h>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <cfbox/error.hpp>

namespace cfbox::fs {

namespace detail {

inline auto make_error(int ec, std::string_view msg) -> base::Error {
    return base::Error{ec, std::string{msg}};
}

} // namespace detail

inline auto exists(std::string_view path) -> bool {
    return std::filesystem::exists(std::filesystem::path{path});
}

inline auto is_directory(std::string_view path) -> bool {
    return std::filesystem::is_directory(std::filesystem::path{path});
}

inline auto file_size(std::string_view path) -> base::Result<std::uintmax_t> {
    std::error_code ec;
    auto sz = std::filesystem::file_size(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return sz;
}

inline auto mkdir_single(std::string_view path, std::filesystem::perms mode) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::create_directory(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    std::filesystem::permissions(std::filesystem::path{path}, mode, ec);
    if (ec) {
        // non-fatal: directory created but permissions not set
    }
    return {};
}

inline auto mkdir_recursive(std::string_view path, std::filesystem::perms mode) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    std::filesystem::permissions(std::filesystem::path{path}, mode, ec);
    return {};
}

inline auto remove_single(std::string_view path) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::remove(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto remove_all(std::string_view path) -> base::Result<std::uintmax_t> {
    std::error_code ec;
    auto count = std::filesystem::remove_all(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return count;
}

inline auto copy_file(std::string_view src, std::string_view dst) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::copy_file(
        std::filesystem::path{src},
        std::filesystem::path{dst},
        std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto copy_recursive(std::string_view src, std::string_view dst) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::copy(
        std::filesystem::path{src},
        std::filesystem::path{dst},
        std::filesystem::copy_options::recursive |
            std::filesystem::copy_options::overwrite_existing,
        ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto rename(std::string_view old_path, std::string_view new_path) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::rename(std::filesystem::path{old_path}, std::filesystem::path{new_path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto create_hard_link(std::string_view target, std::string_view link_path) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::create_hard_link(
        std::filesystem::path{target},
        std::filesystem::path{link_path},
        ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto current_path() -> base::Result<std::string> {
    std::error_code ec;
    auto p = std::filesystem::current_path(ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return p.string();
}

inline auto directory_entries(std::string_view path) -> base::Result<std::vector<std::filesystem::directory_entry>> {
    std::error_code ec;
    std::filesystem::directory_iterator it{std::filesystem::path{path}, ec};
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    std::vector<std::filesystem::directory_entry> entries;
    for (const auto& e : it) {
        entries.push_back(e);
    }
    return entries;
}

inline auto status(std::string_view path) -> base::Result<std::filesystem::file_status> {
    std::error_code ec;
    auto st = std::filesystem::status(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return st;
}

inline auto symlink_status(std::string_view path) -> base::Result<std::filesystem::file_status> {
    std::error_code ec;
    auto st = std::filesystem::symlink_status(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return st;
}

inline auto last_write_time(std::string_view path) -> base::Result<std::filesystem::file_time_type> {
    std::error_code ec;
    auto t = std::filesystem::last_write_time(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return t;
}

inline auto permissions(std::string_view path, std::filesystem::perms prms) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::permissions(std::filesystem::path{path}, prms, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto hard_link_count(std::string_view path) -> base::Result<std::uintmax_t> {
    std::error_code ec;
    auto count = std::filesystem::hard_link_count(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return count;
}

inline auto create_symlink(std::string_view target, std::string_view link_path) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::create_symlink(
        std::filesystem::path{target},
        std::filesystem::path{link_path},
        ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto read_symlink(std::string_view path) -> base::Result<std::string> {
    std::error_code ec;
    auto p = std::filesystem::read_symlink(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return p.string();
}

inline auto canonical(std::string_view path) -> base::Result<std::string> {
    std::error_code ec;
    auto p = std::filesystem::canonical(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return p.string();
}

inline auto resize_file(std::string_view path, std::uintmax_t new_size) -> base::Result<void> {
    std::error_code ec;
    std::filesystem::resize_file(std::filesystem::path{path}, new_size, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return {};
}

inline auto space(std::string_view path) -> base::Result<std::filesystem::space_info> {
    std::error_code ec;
    auto info = std::filesystem::space(std::filesystem::path{path}, ec);
    if (ec) {
        return std::unexpected(base::Error{static_cast<int>(ec.value()), ec.message()});
    }
    return info;
}

inline auto chown(std::string_view path, uid_t uid, gid_t gid) -> base::Result<void> {
    if (::chown(std::string{path}.c_str(), uid, gid) != 0) {
        return std::unexpected(base::Error{errno, std::strerror(errno)});
    }
    return {};
}

inline auto lchown(std::string_view path, uid_t uid, gid_t gid) -> base::Result<void> {
    if (::lchown(std::string{path}.c_str(), uid, gid) != 0) {
        return std::unexpected(base::Error{errno, std::strerror(errno)});
    }
    return {};
}

// Resolve uid/gid to a name via NSS; fall back to the numeric id when NSS cannot
// resolve it (a statically linked cfbox on a minimal rootfs has no NSS libs, so
// names silently fail — show the number instead of a blank field). Shared by
// ls/stat/id/whoami so the fallback policy is consistent across applets.
inline auto owner_name(uid_t uid) -> std::string {
    if (auto* pw = ::getpwuid(uid))
        return pw->pw_name;
    return std::to_string(uid);
}

inline auto group_name(gid_t gid) -> std::string {
    if (auto* gr = ::getgrgid(gid))
        return gr->gr_name;
    return std::to_string(gid);
}

// lstat — link-aware status (does NOT follow symlinks). Archive copy must read
// the link itself rather than its target, so it cannot use std::filesystem::status.
inline auto lstat(std::string_view path) -> base::Result<struct stat> {
    struct stat st{};
    if (::lstat(std::string{path}.c_str(), &st) != 0) {
        return std::unexpected(base::Error{errno, std::strerror(errno)});
    }
    return st;
}

// Set atime/mtime via utimensat. no_follow applies AT_SYMLINK_NOFOLLOW so a
// symlink's own timestamps are set without dereferencing its target (utime(2)
// cannot do this). times[0]=atime, times[1]=mtime.
inline auto set_times(std::string_view path, const struct timespec times[2], bool no_follow) -> base::Result<void> {
    int flags = no_follow ? AT_SYMLINK_NOFOLLOW : 0;
    if (::utimensat(AT_FDCWD, std::string{path}.c_str(), times, flags) != 0) {
        return std::unexpected(base::Error{errno, std::strerror(errno)});
    }
    return {};
}

template <typename Func>
void for_each_entry(std::string_view path, bool recursive, Func&& fn) {
    if (recursive && is_directory(path)) {
        std::error_code ec;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(std::filesystem::path{path}, ec)) {
            if (ec) continue;
            fn(entry.path().string());
        }
    }
    fn(std::string{path});
}

} // namespace cfbox::fs
