#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_MDEV

TEST(MdevTest, HelpPrintsUsage) {
    char a0[] = "mdev", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return mdev_main(2, argv); });
    EXPECT_NE(out.find("mdev"), std::string::npos);
}

TEST(MdevTest, VersionExitsZero) {
    char a0[] = "mdev", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(mdev_main(2, argv), 0);
}

// Without -s, mdev is in hotplug mode which cfbox does not implement;
// it must exit cleanly (imx-forge only ever runs `mdev -s`).
TEST(MdevTest, NoScanExitsCleanly) {
    char a0[] = "mdev";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(mdev_main(1, argv), 0);
}

#endif // CFBOX_ENABLE_MDEV
