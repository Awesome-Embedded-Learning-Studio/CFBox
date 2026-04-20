#pragma once

#include <cstdlib>
#include <string>
#include <string_view>
#include <unistd.h>

namespace cfbox::term {

namespace detail {
struct ColorState {
    bool auto_detected = false;
    bool auto_value = false;
    bool override_set = false;
    bool override_value = false;
};

inline auto color_state() -> ColorState& {
    static ColorState state;
    return state;
}
} // namespace detail

inline auto color_enabled() -> bool {
    auto& s = detail::color_state();
    if (s.override_set) return s.override_value;
    if (!s.auto_detected) {
        s.auto_detected = true;
        s.auto_value = (std::getenv("NO_COLOR") == nullptr) && (isatty(STDOUT_FILENO) != 0);
    }
    return s.auto_value;
}

inline void set_color_enabled(bool enabled) {
    auto& s = detail::color_state();
    s.override_set = true;
    s.override_value = enabled;
}

inline void reset_color_enabled() {
    auto& s = detail::color_state();
    s.override_set = false;
    s.auto_detected = false;
}

namespace detail {
inline auto sv(const char* code) -> std::string_view {
    return color_enabled() ? std::string_view{code} : std::string_view{};
}
} // namespace detail

// Foreground colors
inline auto red() -> std::string_view { return detail::sv("\033[31m"); }
inline auto green() -> std::string_view { return detail::sv("\033[32m"); }
inline auto yellow() -> std::string_view { return detail::sv("\033[33m"); }
inline auto blue() -> std::string_view { return detail::sv("\033[34m"); }
inline auto magenta() -> std::string_view { return detail::sv("\033[35m"); }
inline auto cyan() -> std::string_view { return detail::sv("\033[36m"); }

// Attributes
inline auto bold() -> std::string_view { return detail::sv("\033[1m"); }
inline auto dim() -> std::string_view { return detail::sv("\033[2m"); }
inline auto underline() -> std::string_view { return detail::sv("\033[4m"); }

// Reset
inline auto reset() -> std::string_view { return detail::sv("\033[0m"); }

// Utility: wrap text with a color and reset
inline auto colored(std::string_view text, std::string_view color_code) -> std::string {
    if (!color_enabled()) return std::string{text};
    return std::string{color_code} + std::string{text} + std::string{reset()};
}

} // namespace cfbox::term
