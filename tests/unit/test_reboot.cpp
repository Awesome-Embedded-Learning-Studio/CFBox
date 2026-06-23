#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

// NOTE: reboot/poweroff must only be exercised through --help/--version here;
// a bare call invokes the reboot(2) syscall and would actually reboot the
// machine. Real reboot/poweroff behavior is verified end-to-end on a board.

#if CFBOX_ENABLE_REBOOT

TEST(RebootTest, HelpPrintsUsage) {
    char a0[] = "reboot", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return reboot_main(2, argv); });
    EXPECT_NE(out.find("reboot"), std::string::npos);
}

TEST(RebootTest, VersionExitsZero) {
    char a0[] = "reboot", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(reboot_main(2, argv), 0);
}

TEST(PoweroffTest, HelpPrintsUsage) {
    char a0[] = "poweroff", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return poweroff_main(2, argv); });
    EXPECT_NE(out.find("poweroff"), std::string::npos);
}

TEST(PoweroffTest, VersionExitsZero) {
    char a0[] = "poweroff", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(poweroff_main(2, argv), 0);
}

#endif // CFBOX_ENABLE_REBOOT
