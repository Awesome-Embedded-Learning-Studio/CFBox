#include <sys/stat.h>

#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "cp",
    .version = CFBOX_VERSION_STRING,
    .one_line = "copy files and directories",
    .usage   = "cp [OPTIONS] SOURCE... DEST",
    .options = "  -r, --recursive   copy directories recursively\n"
               "  -a, --archive     archive mode: -r + preserve mode/owner/time, copy symlinks as links\n"
               "  -p, --preserve    preserve mode, ownership, and timestamps",
    .extra   = "",
};

// Copy ownership + atime/mtime + mode from src to dst. Failures are non-fatal:
// ownership preservation needs privileges (EPERM for non-root), and a missing
// attribute must not abort a multi-file copy (matches coreutils -p behavior).
auto preserve_attrs(const std::string& src, const std::string& dst, bool is_symlink) -> void {
    auto st = cfbox::fs::lstat(src);
    if (!st) {
        return;
    }

    // Ownership: lchown for symlinks (no dereference), chown otherwise.
    auto owner = is_symlink ? cfbox::fs::lchown(dst, st->st_uid, st->st_gid)
                            : cfbox::fs::chown(dst, st->st_uid, st->st_gid);
    (void)owner;  // non-fatal

    // Timestamps: symlinks need AT_SYMLINK_NOFOLLOW so we don't touch the target.
    struct timespec times[2];
    times[0] = st->st_atim;
    times[1] = st->st_mtim;
    auto tr = cfbox::fs::set_times(dst, times, is_symlink);
    (void)tr;  // non-fatal

    // Mode is meaningless on a symlink itself; only set it for regular files/dirs.
    if (!is_symlink) {
        auto pr = cfbox::fs::permissions(dst, static_cast<std::filesystem::perms>(st->st_mode & 0777));
        (void)pr;  // non-fatal
    }
}

// Copy a single filesystem entry in archive mode, link-aware (never follows
// symlinks). Returns 0 on success, 1 on a content-copy failure. Attribute
// failures are reported non-fatally inside preserve_attrs.
auto copy_one_archive(const std::string& src, const std::string& dst) -> int {
    auto st = cfbox::fs::lstat(src);
    if (!st) {
        CFBOX_ERR("cp", "cannot stat '%s': %s", src.c_str(), st.error().msg.c_str());
        return 1;
    }

    if (S_ISLNK(st->st_mode)) {
        auto target = cfbox::fs::read_symlink(src);
        if (!target) {
            CFBOX_ERR("cp", "cannot read symlink '%s': %s", src.c_str(), target.error().msg.c_str());
            return 1;
        }
        // Overwrite an existing destination entry so re-runs are idempotent.
        if (cfbox::fs::exists(dst)) {
            auto rm = cfbox::fs::remove_all(dst);
            (void)rm;
        }
        auto cr = cfbox::fs::create_symlink(*target, dst);
        if (!cr) {
            CFBOX_ERR("cp", "cannot create symlink '%s': %s", dst.c_str(), cr.error().msg.c_str());
            return 1;
        }
        preserve_attrs(src, dst, /*is_symlink=*/true);
        return 0;
    }

    if (S_ISDIR(st->st_mode)) {
        // Create with default (writable) perms first; the source mode is applied
        // after children are copied so a read-only source dir never blocks writes.
        std::error_code ec;
        std::filesystem::create_directory(std::filesystem::path{dst}, ec);
        if (ec) {
            CFBOX_ERR("cp", "cannot create directory '%s': %s", dst.c_str(), ec.message().c_str());
            return 1;
        }

        int rc = 0;
        auto entries = cfbox::fs::directory_entries(src);
        if (!entries) {
            CFBOX_ERR("cp", "cannot read directory '%s': %s", src.c_str(), entries.error().msg.c_str());
            return 1;
        }
        for (const auto& e : *entries) {
            auto name = e.path().filename();
            std::string child_src = (std::filesystem::path{src} / name).string();
            std::string child_dst = (std::filesystem::path{dst} / name).string();
            if (copy_one_archive(child_src, child_dst) != 0) {
                rc = 1;
            }
        }
        // Apply mode/owner/time last so child creation doesn't reset the dir mtime.
        preserve_attrs(src, dst, /*is_symlink=*/false);
        return rc;
    }

    // Regular file (device/fifo/socket fall through to copy_file best-effort).
    auto cr = cfbox::fs::copy_file(src, dst);
    if (!cr) {
        CFBOX_ERR("cp", "cannot copy '%s' to '%s': %s", src.c_str(), dst.c_str(), cr.error().msg.c_str());
        return 1;
    }
    preserve_attrs(src, dst, /*is_symlink=*/false);
    return 0;
}

// Legacy single-file preserve copy (-p without -a): content + mode/owner/time.
auto copy_preserve(const std::string& src, const std::string& dst) -> int {
    auto copy_result = cfbox::fs::copy_file(src, dst);
    if (!copy_result) {
        CFBOX_ERR("cp", "%s", copy_result.error().msg.c_str());
        return 1;
    }
    preserve_attrs(src, dst, /*is_symlink=*/false);
    return 0;
}

} // namespace

auto cp_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'r', false, "recursive"},
        cfbox::args::OptSpec{'p', false, "preserve"},
        cfbox::args::OptSpec{'a', false, "archive"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const bool archive   = parsed.has('a');
    const bool recursive = parsed.has('r') || archive;
    const bool preserve  = parsed.has('p');

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        CFBOX_ERR("cp", "missing file operand");
        return 1;
    }

    std::string dst{pos.back()};
    int rc = 0;

    auto copy_source = [&](const std::string& src) {
        std::string dest = dst;
        if (cfbox::fs::is_directory(dst)) {
            std::filesystem::path src_path{src};
            dest = (std::filesystem::path{dst} / src_path.filename()).string();
        }

        if (archive) {
            // -a copies anything (files, dirs, symlinks) without following links.
            if (copy_one_archive(src, dest) != 0) rc = 1;
            return;
        }

        if (cfbox::fs::is_directory(src)) {
            if (!recursive) {
                CFBOX_ERR("cp", "-r not specified; omitting directory '%s'", src.c_str());
                rc = 1;
                return;
            }
            auto result = cfbox::fs::copy_recursive(src, dest);
            if (!result) {
                CFBOX_ERR("cp", "cannot copy '%s' to '%s': %s", src.c_str(), dest.c_str(), result.error().msg.c_str());
                rc = 1;
            }
        } else if (preserve) {
            if (copy_preserve(src, dest) != 0) rc = 1;
        } else {
            auto result = cfbox::fs::copy_file(src, dest);
            if (!result) {
                CFBOX_ERR("cp", "cannot copy '%s' to '%s': %s", src.c_str(), dest.c_str(), result.error().msg.c_str());
                rc = 1;
            }
        }
    };

    if (pos.size() == 2) {
        copy_source(std::string{pos[0]});
    } else {
        // Multiple sources — destination must be a directory.
        if (!cfbox::fs::is_directory(dst)) {
            CFBOX_ERR("cp", "target '%s' is not a directory", dst.c_str());
            return 1;
        }
        for (std::size_t i = 0; i < pos.size() - 1; ++i) {
            copy_source(std::string{pos[i]});
        }
    }

    return rc;
}
