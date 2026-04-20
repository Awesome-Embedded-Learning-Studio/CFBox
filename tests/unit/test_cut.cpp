#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_CUT

using namespace cfbox::test;

TEST(CutTest, FieldDelimiter) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a:b:c\n");
    char a0[] = "cut", a1[] = "-d:", a2[] = "-f1", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return cut_main(4, argv); });
    EXPECT_EQ(out, "a\n");
}

#endif // CFBOX_ENABLE_CUT
