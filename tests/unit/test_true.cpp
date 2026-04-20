#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_TRUE

TEST(TrueTest, ReturnsZero) {
    char a0[] = "true";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(true_main(1, argv), 0);
}

TEST(TrueTest, IgnoresArgs) {
    char a0[] = "true", a1[] = "anything", a2[] = "else";
    char* argv[] = {a0, a1, a2, nullptr};
    EXPECT_EQ(true_main(3, argv), 0);
}

TEST(TrueTest, Help) {
    char a0[] = "true", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return true_main(2, argv); });
    EXPECT_NE(out.find("true"), std::string::npos);
}

TEST(TrueTest, Version) {
    char a0[] = "true", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return true_main(2, argv); });
    EXPECT_NE(out.find("cfbox"), std::string::npos);
}

#endif // CFBOX_ENABLE_TRUE
