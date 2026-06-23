#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_UMOUNT

TEST(UmountTest, HelpPrintsUsage) {
    char a0[] = "umount", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return umount_main(2, argv); });
    EXPECT_NE(out.find("umount"), std::string::npos);
}

TEST(UmountTest, VersionExitsZero) {
    char a0[] = "umount", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(umount_main(2, argv), 0);
}

// No target and no -a => usage error, not a syscall attempt.
TEST(UmountTest, NoTargetIsUsageError) {
    char a0[] = "umount";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(umount_main(1, argv), 2);
}

#endif // CFBOX_ENABLE_UMOUNT
