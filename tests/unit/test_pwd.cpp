#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_PWD

TEST(PwdTest, PrintsCurrentDir) {
    char a0[] = "pwd";
    char* argv[] = {a0, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return pwd_main(1, argv); });
    EXPECT_FALSE(out.empty());
    // Should match std::filesystem::current_path()
    auto expected = std::filesystem::current_path().string();
    EXPECT_EQ(out, expected + "\n");
}

TEST(PwdTest, ReturnsZero) {
    char a0[] = "pwd";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(pwd_main(1, argv), 0);
}

TEST(PwdTest, Help) {
    char a0[] = "pwd", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return pwd_main(2, argv); });
    EXPECT_NE(out.find("pwd"), std::string::npos);
}

#endif // CFBOX_ENABLE_PWD
