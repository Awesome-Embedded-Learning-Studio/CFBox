#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_SLEEP

using namespace cfbox::test;

TEST(SleepTest, ZeroReturnsQuickly) {
    char a0[] = "sleep", a1[] = "0";
    char* argv[] = {a0, a1, nullptr};
    auto start = std::chrono::steady_clock::now();
    EXPECT_EQ(sleep_main(2, argv), 0);
    auto elapsed = std::chrono::steady_clock::now() - start;
    EXPECT_LT(std::chrono::duration<double>(elapsed).count(), 0.1);
}

TEST(SleepTest, InvalidArg) {
    char a0[] = "sleep", a1[] = "abc";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(sleep_main(2, argv), 1);
}

TEST(SleepTest, NegativeArg) {
    char a0[] = "sleep", a1[] = "-1";
    char* argv[] = {a0, a1, nullptr};
    EXPECT_EQ(sleep_main(2, argv), 1);
}

TEST(SleepTest, MissingOperand) {
    char a0[] = "sleep";
    char* argv[] = {a0, nullptr};
    EXPECT_EQ(sleep_main(1, argv), 1);
}

#endif // CFBOX_ENABLE_SLEEP
