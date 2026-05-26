#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_MOUNTPOINT

TEST(MountpointTest, RootIsMountpoint) {
    char a0[] = "mountpoint", a1[] = "/";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(mountpoint_main(2, argv), 0);
}

TEST(MountpointTest, NonexistentPath) {
    char a0[] = "mountpoint", a1[] = "/nonexistent_xyz_12345";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_NE(mountpoint_main(2, argv), 0);
}

TEST(MountpointTest, QuietMode) {
    char a0[] = "mountpoint", a1[] = "-q", a2[] = "/";
    char* argv[] = {a0, a1, a2, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return mountpoint_main(3, argv); });
    EXPECT_EQ(mountpoint_main(3, argv), 0);
    EXPECT_TRUE(out.empty());
}

TEST(MountpointTest, Help) {
    char a0[] = "mountpoint", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return mountpoint_main(2, argv); });
    EXPECT_NE(out.find("mountpoint"), std::string::npos);
}

#endif // CFBOX_ENABLE_MOUNTPOINT
