#pragma once

#include <cstdio>
#include <string>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>

namespace cfbox::terminal {

class RawMode {
    int fd_;
    termios orig_;
public:
    explicit RawMode(int fd = STDIN_FILENO) : fd_(fd) {
        tcgetattr(fd_, &orig_);
        auto raw = orig_;
        raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
        raw.c_oflag &= ~(OPOST);
        raw.c_cflag |= (CS8);
        raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(fd_, TCSAFLUSH, &raw);
    }
    ~RawMode() { tcsetattr(fd_, TCSAFLUSH, &orig_); }
    RawMode(const RawMode&) = delete;
    RawMode& operator=(const RawMode&) = delete;
};

[[nodiscard]] inline auto get_size(int fd = STDOUT_FILENO) noexcept -> std::pair<int, int> {
    struct winsize ws;
    if (ioctl(fd, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0) {
        return {static_cast<int>(ws.ws_row), static_cast<int>(ws.ws_col)};
    }
    return {24, 80};
}

inline auto clear_screen() noexcept -> void { std::fwrite("\033[2J\033[H", 1, 7, stdout); }
inline auto hide_cursor() noexcept -> void { std::fwrite("\033[?25l", 1, 6, stdout); }
inline auto show_cursor() noexcept -> void { std::fwrite("\033[?25h", 1, 6, stdout); }
inline auto move_cursor(int row, int col) noexcept -> void { std::printf("\033[%d;%dH", row, col); }
inline auto clear_line() noexcept -> void { std::fwrite("\033[2K", 1, 3, stdout); }
inline auto enter_alt_screen() noexcept -> void { std::fwrite("\033[?1049h", 1, 8, stdout); }
inline auto leave_alt_screen() noexcept -> void { std::fwrite("\033[?1049l", 1, 8, stdout); }
inline auto invert_video(bool on) noexcept -> void { std::fwrite(on ? "\033[7m" : "\033[27m", 1, on ? 4 : 5, stdout); }
inline auto bold(bool on) noexcept -> void { std::fwrite(on ? "\033[1m" : "\033[22m", 1, on ? 4 : 5, stdout); }
inline auto reset_attr() noexcept -> void { std::fwrite("\033[0m", 1, 3, stdout); }

} // namespace cfbox::terminal
