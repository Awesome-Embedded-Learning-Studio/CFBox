#pragma once

#include <atomic>
#include <cstdio>
#include <cstring>
#include <optional>
#include <poll.h>
#include <string>
#include <string_view>
#include <unistd.h>
#include <vector>

#include <cfbox/terminal.hpp>

namespace cfbox::tui {

// --- Key representation ---

enum class KeyType : unsigned short {
    Char,
    Escape,
    Enter,
    Tab,
    Backspace,
    Delete,
    Up, Down, Left, Right,
    Home, End,
    PageUp, PageDown,
    F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
    Ctrl_A, Ctrl_B, Ctrl_C, Ctrl_D, Ctrl_E, Ctrl_F,
    Ctrl_G, Ctrl_H, Ctrl_K, Ctrl_L, Ctrl_N, Ctrl_O,
    Ctrl_P, Ctrl_Q, Ctrl_R, Ctrl_S, Ctrl_T, Ctrl_U,
    Ctrl_V, Ctrl_W, Ctrl_X, Ctrl_Y, Ctrl_Z,
    Unknown,
};

struct Key {
    KeyType type = KeyType::Unknown;
    char32_t ch = 0;

    auto is_char() const -> bool { return type == KeyType::Char; }
    auto is_quit() const -> bool {
        return type == KeyType::Escape || type == KeyType::Ctrl_C || type == KeyType::Ctrl_Q;
    }
    auto ctrl_char() const -> char {
        if (type >= KeyType::Ctrl_A && type <= KeyType::Ctrl_Z) {
            auto idx = static_cast<int>(type) - static_cast<int>(KeyType::Ctrl_A);
            return static_cast<char>('a' + idx);
        }
        return '\0';
    }
};

inline auto read_key(int fd = 0, int timeout_ms = -1) -> std::optional<Key> {
    struct pollfd pfd{fd, POLLIN, 0};
    int pret = poll(&pfd, 1, timeout_ms);
    if (pret <= 0) return std::nullopt;

    unsigned char buf[16];
    auto n = ::read(fd, buf, 1);
    if (n <= 0) return std::nullopt;

    auto c = static_cast<unsigned int>(buf[0]);

    // Ctrl+letter: 1-26
    if (c >= 1 && c <= 26) {
        auto idx = static_cast<int>(c) - 1;
        Key k;
        k.type = static_cast<KeyType>(static_cast<int>(KeyType::Ctrl_A) + idx);
        return k;
    }

    if (c == 13 || c == 10) return Key{KeyType::Enter, 0};
    if (c == 9) return Key{KeyType::Tab, 0};
    if (c == 127 || c == 8) return Key{KeyType::Backspace, 0};

    if (c == 27) {
        auto seq0 = read_key(fd, 10);
        if (!seq0) return Key{KeyType::Escape, 0};

        auto c1 = static_cast<unsigned int>(seq0->ch);

        if (c1 != '[' && c1 != 'O') {
            return Key{KeyType::Escape, 0};
        }

        auto seq1 = read_key(fd, 10);
        if (!seq1) return Key{KeyType::Escape, 0};

        auto c2 = static_cast<unsigned int>(seq1->ch);

        if (c1 == '[') {
            switch (c2) {
                case 'A': return Key{KeyType::Up, 0};
                case 'B': return Key{KeyType::Down, 0};
                case 'C': return Key{KeyType::Right, 0};
                case 'D': return Key{KeyType::Left, 0};
                case 'H': return Key{KeyType::Home, 0};
                case 'F': return Key{KeyType::End, 0};
                case '5': {
                    auto next = read_key(fd, 10);
                    if (next && next->ch == '~') return Key{KeyType::PageUp, 0};
                    return Key{KeyType::Unknown, 0};
                }
                case '6': {
                    auto next = read_key(fd, 10);
                    if (next && next->ch == '~') return Key{KeyType::PageDown, 0};
                    return Key{KeyType::Unknown, 0};
                }
                default: return Key{KeyType::Unknown, 0};
            }
        }
        if (c1 == 'O') {
            switch (c2) {
                case 'P': return Key{KeyType::F1, 0};
                case 'Q': return Key{KeyType::F2, 0};
                case 'R': return Key{KeyType::F3, 0};
                case 'S': return Key{KeyType::F4, 0};
                default: return Key{KeyType::Unknown, 0};
            }
        }
        return Key{KeyType::Unknown, 0};
    }

    if (c >= 32 && c < 127) return Key{KeyType::Char, static_cast<char32_t>(c)};

    return Key{KeyType::Unknown, 0};
}

// --- Screen buffer ---

struct Cell {
    char ch = ' ';
    bool bold = false;
    bool reverse = false;
};

class ScreenBuffer {
    int rows_ = 0;
    int cols_ = 0;
    std::vector<Cell> cells_;
    std::vector<Cell> prev_;
    bool first_frame_ = true;

