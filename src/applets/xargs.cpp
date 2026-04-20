#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/wait.h>
#include <unistd.h>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "xargs",
    .version = CFBOX_VERSION_STRING,
    .one_line = "build and execute command lines from stdin",
    .usage   = "xargs [OPTIONS] [COMMAND [INITIAL-ARGS]...]",
    .options = "  -n N       use at most N args per command\n"
               "  -I REPLACE replace string REPLACE in initial args\n"
               "  -0         null-delimited input\n"
               "  -r         do not run if stdin is empty\n"
               "  -t         print command before executing",
    .extra   = "Default command is echo.",
};
} // namespace

auto xargs_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true, "max-args"},
        cfbox::args::OptSpec{'I', true, "replace"},
        cfbox::args::OptSpec{'0', false},
        cfbox::args::OptSpec{'r', false},
        cfbox::args::OptSpec{'t', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int max_args = 0;
    if (auto n = parsed.get_any('n', "max-args")) {
        max_args = std::stoi(std::string{*n});
    }

    std::string replace_str;
    if (auto r = parsed.get_any('I', "replace")) {
        replace_str = std::string{*r};
        if (max_args == 0) max_args = 1;
    }

    bool null_delim = parsed.has('0') || parsed.has_long("null");
    bool no_run_empty = parsed.has('r') || parsed.has_long("no-run-if-empty");
    bool trace = parsed.has('t') || parsed.has_long("verbose");

    // Determine command + initial args
    std::string command = "echo";
    std::vector<std::string> initial_args;
    const auto& pos = parsed.positional();
    if (!pos.empty()) {
        command = std::string{pos[0]};
        for (std::size_t i = 1; i < pos.size(); ++i) {
            initial_args.emplace_back(pos[i]);
        }
    }

    // Read items from stdin
    auto input = cfbox::io::read_all_stdin();
    if (!input) {
        std::fprintf(stderr, "cfbox xargs: %s\n", input.error().msg.c_str());
        return 1;
    }

    std::vector<std::string> items;
    if (null_delim) {
        std::string item;
        for (char c : *input) {
            if (c == '\0') {
                if (!item.empty()) items.push_back(std::move(item));
                item.clear();
            } else {
                item += c;
            }
        }
        if (!item.empty()) items.push_back(std::move(item));
    } else {
        std::string item;
        for (char c : *input) {
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                if (!item.empty()) { items.push_back(std::move(item)); item.clear(); }
            } else {
                item += c;
            }
        }
        if (!item.empty()) items.push_back(std::move(item));
    }

    if (items.empty() && no_run_empty) return 0;

    auto run_command = [&](const std::vector<std::string>& args) -> int {
        std::vector<std::string> all_args_storage;
        std::vector<char*> exec_args;

        all_args_storage.push_back(command);
        exec_args.push_back(all_args_storage.back().data());

        if (!replace_str.empty()) {
            for (const auto& a : initial_args) {
                std::string expanded = a;
                auto rpos = expanded.find(replace_str);
                while (rpos != std::string::npos) {
                    expanded.replace(rpos, replace_str.size(), args[0]);
                    rpos = expanded.find(replace_str, rpos + args[0].size());
                }
                all_args_storage.push_back(std::move(expanded));
                exec_args.push_back(all_args_storage.back().data());
            }
        } else {
            for (const auto& a : initial_args) {
                all_args_storage.push_back(a);
                exec_args.push_back(all_args_storage.back().data());
            }
            for (const auto& a : args) {
                all_args_storage.push_back(a);
                exec_args.push_back(all_args_storage.back().data());
            }
        }
        exec_args.push_back(nullptr);

        if (trace) {
            std::fprintf(stderr, "%s", command.c_str());
            for (std::size_t i = 1; exec_args[i]; ++i) {
                std::fprintf(stderr, " %s", exec_args[i]);
            }
            std::fputc('\n', stderr);
        }

        pid_t pid = fork();
        if (pid < 0) {
            std::fprintf(stderr, "cfbox xargs: fork: %s\n", std::strerror(errno));
            return 1;
        }
        if (pid == 0) {
            execvp(command.c_str(), exec_args.data());
            std::fprintf(stderr, "cfbox xargs: %s: %s\n", command.c_str(), std::strerror(errno));
            _exit(127);
        }
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) return WEXITSTATUS(status);
        if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
        return 1;
    };

    int rc = 0;
    std::vector<std::string> batch;
    for (const auto& item : items) {
        batch.push_back(item);
        if (max_args > 0 && static_cast<int>(batch.size()) >= max_args) {
            int r = run_command(batch);
            if (r > rc) rc = r;
            batch.clear();
        }
    }
    if (!batch.empty()) {
        int r = run_command(batch);
        if (r > rc) rc = r;
    }
    return rc;
}
