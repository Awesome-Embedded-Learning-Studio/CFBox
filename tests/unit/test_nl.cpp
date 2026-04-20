#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_NL

using namespace cfbox::test;

TEST(NlTest, NumberedLines) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\nworld\n");
    char a0[] = "nl", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return nl_main(2, argv); });
    EXPECT_NE(out.find("1"), std::string::npos);
    EXPECT_NE(out.find("hello"), std::string::npos);
    EXPECT_NE(out.find("2"), std::string::npos);
    EXPECT_NE(out.find("world"), std::string::npos);
}

#endif // CFBOX_ENABLE_NL
