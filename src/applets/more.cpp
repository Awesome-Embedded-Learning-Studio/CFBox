#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/terminal.hpp>
#include <cfbox/tui.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "more",
    .version = CFBOX_VERSION_STRING,
    .one_line = "file perusal filter for crt viewing",
    .usage   = "more [FILE]",
    .options = "",
    .extra   = "Space=next page  Enter=next line  q=quit",
};

auto read_lines(std::FILE* f) -> std::vector<std::string> {
    std::vector<std::string> lines;
    char buf[4096];
    while (std::fgets(buf, sizeof(buf), f)) {
        auto len = std::strlen(buf);
        while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r')) {
            buf[--len] = '\0';
        }
        lines.emplace_back(buf);
    }
    return lines;
}

} // anonymous namespace

auto more_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    std::string filename = pos.empty() ? "" : std::string(pos[0]);

    std::FILE* f = stdin;
    if (!filename.empty() && filename != "-") {
        f = std::fopen(filename.c_str(), "r");
        if (!f) {
            std::fprintf(stderr, "cfbox more: cannot open %s\n", filename.c_str());
            return 1;
        }
    }

    auto lines = read_lines(f);
    if (f != stdin) std::fclose(f);

    if (lines.empty()) return 0;

    // Check if output is a terminal
    if (!isatty(STDOUT_FILENO)) {
        for (const auto& line : lines) std::printf("%s\n", line.c_str());
        return 0;
    }

    auto [rows, cols] = cfbox::terminal::get_size();
    int usable_rows = rows - 1; // Leave room for status line
    if (usable_rows < 1) usable_rows = 1;

    cfbox::terminal::RawMode raw_mode;
    std::size_t top_line = 0;

    while (top_line < lines.size()) {
        cfbox::terminal::clear_screen();
        cfbox::terminal::move_cursor(1, 1);

        // Display a page
        auto end = std::min(top_line + static_cast<std::size_t>(usable_rows), lines.size());
        for (auto i = top_line; i < end; ++i) {
            cfbox::terminal::move_cursor(static_cast<int>(i - top_line) + 1, 1);
            const auto& line = lines[i];
            // Truncate to terminal width
            auto print_len = std::min(static_cast<int>(line.size()), cols);
            std::printf("%.*s", print_len, line.c_str());
            cfbox::terminal::clear_line();
        }

        // Status line
        cfbox::terminal::invert_video(true);
        cfbox::terminal::move_cursor(rows, 1);
        std::printf("--More--(%zu%%)", std::min(end * 100 / lines.size(), static_cast<std::size_t>(100)));
        cfbox::terminal::clear_line();
        cfbox::terminal::invert_video(false);
        std::fflush(stdout);

        // Wait for key
        while (true) {
            auto key = cfbox::tui::read_key(0, -1);
            if (!key) continue;

            if (key->is_char() && key->ch == 'q') return 0;
            if (key->type == cfbox::tui::KeyType::Escape) return 0;

            if (key->type == cfbox::tui::KeyType::Enter) {
                top_line += 1;
                break;
            }
            if ((key->is_char() && key->ch == ' ') ||
                key->type == cfbox::tui::KeyType::PageDown) {
                top_line += static_cast<std::size_t>(usable_rows);
                break;
            }
            if (key->type == cfbox::tui::KeyType::PageUp) {
                if (top_line >= static_cast<std::size_t>(usable_rows)) {
                    top_line -= static_cast<std::size_t>(usable_rows);
                } else {
                    top_line = 0;
                }
                break;
            }
        }
    }

    cfbox::terminal::clear_screen();
    return 0;
}
