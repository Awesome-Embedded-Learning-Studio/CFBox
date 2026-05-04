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

inline auto color_state() noexcept -> ColorState& {
    static ColorState state;
    return state;
}
} // namespace detail

[[nodiscard]] inline auto color_enabled() noexcept -> bool {
    auto& s = detail::color_state();
    if (s.override_set) return s.override_value;
    if (!s.auto_detected) {
        s.auto_detected = true;
        s.auto_value = (std::getenv("NO_COLOR") == nullptr) && (isatty(STDOUT_FILENO) != 0);
    }
    return s.auto_value;
}

inline void set_color_enabled(bool enabled) noexcept {
    auto& s = detail::color_state();
    s.override_set = true;
    s.override_value = enabled;
}

inline void reset_color_enabled() noexcept {
    auto& s = detail::color_state();
    s.override_set = false;
    s.auto_detected = false;
}

namespace detail {
[[nodiscard]] inline auto sv(const char* code) noexcept -> std::string_view {
    return color_enabled() ? std::string_view{code} : std::string_view{};
}
} // namespace detail

// Foreground colors
[[nodiscard]] inline auto red() noexcept -> std::string_view { return detail::sv("\033[31m"); }
[[nodiscard]] inline auto green() noexcept -> std::string_view { return detail::sv("\033[32m"); }
[[nodiscard]] inline auto yellow() noexcept -> std::string_view { return detail::sv("\033[33m"); }
[[nodiscard]] inline auto blue() noexcept -> std::string_view { return detail::sv("\033[34m"); }
[[nodiscard]] inline auto magenta() noexcept -> std::string_view { return detail::sv("\033[35m"); }
[[nodiscard]] inline auto cyan() noexcept -> std::string_view { return detail::sv("\033[36m"); }

// Attributes
[[nodiscard]] inline auto bold() noexcept -> std::string_view { return detail::sv("\033[1m"); }
[[nodiscard]] inline auto dim() noexcept -> std::string_view { return detail::sv("\033[2m"); }
[[nodiscard]] inline auto underline() noexcept -> std::string_view { return detail::sv("\033[4m"); }

// Reset
[[nodiscard]] inline auto reset() noexcept -> std::string_view { return detail::sv("\033[0m"); }

// Utility: wrap text with a color and reset
inline auto colored(std::string_view text, std::string_view color_code) -> std::string {
    if (!color_enabled()) return std::string{text};
    return std::string{color_code} + std::string{text} + std::string{reset()};
}

} // namespace cfbox::term
