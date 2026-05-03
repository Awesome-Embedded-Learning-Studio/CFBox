#include <gtest/gtest.h>
#include <cfbox/applets.hpp>

#if CFBOX_ENABLE_FREE
TEST(FreeTest, RunsAndOutputs) {
    char a0[] = "free";
    char* argv[] = {a0, nullptr};
    testing::internal::CaptureStdout();
    int rc = free_main(1, argv);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(output.find("total"), std::string::npos);
    EXPECT_NE(output.find("Mem:"), std::string::npos);
}

TEST(FreeTest, HumanFlag) {
    char a0[] = "free";
    char a1[] = "-h";
    char* argv[] = {a0, a1, nullptr};
    testing::internal::CaptureStdout();
    int rc = free_main(2, argv);
    std::string output = testing::internal::GetCapturedStdout();
    EXPECT_EQ(rc, 0);
    EXPECT_NE(output.find("total"), std::string::npos);
}
#endif
