#include <gtest/gtest.h>
#include <cfbox/applets.hpp>

#if CFBOX_ENABLE_PS
TEST(PsTest, RunsAndOutputs) {
    char a0[] = "ps";
    char* argv[] = {a0, nullptr};
    testing::internal::CaptureStdout();
    int rc = ps_main(1, argv);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(output.find("PID"), std::string::npos);
    EXPECT_NE(output.find("CMD"), std::string::npos);
}

TEST(PsTest, AuxFormat) {
    char a0[] = "ps";
    char a1[] = "aux";
    char* argv[] = {a0, a1, nullptr};
    testing::internal::CaptureStdout();
    int rc = ps_main(2, argv);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(output.find("USER"), std::string::npos);
    EXPECT_NE(output.find("%CPU"), std::string::npos);
    EXPECT_NE(output.find("%MEM"), std::string::npos);
}
#endif
