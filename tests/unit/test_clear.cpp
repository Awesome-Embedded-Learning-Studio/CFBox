#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_CLEAR

TEST(ClearTest, ReturnsZero) {
    char a0[] = "clear";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(clear_main(1, argv), 0);
}

TEST(ClearTest, OutputsEscapeSequence) {
    char a0[] = "clear";
    char* argv[] = {a0, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return clear_main(1, argv); });
    EXPECT_NE(out.find("\033[2J\033[H"), std::string::npos);
}

TEST(ClearTest, Help) {
    char a0[] = "clear", a1[] = "--help";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return clear_main(2, argv); });
    EXPECT_NE(out.find("clear"), std::string::npos);
}

TEST(ClearTest, Version) {
    char a0[] = "clear", a1[] = "--version";
    char* argv[] = {a0, a1, nullptr};
    auto out = cfbox::test::capture_stdout([&] { return clear_main(2, argv); });
    EXPECT_NE(out.find("cfbox"), std::string::npos);
}

#endif // CFBOX_ENABLE_CLEAR
