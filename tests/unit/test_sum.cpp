#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_SUM

using namespace cfbox::test;

TEST(SumTest, OnFile) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello world\n");
    char a0[] = "sum", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return sum_main(2, argv); });
    EXPECT_FALSE(out.empty());
    // Should contain digits and spaces
    bool has_digit = false;
    for (char c : out) { if (c >= '0' && c <= '9') { has_digit = true; break; } }
    EXPECT_TRUE(has_digit);
}

#endif // CFBOX_ENABLE_SUM
