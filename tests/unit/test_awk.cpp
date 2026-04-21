#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_AWK

using namespace cfbox::test;

TEST(AwkTest, HelpOutput) {
    char a0[] = "awk", a1[] = "--help";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return awk_main(2, argv); });
    EXPECT_FALSE(out.empty());
    EXPECT_NE(out.find("pattern"), std::string::npos);
}

TEST(AwkTest, SimplePrint) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello world\nfoo bar\n");
    char a0[] = "awk", a1[] = "{print $1}", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return awk_main(3, argv); });
    EXPECT_NE(out.find("hello"), std::string::npos);
    EXPECT_NE(out.find("foo"), std::string::npos);
}

TEST(AwkTest, BeginEnd) {
    char a0[] = "awk", a1[] = "BEGIN{print 42}";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return awk_main(2, argv); });
    EXPECT_NE(out.find("42"), std::string::npos);
}

TEST(AwkTest, SumFields) {
    TempDir tmp;
    auto f = tmp.write_file("nums.txt", "10\n20\n30\n");
    char a0[] = "awk", a1[] = "{s+=$1}END{print s}", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return awk_main(3, argv); });
    EXPECT_NE(out.find("60"), std::string::npos);
}

#endif // CFBOX_ENABLE_AWK
