#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_EXPR

using namespace cfbox::test;

TEST(ExprTest, Addition) {
    char a0[] = "expr", a1[] = "2", a2[] = "+", a3[] = "3";
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return expr_main(4, argv); });
    EXPECT_EQ(out, "5\n");
}

TEST(ExprTest, Division) {
    char a0[] = "expr", a1[] = "10", a2[] = "/", a3[] = "3";
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return expr_main(4, argv); });
    EXPECT_EQ(out, "3\n");
}

TEST(ExprTest, Multiplication) {
    char a0[] = "expr", a1[] = "2", a2[] = "*", a3[] = "3";
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return expr_main(4, argv); });
    EXPECT_EQ(out, "6\n");
}

TEST(ExprTest, Length) {
    char a0[] = "expr", a1[] = "length", a2[] = "hello";
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return expr_main(3, argv); });
    EXPECT_EQ(out, "5\n");
}

#endif // CFBOX_ENABLE_EXPR
