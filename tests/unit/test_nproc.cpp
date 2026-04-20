#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>

#include "test_capture.hpp"

#if CFBOX_ENABLE_NPROC

using namespace cfbox::test;

TEST(NprocTest, ReturnsPositiveNumber) {
    char a0[] = "nproc";
    char* argv[] = {a0, nullptr};
    auto out = capture_stdout([&] { return nproc_main(1, argv); });
    EXPECT_FALSE(out.empty());
    int val = std::stoi(out);
    EXPECT_GT(val, 0);
}

TEST(NprocTest, IgnoreLarge) {
    char a0[] = "nproc", a1[] = "--ignore=999";
    char* argv[] = {a0, a1, nullptr};
    auto out = capture_stdout([&] { return nproc_main(2, argv); });
    int val = std::stoi(out);
    EXPECT_EQ(val, 1);
}

#endif // CFBOX_ENABLE_NPROC
