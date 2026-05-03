#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_WATCH
#include <cfbox/applets.hpp>

TEST(WatchTest, HelpAndVersion) {
    char a0[] = "watch";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = watch_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("watch"), std::string::npos);
}

TEST(WatchTest, NoCommand) {
    char a0[] = "watch";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    testing::internal::CaptureStderr();
    int rc = watch_main(1, argv);
    testing::internal::GetCapturedStdout();
    testing::internal::GetCapturedStderr();
    EXPECT_EQ(rc, 1);
}
#endif
