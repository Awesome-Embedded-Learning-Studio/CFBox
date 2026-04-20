#include <cstdio>
#include <grp.h>
#include <pwd.h>
#include <string>
#include <unistd.h>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "id",
    .version = CFBOX_VERSION_STRING,
    .one_line = "print real and effective user and group IDs",
    .usage   = "id [-u|-g|-G] [-n] [-r]",
    .options = "  -u     print only effective UID\n"
               "  -g     print only effective GID\n"
               "  -G     print all group IDs\n"
               "  -n     print names instead of numbers\n"
               "  -r     print real ID instead of effective",
    .extra   = "",
};
} // namespace

auto id_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'u', false},
        cfbox::args::OptSpec{'g', false},
        cfbox::args::OptSpec{'G', false},
        cfbox::args::OptSpec{'n', false},
        cfbox::args::OptSpec{'r', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool opt_u = parsed.has('u');
    bool opt_g = parsed.has('g');
    bool opt_G = parsed.has('G');
    bool opt_n = parsed.has('n');
    bool opt_r = parsed.has('r');

    uid_t uid = opt_r ? getuid() : geteuid();
    gid_t gid = opt_r ? getgid() : getegid();

    auto get_username = [](uid_t id) -> std::string {
        auto* pw = getpwuid(id);
        return pw ? std::string{pw->pw_name} : std::to_string(static_cast<unsigned>(id));
    };

    auto get_groupname = [](gid_t id) -> std::string {
        auto* gr = getgrgid(id);
        return gr ? std::string{gr->gr_name} : std::to_string(static_cast<unsigned>(id));
    };

    if (opt_u) {
        if (opt_n) std::puts(get_username(uid).c_str());
        else std::printf("%u\n", static_cast<unsigned>(uid));
        return 0;
    }

    if (opt_g) {
        if (opt_n) std::puts(get_groupname(gid).c_str());
        else std::printf("%u\n", static_cast<unsigned>(gid));
        return 0;
    }

    if (opt_G) {
        std::vector<gid_t> groups;
        int ngroups = getgroups(0, nullptr);
        if (ngroups > 0) {
            groups.resize(static_cast<std::size_t>(ngroups));
            if (getgroups(ngroups, groups.data()) < 0) groups.clear();
        }

        bool first = true;
        auto print_one = [&](gid_t g) {
            if (!first) std::fputc(' ', stdout);
            first = false;
            if (opt_n) {
                std::fputs(get_groupname(g).c_str(), stdout);
            } else {
                std::printf("%u", static_cast<unsigned>(g));
            }
        };

        print_one(gid);
        for (auto g : groups) {
            if (g != gid) print_one(g);
        }
        std::fputc('\n', stdout);
        return 0;
    }

    // Default: print full format
    std::printf("uid=%u(%s) gid=%u(%s)",
                static_cast<unsigned>(uid), get_username(uid).c_str(),
                static_cast<unsigned>(gid), get_groupname(gid).c_str());

    std::vector<gid_t> groups;
    int ngroups = getgroups(0, nullptr);
    if (ngroups > 0) {
        groups.resize(static_cast<std::size_t>(ngroups));
        int ret = getgroups(ngroups, groups.data());
        if (ret < 0) groups.clear();
        std::fputs(" groups=", stdout);
        bool first = true;
        for (auto g : groups) {
            if (!first) std::fputc(',', stdout);
            first = false;
            std::printf("%u(%s)", static_cast<unsigned>(g), get_groupname(g).c_str());
        }
    }
    std::fputc('\n', stdout);
    return 0;
}
