#include "init.hpp"

#include <cstdio>
#include <cstring>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <cfbox/applets.hpp>

namespace cfbox::init {

auto run_smoke_tests(InitState& /*state*/) -> void {
    struct TestResult { int pass = 0; int fail = 0; };
    TestResult result;

    std::printf("=== CFBox Init / System-Level Boot Test ===\n");

    struct utsname info {};
    if (uname(&info) == 0) {
        std::printf("Kernel: %s %s %s\n", info.sysname, info.release, info.machine);
    }
    std::printf("PID:    %d\n\n", getpid());

    std::printf("--- Applet Smoke Tests ---\n");

#if CFBOX_ENABLE_ECHO
    { char n[] = "echo", a[] = "hello"; char* av[] = {n, a, nullptr};
      if (echo_main(2, av) == 0) { std::printf("  PASS: echo\n"); result.pass++; }
      else { std::printf("  FAIL: echo\n"); result.fail++; } }
#endif

#if CFBOX_ENABLE_CAT
    { char n[] = "cat", a[] = "/proc/version"; char* av[] = {n, a, nullptr};
      if (cat_main(2, av) == 0) { std::printf("  PASS: cat\n"); result.pass++; }
      else { std::printf("  FAIL: cat\n"); result.fail++; } }
#endif

#if CFBOX_ENABLE_LS
    { char n[] = "ls", a[] = "/"; char* av[] = {n, a, nullptr};
      if (ls_main(2, av) == 0) { std::printf("  PASS: ls\n"); result.pass++; }
      else { std::printf("  FAIL: ls\n"); result.fail++; } }
#endif

#if CFBOX_ENABLE_WC
    { char n[] = "wc", a1[] = "-l", a2[] = "/proc/cpuinfo"; char* av[] = {n, a1, a2, nullptr};
      if (wc_main(3, av) == 0) { std::printf("  PASS: wc\n"); result.pass++; }
      else { std::printf("  FAIL: wc\n"); result.fail++; } }
#endif

#if CFBOX_ENABLE_FREE
    { char n[] = "free"; char* av[] = {n, nullptr};
      if (free_main(1, av) == 0) { std::printf("  PASS: free\n"); result.pass++; }
      else { std::printf("  FAIL: free\n"); result.fail++; } }
#endif

#if CFBOX_ENABLE_UPTIME
    { char n[] = "uptime"; char* av[] = {n, nullptr};
      if (uptime_main(1, av) == 0) { std::printf("  PASS: uptime\n"); result.pass++; }
      else { std::printf("  FAIL: uptime\n"); result.fail++; } }
#endif

#if CFBOX_ENABLE_PS
    { char n[] = "ps"; char* av[] = {n, nullptr};
      if (ps_main(1, av) == 0) { std::printf("  PASS: ps\n"); result.pass++; }
      else { std::printf("  FAIL: ps\n"); result.fail++; } }
#endif

    std::printf("\n=== RESULTS: %d passed, %d failed ===\n", result.pass, result.fail);
    std::printf("=== ALL TESTS COMPLETE ===\n");
    std::fflush(nullptr);
}

} // namespace cfbox::init

auto init_main(int argc, char* argv[]) -> int {
    // Handle --help/--version
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--help")    { cfbox::help::print_help(cfbox::init::HELP); return 0; }
        if (arg == "--version") { cfbox::help::print_version(cfbox::init::HELP); return 0; }
    }

    cfbox::init::InitState state;
    state.is_pid1 = (getpid() == 1);

    // Check if /etc/inittab exists
    FILE* inittab = std::fopen("/etc/inittab", "r");
    if (!inittab) {
        // Fallback: QEMU smoke test mode (preserves CI compatibility)
        if (state.is_pid1) {
            mount("proc",     "/proc", "proc",     0, nullptr);
            mount("sysfs",    "/sys",  "sysfs",    0, nullptr);
            mount("devtmpfs", "/dev",  "devtmpfs", 0, nullptr);
        }
        cfbox::init::run_smoke_tests(state);
        if (state.is_pid1) {
            std::fflush(nullptr);
            sync();
            reboot(RB_POWER_OFF);
        }
        return 0;
    }
    std::fclose(inittab);

    // Full init mode
    state.entries = cfbox::init::parse_inittab("/etc/inittab");

    cfbox::init::install_signal_handlers(state);

    // Phase 1: sysinit (mount filesystems, run sysinit commands)
    cfbox::init::run_sysinit(state);

    // Phase 2: boot entries
    cfbox::init::run_boot_entries(state);

    // Phase 3: multi-user entries (respawn, once, wait)
    cfbox::init::run_level_entries(state, "2345");

    // Main loop: wait for signals, reap children, handle respawn
    while (!state.shutting_down) {
        // Handle SIGCHLD: reap children
        if (state.sigchld_received) {
            state.sigchld_received = false;
            cfbox::init::reap_children(state);
            cfbox::init::check_respawn(state);
        }

        // Handle SIGINT (ctrlaltdel)
        if (state.sigint_received) {
            state.sigint_received = false;
            // Run ctrlaltdel entries
            for (const auto& e : state.entries) {
                if (e.action == "ctrlaltdel") {
                    cfbox::init::InittabEntry fake;
                    fake.process = e.process;
                    auto pid = cfbox::init::spawn_process(state, fake, false);
                    if (pid > 0) {
                        int status = 0;
                        waitpid(pid, &status, 0);
                    }
                }
            }
        }

        // Sleep briefly, then check again
        usleep(100000); // 100ms
    }

    // Shutdown
    cfbox::init::do_shutdown(state, false);
    return 0;
}
