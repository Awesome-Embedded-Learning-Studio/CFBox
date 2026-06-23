#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_SWAPOFF

TEST(SwapoffTest, HelpPrintsUsage) {
    char a0[] = "swapoff", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return swapoff_main(2, argv); });
    EXPECT_NE(out.find("swapoff"), std::string::npos);
}

TEST(SwapoffTest, VersionExitsZero) {
    char a0[] = "swapoff", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(swapoff_main(2, argv), 0);
}

// No device and no -a => usage error, not a syscall attempt.
TEST(SwapoffTest, NoDeviceIsUsageError) {
    char a0[] = "swapoff";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(swapoff_main(1, argv), 2);
}

#endif // CFBOX_ENABLE_SWAPOFF
