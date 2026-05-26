#include <cerrno>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <grp.h>
#include <pwd.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "chown",
    .version = CFBOX_VERSION_STRING,
    .one_line = "change file owner and group",
    .usage   = "chown [-R] [-v] [OWNER][:GROUP] FILE...",
    .options = "  -R          operate on files and directories recursively\n"
               "  -v          output a diagnostic for every file processed\n"
               "  --reference=RFILE  use RFILE's owner and group",
    .extra   = "OWNER and GROUP can be names or numeric IDs.",
};

struct OwnerSpec {
    uid_t uid = static_cast<uid_t>(-1);
    gid_t gid = static_cast<gid_t>(-1);
    bool set_uid = false;
    bool set_gid = false;
};

auto parse_owner_spec(std::string_view spec) -> OwnerSpec {
    OwnerSpec result;
    auto colon = spec.find(':');

    if (colon == std::string_view::npos) {
        result.set_uid = true;
        std::string s(spec);
        auto* pw = getpwnam(s.c_str());
        if (pw) result.uid = pw->pw_uid;
        else result.uid = static_cast<uid_t>(std::strtoul(s.c_str(), nullptr, 10));
    } else {
        auto owner_part = spec.substr(0, colon);
        auto group_part = spec.substr(colon + 1);

        if (!owner_part.empty()) {
            result.set_uid = true;
            std::string os(owner_part);
            auto* pw = getpwnam(os.c_str());
            if (pw) result.uid = pw->pw_uid;
            else result.uid = static_cast<uid_t>(std::strtoul(os.c_str(), nullptr, 10));
        }
        if (!group_part.empty()) {
            result.set_gid = true;
            std::string gs(group_part);
            auto* gr = getgrnam(gs.c_str());
            if (gr) result.gid = gr->gr_gid;
            else result.gid = static_cast<gid_t>(std::strtoul(gs.c_str(), nullptr, 10));
        }
    }
    return result;
}

auto chown_one(const std::string& path, uid_t uid, gid_t gid, bool verbose) -> int {
    if (::chown(path.c_str(), uid, gid) != 0) {
        std::fprintf(stderr, "cfbox chown: %s: %s\n", path.c_str(), std::strerror(errno));
        return 1;
    }
    if (verbose) std::printf("ownership of '%s' changed\n", path.c_str());
    return 0;
}

} // namespace

auto chown_main(int argc, char* argv[]) -> int {
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

    OwnerSpec owner;
    int files_start = 0;

    if (parsed.has_long("reference")) {
        auto rfile = parsed.get_long("reference");
        if (!rfile) {
            std::fprintf(stderr, "cfbox chown: --reference requires an argument\n");
            return 2;
        }
        struct stat st;
        std::string rfile_str(*rfile);
        if (stat(rfile_str.c_str(), &st) != 0) {
            std::fprintf(stderr, "cfbox chown: %s: %s\n", rfile_str.c_str(), std::strerror(errno));
            return 1;
        }
        owner.uid = st.st_uid;
        owner.gid = st.st_gid;
        owner.set_uid = true;
        owner.set_gid = true;
        files_start = 0;
        if (pos.empty()) {
            std::fprintf(stderr, "cfbox chown: missing operand\n");
            return 2;
        }
    } else {
        if (pos.size() < 2) {
            std::fprintf(stderr, "cfbox chown: missing operand\n");
            return 2;
        }
        owner = parse_owner_spec(pos[0]);
        files_start = 1;
    }

    int rc = 0;
    for (size_t i = files_start; i < pos.size(); i++) {
        std::string path(pos[i]);
        auto apply = [&](const std::string& p) {
            uid_t uid = owner.set_uid ? owner.uid : static_cast<uid_t>(-1);
            gid_t gid = owner.set_gid ? owner.gid : static_cast<gid_t>(-1);
            return chown_one(p, uid, gid, verbose);
        };

        if (recursive && std::filesystem::is_directory(path)) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::recursive_directory_iterator(path, ec)) {
                if (ec) continue;
                if (apply(entry.path().string()) != 0) rc = 1;
            }
        }
        if (apply(path) != 0) rc = 1;
    }
    return rc;
}
