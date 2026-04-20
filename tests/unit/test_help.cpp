#include <cfbox/help.hpp>
#include <cfbox/term.hpp>
#include <gtest/gtest.h>

#include <cstdio>
#include <string>

#include "test_capture.hpp"

TEST(HelpTest, PrintHelpContainsName) {
    constexpr cfbox::help::HelpEntry entry = {
        .name = "testapp",
        .version = "1.0",
        .one_line = "a test applet",
        .usage = "testapp [OPTIONS]",
        .options = "  -v     verbose",
        .extra = "",
    };
    auto out = cfbox::test::capture_stdout([&]()->int {
        cfbox::help::print_help(entry);
        return 0;
    });
    EXPECT_NE(out.find("testapp"), std::string::npos);
    EXPECT_NE(out.find("a test applet"), std::string::npos);
    EXPECT_NE(out.find("testapp [OPTIONS]"), std::string::npos);
    EXPECT_NE(out.find("-v"), std::string::npos);
    EXPECT_NE(out.find("--help"), std::string::npos);
    EXPECT_NE(out.find("--version"), std::string::npos);
}

TEST(HelpTest, PrintHelpWithExtra) {
    constexpr cfbox::help::HelpEntry entry = {
        .name = "foo",
        .version = "0.1",
        .one_line = "does foo",
        .usage = "foo",
        .options = "",
        .extra = "Note: foo is experimental",
    };
    auto out = cfbox::test::capture_stdout([&]()->int {
        cfbox::help::print_help(entry);
        return 0;
    });
    EXPECT_NE(out.find("experimental"), std::string::npos);
}

TEST(HelpTest, PrintVersion) {
    constexpr cfbox::help::HelpEntry entry = {
        .name = "echo",
        .version = "0.0.1",
        .one_line = "",
        .usage = "",
        .options = "",
        .extra = "",
    };
    auto out = cfbox::test::capture_stdout([&]()->int {
        cfbox::help::print_version(entry);
        return 0;
    });
    EXPECT_NE(out.find("cfbox echo 0.0.1"), std::string::npos);
}

TEST(HelpTest, PrintCfboxVersion) {
    auto out = cfbox::test::capture_stdout([&]()->int {
        cfbox::help::print_cfbox_version();
        return 0;
    });
    EXPECT_NE(out.find("cfbox"), std::string::npos);
}

TEST(HelpTest, ColorDisabledOutput) {
    cfbox::term::set_color_enabled(false);
    constexpr cfbox::help::HelpEntry entry = {
        .name = "test",
        .version = "1.0",
        .one_line = "test",
        .usage = "test",
        .options = "",
        .extra = "",
    };
    auto out = cfbox::test::capture_stdout([&]()->int {
        cfbox::help::print_help(entry);
        return 0;
    });
    EXPECT_EQ(out.find("\033["), std::string::npos);
    cfbox::term::reset_color_enabled();
}

TEST(HelpTest, ColorEnabledOutput) {
    cfbox::term::set_color_enabled(true);
    constexpr cfbox::help::HelpEntry entry = {
        .name = "test",
        .version = "1.0",
        .one_line = "test",
        .usage = "test",
        .options = "",
        .extra = "",
    };
    auto out = cfbox::test::capture_stdout([&]()->int {
        cfbox::help::print_help(entry);
        return 0;
    });
    EXPECT_NE(out.find("\033["), std::string::npos);
    cfbox::term::reset_color_enabled();
}
