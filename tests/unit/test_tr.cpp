#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_TR

using namespace cfbox::test;

TEST(TrTest, HelpOutput) {
    char a0[] = "tr", a1[] = "--help";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return tr_main(2, argv); });
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find("translate"), std::string::npos);
}

#endif // CFBOX_ENABLE_TR
