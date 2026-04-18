#pragma once

#include <array>
#include <string_view>

namespace cfbox::applet {
struct AppEntry {
    using AppExecFunc = int (*)(int argc, char* argv[]);
    std::string_view app_name;
    AppExecFunc app_func;
    std::string_view help;
};

// constexpr auto APPLET_REGISTRY = std::to_array<AppEntry>({
//     // {"echo", echo_main, "display a line of text"},
//     // {"cat", cat_main, "concatenate files"},
// });

template <std::size_t N>
inline auto find_applet(std::string_view name, const std::array<AppEntry, N>& registry)
    -> const AppEntry* {
    for (const auto& entry : registry) {
        if (entry.app_name == name)
            return &entry;
    }
    return nullptr;
}

} // namespace cfbox::applet
