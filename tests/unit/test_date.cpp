#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_DATE

using namespace cfbox::test;

TEST(DateTest, NonEmptyOutput) {
    char a0[] = "date";
    char* argv[] = {a0};
    auto out = capture_stdout([&]{ return date_main(1, argv); });
    EXPECT_FALSE(out.empty());
}

TEST(DateTest, YearFormat) {
    char a0[] = "date", a1[] = "+%Y";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return date_main(2, argv); });
    ASSERT_GE(out.size(), 4u);
    // Verify it is a 4-digit year (starts with 1 or 2)
    EXPECT_TRUE(out[0] == '1' || out[0] == '2');
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(out[i] >= '0' && out[i] <= '9');
    }
}

#endif // CFBOX_ENABLE_DATE
