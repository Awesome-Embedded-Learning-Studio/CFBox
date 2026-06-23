#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

#include <cstring>
#include <sys/reboot.h>
#include <unistd.h>

namespace {

constexpr cfbox::help::HelpEntry HELP_REBOOT = {
    .name = "reboot",
    .version = CFBOX_VERSION_STRING,
    .one_line = "reboot the system",
    .usage = "reboot",
    .options = "",
    .extra = "",
};

constexpr cfbox::help::HelpEntry HELP_POWEROFF = {
    .name = "poweroff",
    .version = CFBOX_VERSION_STRING,
    .one_line = "power off the system",
    .usage = "poweroff",
    .options = "",
    .extra = "",
};

} // namespace

auto reboot_main(int argc, char* argv[]) -> int {
    // Honor --help/--version before touching the reboot syscall.
    for (int i = 1; i < argc; ++i) {
        std::string_view a{argv[i]};
        if (a == "--help") {
            cfbox::help::print_help(HELP_REBOOT);
            return 0;
        }
        if (a == "--version") {
            cfbox::help::print_version(HELP_REBOOT);
            return 0;
        }
    }
    sync(); // never reboot with dirty filesystems (gotcha #6)
    if (reboot(RB_AUTOBOOT) != 0) {
        CFBOX_ERR("reboot", "%s", std::strerror(errno));
        return 1;
    }
    return 0; // unreachable on a successful reboot
}

auto poweroff_main(int argc, char* argv[]) -> int {
    for (int i = 1; i < argc; ++i) {
        std::string_view a{argv[i]};
        if (a == "--help") {
            cfbox::help::print_help(HELP_POWEROFF);
            return 0;
        }
        if (a == "--version") {
            cfbox::help::print_version(HELP_POWEROFF);
            return 0;
        }
    }
    sync();
    if (reboot(RB_POWER_OFF) != 0) {
        CFBOX_ERR("poweroff", "%s", std::strerror(errno));
        return 1;
    }
    return 0; // unreachable on a successful poweroff
}
