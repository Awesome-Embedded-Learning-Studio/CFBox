#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_UNAME

using namespace cfbox::test;

TEST(UnameTest, DefaultIsKernelName) {
    char a0[] = "uname";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return uname_main(1, argv); });
    EXPECT_NE(out.find("Linux"), std::string::npos);
}

TEST(UnameTest, All) {
    char a0[] = "uname", a1[] = "-a";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return uname_main(2, argv); });
    EXPECT_NE(out.find("Linux"), std::string::npos);
    // -a should contain multiple fields
    EXPECT_GT(out.find(' '), 0u);
}

TEST(UnameTest, MachineFlag) {
    char a0[] = "uname", a1[] = "-m";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return uname_main(2, argv); });
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find('\n'), std::string::npos);
}

#endif // CFBOX_ENABLE_UNAME
