#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_MOUNT

TEST(MountTest, HelpPrintsUsage) {
    char a0[] = "mount", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return mount_main(2, argv); });
    EXPECT_NE(out.find("mount"), std::string::npos);
}

TEST(MountTest, VersionExitsZero) {
    char a0[] = "mount", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(mount_main(2, argv), 0);
}

// No SOURCE TARGET pair => usage error, not a syscall attempt.
TEST(MountTest, NoArgsIsUsageError) {
    char a0[] = "mount";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(mount_main(1, argv), 2);
}

// Only one positional (missing TARGET) => usage error.
TEST(MountTest, SinglePositionalIsUsageError) {
    char a0[] = "mount", a1[] = "-t", a2[] = "proc", a3[] = "/proc";
    char* argv[] = {a0, a1, a2, a3, nullptr};
    EXPECT_EQ(mount_main(4, argv), 2);
}

#endif // CFBOX_ENABLE_MOUNT
