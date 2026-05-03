#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <string_view>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/terminal.hpp>
#include <cfbox/tui.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "watch",
    .version = CFBOX_VERSION_STRING,
    .one_line = "execute a program periodically",
    .usage   = "watch [-n SECS] COMMAND [ARGS...]",
    .options = "  -n N   seconds between updates (default 2)",
    .extra   = "",
};

auto run_command(const std::vector<std::string>& cmd) -> std::string {
    int pipefd[2];
    if (pipe(pipefd) != 0) return "(pipe failed)\n";

    auto pid = fork();
    if (pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return "(fork failed)\n";
    }

    if (pid == 0) {
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        dup2(pipefd[1], STDERR_FILENO);
        close(pipefd[1]);

        std::vector<char*> argv;
        for (auto& a : cmd) argv.push_back(const_cast<char*>(a.c_str()));
        argv.push_back(nullptr);

        execvp(argv[0], argv.data());
        _exit(127);
    }

    close(pipefd[1]);
    std::string output;
    char buf[4096];
    ssize_t n;
    while ((n = read(pipefd[0], buf, sizeof(buf))) > 0) {
        output.append(buf, static_cast<size_t>(n));
    }
    close(pipefd[0]);

    int status = 0;
    waitpid(pid, &status, 0);
    return output;
}

auto format_time() -> std::string {
    auto now = std::time(nullptr);
    auto tm = std::localtime(&now);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%H:%M:%S", tm);
    return buf;
}

} // anonymous namespace

auto watch_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true, "interval"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    int interval = 2;
    if (auto v = parsed.get('n')) interval = std::stoi(std::string(*v));
    if (interval < 1) interval = 1;

    const auto& pos = parsed.positional();
    if (pos.empty()) {
        std::fprintf(stderr, "cfbox watch: no command specified\n");
        return 1;
    }

    std::vector<std::string> cmd;
    for (const auto& a : pos) cmd.emplace_back(a);

    std::string cmd_str;
    for (size_t i = 0; i < cmd.size(); ++i) {
        if (i > 0) cmd_str += ' ';
        cmd_str += cmd[i];
    }

    cfbox::terminal::RawMode raw_mode;

    while (true) {
        auto output = run_command(cmd);
        auto [rows, cols] = cfbox::terminal::get_size();

        cfbox::terminal::clear_screen();
        cfbox::terminal::move_cursor(1, 1);
        cfbox::terminal::invert_video(true);
        std::printf("Every %ds: %s  %s", interval, cmd_str.c_str(), format_time().c_str());
        cfbox::terminal::clear_line();
        cfbox::terminal::invert_video(false);

        int line = 2;
        int col = 0;
        for (size_t i = 0; i < output.size() && line <= rows; ++i) {
            auto ch = output[i];
            if (ch == '\n') {
                cfbox::terminal::move_cursor(line, col + 1);
                cfbox::terminal::clear_line();
                ++line;
                col = 0;
            } else if (ch == '\r') {
                col = 0;
            } else if (col < cols) {
                cfbox::terminal::move_cursor(line, col + 1);
                std::putchar(ch);
                ++col;
            }
        }
        std::fflush(stdout);

        for (int waited = 0; waited < interval * 1000; waited += 100) {
            auto key = cfbox::tui::read_key(0, 100);
            if (key && (key->type == cfbox::tui::KeyType::Ctrl_C ||
                        key->type == cfbox::tui::KeyType::Escape ||
                        (key->is_char() && key->ch == 'q'))) {
                cfbox::terminal::clear_screen();
                return 0;
            }
        }
    }
}
