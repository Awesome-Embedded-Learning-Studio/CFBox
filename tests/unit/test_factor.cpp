#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_FACTOR

using namespace cfbox::test;

TEST(FactorTest, CompositeNumber) {
    char a0[] = "factor", a1[] = "12";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return factor_main(2, argv); });
    EXPECT_EQ(out, "12: 2 2 3\n");
}

TEST(FactorTest, PrimeNumber) {
    char a0[] = "factor", a1[] = "7";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return factor_main(2, argv); });
    EXPECT_EQ(out, "7: 7\n");
}

#endif // CFBOX_ENABLE_FACTOR
