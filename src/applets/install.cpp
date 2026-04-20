#include <cstdio>
#include <cstring>
#include <string>
#include <filesystem>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "install",
    .version = CFBOX_VERSION_STRING,
    .one_line = "copy files and set attributes",
    .usage   = "install [OPTIONS] SOURCE... DEST",
    .options = "  -d         create directories\n"
               "  -m MODE    set permission mode (octal)\n"
               "  -t DIR     install into DIR",
    .extra   = "",
};
} // namespace

auto install_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'d', false},
        cfbox::args::OptSpec{'m', true, "mode"},
        cfbox::args::OptSpec{'t', true, "target-directory"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool mkdir_mode = parsed.has('d');
    auto mode_str = parsed.get_any('m', "mode");
    auto target_dir = parsed.get_any('t', "target-directory");
    const auto& pos = parsed.positional();

    auto parse_mode = [](const std::string& s) -> std::filesystem::perms {
        unsigned long m = std::stoul(s, nullptr, 8);
        return static_cast<std::filesystem::perms>(m);
    };

    std::filesystem::perms mode = std::filesystem::perms::owner_read |
                                   std::filesystem::perms::owner_write |
                                   std::filesystem::perms::group_read |
                                   std::filesystem::perms::others_read;
    if (mode_str) {
        try { mode = parse_mode(std::string{*mode_str}); }
        catch (...) {
            std::fprintf(stderr, "cfbox install: invalid mode '%.*s'\n",
                         static_cast<int>(mode_str->size()), mode_str->data());
            return 1;
        }
    }

    if (mkdir_mode) {
        if (pos.empty()) {
            std::fprintf(stderr, "cfbox install: missing operand\n");
            return 1;
        }
        for (auto p : pos) {
            std::error_code ec;
            std::filesystem::create_directories(std::filesystem::path{p}, ec);
            if (ec) {
                std::fprintf(stderr, "cfbox install: cannot create directory '%.*s': %s\n",
                             static_cast<int>(p.size()), p.data(), ec.message().c_str());
                return 1;
            }
            std::filesystem::permissions(std::filesystem::path{p}, mode, ec);
        }
        return 0;
    }

    if (pos.empty()) {
        std::fprintf(stderr, "cfbox install: missing operand\n");
        return 1;
    }

    std::string dest;
    std::vector<std::string_view> sources;
    if (target_dir) {
        dest = std::string{*target_dir};
        sources = pos;
    } else {
        if (pos.size() < 2) {
            std::fprintf(stderr, "cfbox install: missing destination file operand\n");
            return 1;
        }
        dest = std::string{pos.back()};
        for (std::size_t i = 0; i + 1 < pos.size(); ++i) {
            sources.push_back(pos[i]);
        }
    }

    int rc = 0;
    for (auto src : sources) {
        std::string src_path{src};
        std::string dst_path = dest;

        if (!target_dir && sources.size() == 1) {
            // Single source, dest can be a directory or new filename
        } else {
            // Multiple sources or -t: dest must be a directory
            auto filename = std::filesystem::path{src_path}.filename();
            dst_path = (std::filesystem::path{dest} / filename).string();
        }

        auto result = cfbox::io::read_all(src_path);
        if (!result) {
            std::fprintf(stderr, "cfbox install: %s\n", result.error().msg.c_str());
            rc = 1;
            continue;
        }
        auto wresult = cfbox::io::write_all(dst_path, *result);
        if (!wresult) {
            std::fprintf(stderr, "cfbox install: %s\n", wresult.error().msg.c_str());
            rc = 1;
            continue;
        }
        std::error_code ec;
        std::filesystem::permissions(std::filesystem::path{dst_path}, mode, ec);
    }
    return rc;
}
