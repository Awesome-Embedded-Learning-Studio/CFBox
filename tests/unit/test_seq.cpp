#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_SEQ

using namespace cfbox::test;

TEST(SeqTest, SingleArg) {
    char a0[] = "seq", a1[] = "5";
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return seq_main(2, argv); });
    EXPECT_EQ(out, "1\n2\n3\n4\n5\n");
}

TEST(SeqTest, TwoArgs) {
    char a0[] = "seq", a1[] = "2", a2[] = "5";
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return seq_main(3, argv); });
    EXPECT_EQ(out, "2\n3\n4\n5\n");
}

TEST(SeqTest, ThreeArgs) {
    char a0[] = "seq", a1[] = "1", a2[] = "2", a3[] = "5";
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return seq_main(4, argv); });
    EXPECT_EQ(out, "1\n3\n5\n");
}

TEST(SeqTest, Separator) {
    char a0[] = "seq", a1[] = "-s,", a2[] = "3";
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return seq_main(3, argv); });
    EXPECT_EQ(out, "1,2,3\n");
}

#endif // CFBOX_ENABLE_SEQ
