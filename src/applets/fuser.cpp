#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <signal.h>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "fuser",
    .version = CFBOX_VERSION_STRING,
    .one_line = "identify processes using files or sockets",
    .usage   = "fuser [-k] FILE...",
    .options = "  -k     kill processes accessing the file\n"
               "  -v     verbose output",
    .extra   = "",
};

} // anonymous namespace

auto fuser_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'k', false, "kill"},
        cfbox::args::OptSpec{'v', false, "verbose"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool do_kill = parsed.has('k') || parsed.has_long("kill");
    bool verbose = parsed.has('v') || parsed.has_long("verbose");
    const auto& targets = parsed.positional();
    if (targets.empty()) {
        std::fprintf(stderr, "cfbox fuser: no file specified\n");
        return 1;
    }

    int rc = 1;

    for (const auto& target : targets) {
        auto target_str = std::string(target);
        struct stat target_stat {};
        if (stat(target_str.c_str(), &target_stat) != 0) {
            std::fprintf(stderr, "cfbox fuser: cannot stat %s\n", target_str.c_str());
            continue;
        }

        dev_t target_dev = target_stat.st_dev;
        ino_t target_ino = target_stat.st_ino;
        std::vector<pid_t> found_pids;

        std::error_code ec;
        for (const auto& proc_entry : std::filesystem::directory_iterator("/proc", ec)) {
            if (!proc_entry.is_directory()) continue;
            auto name = proc_entry.path().filename().string();
            if (name.empty() || name[0] < '0' || name[0] > '9') continue;

            pid_t pid = static_cast<pid_t>(std::stoi(name));
            auto fd_dir = proc_entry.path() / "fd";

            for (const auto& fd_entry : std::filesystem::directory_iterator(fd_dir, ec)) {
                auto link = std::filesystem::read_symlink(fd_entry.path(), ec);
                if (ec) continue;

                struct stat fd_stat {};
                if (stat(fd_entry.path().c_str(), &fd_stat) != 0) continue;
                if (fd_stat.st_dev == target_dev && fd_stat.st_ino == target_ino) {
                    found_pids.push_back(pid);
                    break;
                }
            }
        }

        if (found_pids.empty()) continue;
        rc = 0;

        if (verbose) {
            std::printf("%s:", target_str.c_str());
            for (auto p : found_pids) std::printf(" %d", p);
            std::printf("\n");
        }

        if (do_kill) {
            for (auto p : found_pids) {
                if (::kill(p, SIGKILL) != 0) {
                    std::fprintf(stderr, "cfbox fuser: cannot kill %d\n", p);
                }
            }
        } else if (!verbose) {
            for (auto p : found_pids) std::printf("%d ", p);
            std::printf("\n");
        }
    }

    return rc;
}
