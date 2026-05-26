#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_ECHO

using namespace cfbox::test;

TEST(EchoTest, BasicOutput) {
    char a0[] = "echo", a1[] = "hello", a2[] = "world";
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return echo_main(3, argv); });
    EXPECT_EQ(out, "hello world\n");
}

TEST(EchoTest, NoTrailingNewline) {
    char a0[] = "echo", a1[] = "-n", a2[] = "no newline";
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return echo_main(3, argv); });
    EXPECT_EQ(out, "no newline");
}

TEST(EchoTest, NoArgs) {
    char a0[] = "echo";
    char* argv[] = {a0};
    auto out = capture_stdout([&]{ return echo_main(1, argv); });
    EXPECT_EQ(out, "\n");
}

TEST(EchoTest, EscapeSequences) {
    char a0[] = "echo", a1[] = "-e", a2[] = "a\\tb";
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return echo_main(3, argv); });
    EXPECT_EQ(out, "a\tb\n");
}

TEST(EchoTest, EscapeNewline) {
    char a0[] = "echo", a1[] = "-e", a2[] = "line1\\nline2";
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return echo_main(3, argv); });
    EXPECT_EQ(out, "line1\nline2\n");
}

#endif // CFBOX_ENABLE_ECHO
