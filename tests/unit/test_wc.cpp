#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"
#include <cfbox/applet_config.hpp>

#if CFBOX_ENABLE_WC

using namespace cfbox::test;

TEST(WcTest, DefaultAll) {
    TempDir tmp;
    tmp.write_file("test.txt", "hello world\nfoo bar\n");
    auto f = (tmp.path / "test.txt").string();
    char a0[] = "wc", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return wc_main(2, argv); });
    // 2 lines, 4 words, 20 bytes
    EXPECT_NE(out.find("2"), std::string::npos);
    EXPECT_NE(out.find("4"), std::string::npos);
    EXPECT_NE(out.find("20"), std::string::npos);
    EXPECT_NE(out.find("test.txt"), std::string::npos);
}

TEST(WcTest, LinesOnly) {
    TempDir tmp;
    tmp.write_file("test.txt", "a\nb\nc\n");
    auto f = (tmp.path / "test.txt").string();
    char a0[] = "wc", a1[] = "-l", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return wc_main(3, argv); });
    EXPECT_NE(out.find("3"), std::string::npos);
}

TEST(WcTest, WordsOnly) {
    TempDir tmp;
    tmp.write_file("test.txt", "one two three\n");
    auto f = (tmp.path / "test.txt").string();
    char a0[] = "wc", a1[] = "-w", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return wc_main(3, argv); });
    EXPECT_NE(out.find("3"), std::string::npos);
}

TEST(WcTest, BytesOnly) {
    TempDir tmp;
    tmp.write_file("test.txt", "abc");
    auto f = (tmp.path / "test.txt").string();
    char a0[] = "wc", a1[] = "-c", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return wc_main(3, argv); });
    EXPECT_NE(out.find("3"), std::string::npos);
}

TEST(WcTest, EmptyFile) {
    TempDir tmp;
    tmp.write_file("empty.txt", "");
    auto f = (tmp.path / "empty.txt").string();
    char a0[] = "wc", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return wc_main(2, argv); });
    EXPECT_NE(out.find("0"), std::string::npos);
}

TEST(WcTest, MultipleFilesShowsTotal) {
    TempDir tmp;
    tmp.write_file("a.txt", "aaa\n");
    tmp.write_file("b.txt", "bbb\n");
    auto fa = (tmp.path / "a.txt").string();
    auto fb = (tmp.path / "b.txt").string();
    char a0[] = "wc", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", fa.c_str());
    std::snprintf(a2, sizeof(a2), "%s", fb.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return wc_main(3, argv); });
    EXPECT_NE(out.find("total"), std::string::npos);
}

#endif // CFBOX_ENABLE_WC
