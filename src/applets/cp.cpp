#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/io.hpp>

namespace {

auto copy_preserve(const std::string& src, const std::string& dst) -> int {
    // Copy the file first
    auto copy_result = cfbox::fs::copy_file(src, dst);
    if (!copy_result) {
        std::fprintf(stderr, "cfbox cp: %s\n", copy_result.error().msg.c_str());
        return 1;
    }

    // Preserve permissions
    auto status_result = cfbox::fs::status(src);
    if (status_result) {
        auto perm_result = cfbox::fs::permissions(dst, status_result->permissions());
        if (!perm_result) {
            // non-fatal
        }
    }

    return 0;
}

} // namespace

auto cp_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'r', false},
        cfbox::args::OptSpec{'p', false},
    });

    bool recursive = parsed.has('r');
    bool preserve = parsed.has('p');

    const auto& pos = parsed.positional();
    if (pos.size() < 2) {
        std::fprintf(stderr, "cfbox cp: missing file operand\n");
        return 1;
    }

    // Last argument is the destination
    std::string dst{pos.back()};
    int rc = 0;

    if (pos.size() == 2) {
        // Single source
        std::string src{pos[0]};

        if (cfbox::fs::is_directory(src)) {
            if (!recursive) {
                std::fprintf(stderr, "cfbox cp: -r not specified; omitting directory '%s'\n",
                             src.c_str());
                return 1;
            }
            // Determine destination path
            std::string dest = dst;
            if (cfbox::fs::is_directory(dst)) {
                // Copy into directory
                std::filesystem::path src_path{src};
                dest = (std::filesystem::path{dst} / src_path.filename()).string();
            }
            auto result = cfbox::fs::copy_recursive(src, dest);
            if (!result) {
                std::fprintf(stderr, "cfbox cp: cannot copy '%s' to '%s': %s\n",
                             src.c_str(), dest.c_str(), result.error().msg.c_str());
                rc = 1;
            }
        } else {
            std::string dest = dst;
            if (cfbox::fs::is_directory(dst)) {
                std::filesystem::path src_path{src};
                dest = (std::filesystem::path{dst} / src_path.filename()).string();
            }
            if (preserve) {
                rc = copy_preserve(src, dest);
            } else {
                auto result = cfbox::fs::copy_file(src, dest);
                if (!result) {
                    std::fprintf(stderr, "cfbox cp: cannot copy '%s' to '%s': %s\n",
                                 src.c_str(), dest.c_str(), result.error().msg.c_str());
                    rc = 1;
                }
            }
        }
    } else {
        // Multiple sources — destination must be a directory
        if (!cfbox::fs::is_directory(dst)) {
            std::fprintf(stderr, "cfbox cp: target '%s' is not a directory\n", dst.c_str());
            return 1;
        }

        for (std::size_t i = 0; i < pos.size() - 1; ++i) {
            std::string src{pos[i]};
            std::filesystem::path src_path{src};
            std::string dest = (std::filesystem::path{dst} / src_path.filename()).string();

            if (cfbox::fs::is_directory(src)) {
                if (!recursive) {
                    std::fprintf(stderr, "cfbox cp: -r not specified; omitting directory '%s'\n",
                                 src.c_str());
                    rc = 1;
                    continue;
                }
                auto result = cfbox::fs::copy_recursive(src, dest);
                if (!result) {
                    std::fprintf(stderr, "cfbox cp: cannot copy '%s' to '%s': %s\n",
                                 src.c_str(), dest.c_str(), result.error().msg.c_str());
                    rc = 1;
                }
            } else {
                if (preserve) {
                    if (copy_preserve(src, dest) != 0) rc = 1;
                } else {
                    auto result = cfbox::fs::copy_file(src, dest);
                    if (!result) {
                        std::fprintf(stderr, "cfbox cp: cannot copy '%s' to '%s': %s\n",
                                     src.c_str(), dest.c_str(), result.error().msg.c_str());
                        rc = 1;
                    }
                }
            }
        }
    }

    return rc;
}
