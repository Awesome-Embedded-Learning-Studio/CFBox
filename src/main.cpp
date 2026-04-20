#include <cstdio>
#include <string>
#include <string_view>

#include <cfbox/applets.hpp>
#include <cfbox/help.hpp>

namespace {

auto basename_of(std::string_view path) -> std::string_view {
    auto pos = path.rfind('/');
    return (pos == std::string_view::npos) ? path : path.substr(pos + 1);
}

auto print_list() -> void {
    for (const auto& entry : APPLET_REGISTRY) {
        std::printf("  %-10s %s\n", entry.app_name.data(), entry.help.data());
    }
}

auto print_help(const char* prog) -> void {
    std::printf("Usage: %s <applet> [args...]\n", prog);
    std::printf("       %s --list\n", prog);
    std::printf("       %s --help\n\n", prog);
    std::printf("Available applets:\n");
    print_list();
}

} // namespace

auto main(int argc, char* argv[]) -> int {
    // argv[0] symlink detection: /usr/bin/echo → call echo applet
    std::string_view prog_name = basename_of(argv[0]);
    if (const auto* entry = cfbox::applet::find_applet(prog_name, APPLET_REGISTRY)) {
        return entry->app_func(argc, argv);
    }

    if (argc < 2) {
        std::fprintf(stderr, "cfbox: no applet specified. Use '%s --help'.\n", argv[0]);
        return 1;
    }

    std::string_view cmd = argv[1];

    if (cmd == "--list") {
        print_list();
        return 0;
    }
    if (cmd == "--version") {
        cfbox::help::print_cfbox_version();
        return 0;
    }
    if (cmd == "--help") {
        print_help(argv[0]);
        return 0;
    }

    // subcommand mode: cfbox echo hello
    if (const auto* entry = cfbox::applet::find_applet(cmd, APPLET_REGISTRY)) {
        return entry->app_func(argc - 1, argv + 1);
    }

    std::fprintf(stderr, "cfbox: unknown applet '%s'\n", std::string{cmd}.c_str());
    return 1;
}
