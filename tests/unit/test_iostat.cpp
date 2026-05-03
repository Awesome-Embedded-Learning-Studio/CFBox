#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_IOSTAT
#include <cfbox/applets.hpp>

TEST(IostatTest, HelpAndVersion) {
    char a0[] = "iostat";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = iostat_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("iostat"), std::string::npos);
}

TEST(IostatTest, SingleRun) {
    char a0[] = "iostat";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    int rc = iostat_main(1, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("Device"), std::string::npos);
}
#endif
