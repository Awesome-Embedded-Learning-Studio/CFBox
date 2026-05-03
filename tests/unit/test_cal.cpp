#include <gtest/gtest.h>
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_CAL
#include <cfbox/applets.hpp>

TEST(CalTest, HelpAndVersion) {
    char a0[] = "cal";
    char a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};

    testing::internal::CaptureStdout();
    int rc = cal_main(2, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("cal"), std::string::npos);
}

TEST(CalTest, CurrentMonth) {
    char a0[] = "cal";
    char* argv[] = {a0, nullptr};

    testing::internal::CaptureStdout();
    int rc = cal_main(1, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("Su Mo Tu We Th Fr Sa"), std::string::npos);
}

TEST(CalTest, SpecificMonth) {
    char a0[] = "cal";
    char a1[] = "1";
    char a2[] = "2025";
    char* argv[] = {a0, a1, a2, nullptr};

    testing::internal::CaptureStdout();
    int rc = cal_main(3, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("January 2025"), std::string::npos);
}

TEST(CalTest, WholeYear) {
    char a0[] = "cal";
    char a1[] = "-y";
    char a2[] = "2025";
    char* argv[] = {a0, a1, a2, nullptr};

    testing::internal::CaptureStdout();
    int rc = cal_main(3, argv);
    std::string out = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(out.find("January 2025"), std::string::npos);
    EXPECT_NE(out.find("December 2025"), std::string::npos);
}
#endif
