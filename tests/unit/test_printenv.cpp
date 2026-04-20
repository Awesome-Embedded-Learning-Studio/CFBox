#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_PRINTENV
using namespace cfbox::test;

TEST(PrintenvTest, PrintsHome) {
    char a0[] = "printenv", a1[] = "HOME";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return printenv_main(2, argv); });
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find('/'), std::string::npos);
}

TEST(PrintenvTest, MissingVarReturnsOne) {
    char a0[] = "printenv", a1[] = "NONEXISTENT_VAR_XYZ_12345";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(printenv_main(2, argv), 1);
}
#endif