    auto idx(int r, int c) const -> int { return r * cols_ + c; }
public:
    ScreenBuffer() = default;
    ScreenBuffer(int rows, int cols) : rows_(rows), cols_(cols),
        cells_(static_cast<size_t>(rows * cols)),
        prev_(static_cast<size_t>(rows * cols)) {}

    auto resize(int rows, int cols) -> void {
        rows_ = rows;
        cols_ = cols;
        cells_.assign(static_cast<size_t>(rows * cols), Cell{});
        prev_.assign(static_cast<size_t>(rows * cols), Cell{});
        first_frame_ = true;
    }

    auto rows() const -> int { return rows_; }
    auto cols() const -> int { return cols_; }

    auto set(int row, int col, char ch, bool bold = false, bool reverse = false) -> void {
        if (row < 0 || row >= rows_ || col < 0 || col >= cols_) return;
        auto& c = cells_[idx(row, col)];
        c.ch = ch;
        c.bold = bold;
        c.reverse = reverse;
    }

    auto set_string(int row, int col, std::string_view s, bool bold = false, bool reverse = false) -> void {
        for (int i = 0; i < static_cast<int>(s.size()); ++i) {
            set(row, col + i, s[static_cast<size_t>(i)], bold, reverse);
        }
    }

    auto clear() -> void {
        cells_.assign(cells_.size(), Cell{});
    }

    auto render() -> void {
        if (first_frame_) {
            terminal::clear_screen();
            for (int r = 0; r < rows_; ++r) {
                terminal::move_cursor(r + 1, 1);
                for (int c = 0; c < cols_; ++c) {
                    auto& cell = cells_[idx(r, c)];
                    if (cell.bold) terminal::bold(true);
                    if (cell.reverse) terminal::invert_video(true);
                    std::putchar(cell.ch);
                    if (cell.bold || cell.reverse) terminal::reset_attr();
                }
            }
            prev_ = cells_;
            first_frame_ = false;
            std::fflush(stdout);
            return;
        }

        for (int r = 0; r < rows_; ++r) {
            bool row_dirty = false;
            for (int c = 0; c < cols_; ++c) {
                auto& cur = cells_[idx(r, c)];
                auto& prv = prev_[idx(r, c)];
                bool changed = cur.ch != prv.ch || cur.bold != prv.bold || cur.reverse != prv.reverse;
                if (changed || row_dirty) {
                    if (changed && !row_dirty) {
                        terminal::move_cursor(r + 1, c + 1);
                        row_dirty = true;
                    }
                    if (cur.bold) terminal::bold(true);
                    if (cur.reverse) terminal::invert_video(true);
                    std::putchar(cur.ch);
                    if (cur.bold || cur.reverse) terminal::reset_attr();
                }
            }
        }
        prev_ = cells_;
        std::fflush(stdout);
    }
};

// --- Global resize flag ---

inline auto global_resized_flag() -> std::atomic<bool>& {
    static std::atomic<bool> flag{false};
    return flag;
}

inline auto request_resize() -> void {
    global_resized_flag().store(true);
}

// --- TUI Application base ---

class TuiApp {
    ScreenBuffer screen_;
    int tick_ms_ = 1000;

public:
    explicit TuiApp(int tick_ms = 1000) : tick_ms_(tick_ms) {}
    virtual ~TuiApp() = default;

    virtual auto on_key(Key k) -> bool = 0;
    virtual auto on_tick() -> void = 0;
    virtual auto on_resize(int rows, int cols) -> void = 0;

    auto screen() -> ScreenBuffer& { return screen_; }

    auto run() -> int {
        auto [rows, cols] = terminal::get_size();
        screen_.resize(rows, cols);

        terminal::RawMode raw_mode;
        terminal::enter_alt_screen();
        terminal::hide_cursor();

        on_resize(rows, cols);
        on_tick();
        screen_.render();

        while (true) {
            auto key = read_key(0, tick_ms_);
            if (key) {
                if (!on_key(*key)) break;
                screen_.render();
            } else {
                on_tick();
                if (global_resized_flag().exchange(false)) {
                    auto [nr, nc] = terminal::get_size();
                    screen_.resize(nr, nc);
                    on_resize(nr, nc);
                }
                screen_.render();
            }
        }

        terminal::show_cursor();
        terminal::leave_alt_screen();
        return 0;
    }
};

} // namespace cfbox::tui
