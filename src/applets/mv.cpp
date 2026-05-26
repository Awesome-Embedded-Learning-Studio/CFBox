#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "mv",
    .version = CFBOX_VERSION_STRING,
    .one_line = "move or rename files",
    .usage   = "mv [OPTIONS] SOURCE... DEST",
    .options = "  -f     do not prompt before overwriting",
    .extra   = "",
};

auto do_move(const std::string& src, const std::string& dst, bool force) -> int {
    if (!cfbox::fs::exists(src)) {
        CFBOX_ERR("mv", "cannot stat '%s': No such file or directory", src.c_str());
        return 1;
    }

    std::string dest = dst;
    if (cfbox::fs::is_directory(dst)) {
        std::filesystem::path src_path{src};
        dest = (std::filesystem::path{dst} / src_path.filename()).string();
    }

    if (cfbox::fs::exists(dest) && !force) {
        CFBOX_ERR("mv", "overwrite '%s'? (skipped, use -f to force)", dest.c_str());
        return 1;
    }

    // Try rename first (same filesystem)
    auto rename_result = cfbox::fs::rename(src, dest);
    if (rename_result) {
        return 0;
    }

    // Fallback: copy + remove
    if (cfbox::fs::is_directory(src)) {
        auto copy_result = cfbox::fs::copy_recursive(src, dest);
        if (!copy_result) {
            CFBOX_ERR("mv", "cannot move '%s' to '%s': %s", src.c_str(), dest.c_str(), copy_result.error().msg.c_str());
            return 1;
        }
        auto rm_result = cfbox::fs::remove_all(src);
        if (!rm_result) {
            CFBOX_ERR("mv", "cannot remove '%s': %s", src.c_str(), rm_result.error().msg.c_str());
            return 1;
        }
    } else {
        auto copy_result = cfbox::fs::copy_file(src, dest);
        if (!copy_result) {
            CFBOX_ERR("mv", "cannot move '%s' to '%s': %s", src.c_str(), dest.c_str(), copy_result.error().msg.c_str());
            return 1;
        }
        auto rm_result = cfbox::fs::remove_single(src);
        if (!rm_result) {
            CFBOX_ERR("mv", "cannot remove '%s': %s", src.c_str(), rm_result.error().msg.c_str());
            return 1;
        }
    }

    return 0;
}

} // namespace

auto mv_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'f', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool force = parsed.has('f');

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        CFBOX_ERR("mv", "missing file operand");
        return 1;
    }

    std::string dst{pos.back()};
    int rc = 0;

    if (pos.size() == 2) {
        return do_move(std::string{pos[0]}, dst, force);
    }

    // Multiple sources — destination must be a directory
    if (!cfbox::fs::is_directory(dst)) {
        CFBOX_ERR("mv", "target '%s' is not a directory", dst.c_str());
        return 1;
    }

    for (std::size_t i = 0; i < pos.size() - 1; ++i) {
        if (do_move(std::string{pos[i]}, dst, force) != 0) {
            rc = 1;
        }
    }
    return rc;
}
