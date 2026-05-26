#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_CAT

using namespace cfbox::test;

TEST(CatTest, SingleFile) {
    TempDir tmp;
    tmp.write_file("hello.txt", "hello world\n");
    auto f = (tmp.path / "hello.txt").string();
    char a0[] = "cat", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return cat_main(2, argv); });
    EXPECT_EQ(out, "hello world\n");
}

TEST(CatTest, MultipleFiles) {
    TempDir tmp;
    tmp.write_file("a.txt", "aaa\n");
    tmp.write_file("b.txt", "bbb\n");
    auto fa = (tmp.path / "a.txt").string();
    auto fb = (tmp.path / "b.txt").string();
    char a0[] = "cat", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", fa.c_str());
    std::snprintf(a2, sizeof(a2), "%s", fb.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return cat_main(3, argv); });
    EXPECT_EQ(out, "aaa\nbbb\n");
}

TEST(CatTest, NumberAllLines) {
    TempDir tmp;
    tmp.write_file("num.txt", "one\n\ntwo\n");
    auto f = (tmp.path / "num.txt").string();
    char a0[] = "cat", a1[] = "-n", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return cat_main(3, argv); });
    EXPECT_NE(out.find("1\tone"), std::string::npos);
    EXPECT_NE(out.find("2\t"), std::string::npos);
    EXPECT_NE(out.find("3\ttwo"), std::string::npos);
}

TEST(CatTest, NumberNonEmptyLines) {
    TempDir tmp;
    tmp.write_file("num.txt", "one\n\ntwo\n");
    auto f = (tmp.path / "num.txt").string();
    char a0[] = "cat", a1[] = "-b", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return cat_main(3, argv); });
    EXPECT_NE(out.find("1\tone"), std::string::npos);
    EXPECT_NE(out.find("2\ttwo"), std::string::npos);
}

TEST(CatTest, NonexistentFile) {
    char a0[] = "cat", a1[] = "/no/such/file.txt";
    char* argv[] = {a0, a1};
    EXPECT_NE(cat_main(2, argv), 0);
}

#endif // CFBOX_ENABLE_CAT
