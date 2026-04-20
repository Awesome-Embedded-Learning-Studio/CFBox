#include <gtest/gtest.h>

#include <cfbox/term.hpp>

TEST(TermTest, ColorDisabledReturnsEmpty) {
    cfbox::term::set_color_enabled(false);
    EXPECT_TRUE(cfbox::term::red().empty());
    EXPECT_TRUE(cfbox::term::green().empty());
    EXPECT_TRUE(cfbox::term::bold().empty());
    EXPECT_TRUE(cfbox::term::reset().empty());
    cfbox::term::reset_color_enabled();
}

TEST(TermTest, ColorEnabledReturnsCodes) {
    cfbox::term::set_color_enabled(true);
    EXPECT_EQ(cfbox::term::red(), "\033[31m");
    EXPECT_EQ(cfbox::term::green(), "\033[32m");
    EXPECT_EQ(cfbox::term::yellow(), "\033[33m");
    EXPECT_EQ(cfbox::term::blue(), "\033[34m");
    EXPECT_EQ(cfbox::term::magenta(), "\033[35m");
    EXPECT_EQ(cfbox::term::cyan(), "\033[36m");
    EXPECT_EQ(cfbox::term::bold(), "\033[1m");
    EXPECT_EQ(cfbox::term::dim(), "\033[2m");
    EXPECT_EQ(cfbox::term::underline(), "\033[4m");
    EXPECT_EQ(cfbox::term::reset(), "\033[0m");
    cfbox::term::reset_color_enabled();
}

TEST(TermTest, ColoredWhenDisabled) {
    cfbox::term::set_color_enabled(false);
    EXPECT_EQ(cfbox::term::colored("hello", cfbox::term::red()), "hello");
    cfbox::term::reset_color_enabled();
}

TEST(TermTest, ColoredWhenEnabled) {
    cfbox::term::set_color_enabled(true);
    auto result = cfbox::term::colored("hello", "\033[31m");
    EXPECT_EQ(result, "\033[31mhello\033[0m");
    cfbox::term::reset_color_enabled();
}

TEST(TermTest, SetOverrideWorks) {
    cfbox::term::set_color_enabled(true);
    EXPECT_TRUE(cfbox::term::color_enabled());
    cfbox::term::set_color_enabled(false);
    EXPECT_FALSE(cfbox::term::color_enabled());
    cfbox::term::reset_color_enabled();
}
