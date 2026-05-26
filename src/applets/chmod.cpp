#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/fs_util.hpp>
#include <cfbox/help.hpp>
#include <cfbox/error.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "chmod",
    .version = CFBOX_VERSION_STRING,
    .one_line = "change file mode bits",
    .usage   = "chmod [-R] [-v] MODE FILE...",
    .options = "  -R          change files and directories recursively\n"
               "  -v          output a diagnostic for every file processed\n"
               "  --reference=RFILE  use RFILE's mode instead of MODE value",
    .extra   = "MODE can be octal (755) or symbolic (u+x,go-w,a=rX).",
};

auto parse_octal(std::string_view s) -> std::optional<std::filesystem::perms> {
    if (s.empty() || s.size() > 4) return std::nullopt;
    for (char c : s) if (c < '0' || c > '7') return std::nullopt;
    unsigned mode = 0;
    for (char c : s) mode = mode * 8 + (c - '0');
    return static_cast<std::filesystem::perms>(mode);
}

auto apply_symbolic(std::filesystem::perms current, std::string_view spec) -> std::optional<std::filesystem::perms> {
    auto who_end = spec.find_first_of("-+=");
    if (who_end == std::string_view::npos) return std::nullopt;

    auto who = spec.substr(0, who_end);
    if (who.empty()) who = "a";

    char op = spec[who_end];
    auto rest = spec.substr(who_end + 1);

    auto bits_for = [&](char perm) -> std::filesystem::perms {
        switch (perm) {
        case 'r': return std::filesystem::perms::owner_read | std::filesystem::perms::group_read | std::filesystem::perms::others_read;
        case 'w': return std::filesystem::perms::owner_write | std::filesystem::perms::group_write | std::filesystem::perms::others_write;
        case 'x': return std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
        case 'X': return std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec;
        default: return std::filesystem::perms::none;
        }
    };

    std::filesystem::perms result = current;
    for (char w : who) {
        for (char p : rest) {
            auto bits = bits_for(p);
            if (bits == std::filesystem::perms::none) continue;

            std::filesystem::perms mask;
            switch (w) {
            case 'u': mask = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec; break;
            case 'g': mask = std::filesystem::perms::group_read | std::filesystem::perms::group_write | std::filesystem::perms::group_exec; break;
            case 'o': mask = std::filesystem::perms::others_read | std::filesystem::perms::others_write | std::filesystem::perms::others_exec; break;
            case 'a': mask = std::filesystem::perms::owner_read | std::filesystem::perms::owner_write | std::filesystem::perms::owner_exec
                           | std::filesystem::perms::group_read | std::filesystem::perms::group_write | std::filesystem::perms::group_exec
                           | std::filesystem::perms::others_read | std::filesystem::perms::others_write | std::filesystem::perms::others_exec; break;
            default: continue;
            }

            auto target = bits & mask;
            switch (op) {
            case '+': result |= target; break;
            case '-': result &= ~target; break;
            case '=': result = (result & ~mask) | target; break;
            }
        }
    }
    return result;
}

auto chmod_one(const std::string& path, std::filesystem::perms mode, bool verbose) -> int {
    auto result = cfbox::fs::permissions(path, mode);
    if (!result) {
        CFBOX_ERR("chmod", "%s: %s", path.c_str(), result.error().msg.c_str());
        return 1;
    }
    if (verbose) std::printf("mode of '%s' changed\n", path.c_str());
    return 0;
}

} // namespace

auto chmod_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'R', false, "recursive"},
        cfbox::args::OptSpec{'v', false, "verbose"},
        cfbox::args::OptSpec{'\0', true, "reference"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool recursive = parsed.has('R');
    bool verbose = parsed.has('v');
    const auto& pos = parsed.positional();

    std::filesystem::perms target_mode;
    int files_start = 0;

    if (parsed.has_long("reference")) {
        auto rfile = parsed.get_long("reference");
        if (!rfile) {
            CFBOX_ERR("chmod", "--reference requires an argument");
            return 2;
        }
        std::string rfile_str(*rfile);
        auto st = cfbox::fs::status(rfile_str);
        if (!st) {
            CFBOX_ERR("chmod", "%s: %s", rfile_str.c_str(), st.error().msg.c_str());
            return 1;
        }
        target_mode = st->permissions();
        files_start = 0;
        if (pos.empty()) {
            CFBOX_ERR("chmod", "missing operand");
            return 2;
        }
    } else {
        if (pos.size() < 2) {
            CFBOX_ERR("chmod", "missing operand");
            return 2;
        }
        auto octal = parse_octal(pos[0]);
        if (octal) {
            target_mode = *octal;
        } else {
            int rc = 0;
            for (size_t i = 1; i < pos.size(); i++) {
                std::string path(pos[i]);
                auto st = cfbox::fs::status(path);
                if (!st) {
                    CFBOX_ERR("chmod", "%s: %s", path.c_str(), st.error().msg.c_str());
                    rc = 1;
                    continue;
                }
                auto new_mode = apply_symbolic(st->permissions(), pos[0]);
                if (!new_mode) {
                    CFBOX_ERR("chmod", "invalid mode: %s", std::string(pos[0]).c_str());
                    return 2;
                }
                if (chmod_one(path, *new_mode, verbose) != 0) rc = 1;
            }
            return rc;
        }
        files_start = 1;
    }

    int rc = 0;
    for (size_t i = files_start; i < pos.size(); i++) {
        cfbox::fs::for_each_entry(pos[i], recursive, [&](const std::string& p) {
            if (chmod_one(p, target_mode, verbose) != 0) rc = 1;
        });
    }
    return rc;
}
