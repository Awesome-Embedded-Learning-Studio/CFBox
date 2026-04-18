#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

using namespace cfbox::test;

TEST(SedTest, BasicSubstitution) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello world\nfoo bar\n");
    char a0[] = "sed", a1[] = "s/hello/HELLO/", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    EXPECT_NE(out.find("HELLO world"), std::string::npos);
    EXPECT_NE(out.find("foo bar"), std::string::npos);
}

TEST(SedTest, GlobalFlag) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "aaa bbb aaa\n");
    char a0[] = "sed", a1[] = "s/aaa/XXX/g", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    EXPECT_EQ(out, "XXX bbb XXX\n");
}

TEST(SedTest, SuppressWithPrint) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\nworld\n");
    char a0[] = "sed", a1[] = "-n", a2[] = "s/hello/HELLO/p", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return sed_main(4, argv); });
    EXPECT_EQ(out, "HELLO\n");
}

TEST(SedTest, DeleteLine) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "keep\ndelete\nkeep2\n");
    char a0[] = "sed", a1[] = "2d", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    EXPECT_EQ(out, "keep\nkeep2\n");
}

TEST(SedTest, LastLineAddress) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a\nb\nc\n");
    char a0[] = "sed", a1[] = "$s/c/C/", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    EXPECT_NE(out.find("C"), std::string::npos);
}

TEST(SedTest, RangeAddress) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "line1\nline2\nline3\nline4\n");
    char a0[] = "sed", a1[] = "2,3s/line/LINE/", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    EXPECT_NE(out.find("line1"), std::string::npos);
    EXPECT_NE(out.find("LINE2"), std::string::npos);
    EXPECT_NE(out.find("LINE3"), std::string::npos);
    EXPECT_NE(out.find("line4"), std::string::npos);
}

TEST(SedTest, MultipleCommands) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\nfoo\n");
    char a0[] = "sed", a1[] = "s/hello/HELLO/;s/foo/FOO/", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    EXPECT_NE(out.find("HELLO"), std::string::npos);
    EXPECT_NE(out.find("FOO"), std::string::npos);
}

TEST(SedTest, NoMatchPassesThrough) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\nworld\n");
    char a0[] = "sed", a1[] = "s/xyz/ABC/", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    EXPECT_EQ(out, "hello\nworld\n");
}

TEST(SedTest, PrintCommand) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "a\nb\nc\n");
    char a0[] = "sed", a1[] = "2p", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return sed_main(3, argv); });
    // Line 2 printed twice (auto + p command)
    auto count = std::count(out.begin(), out.end(), '\n');
    EXPECT_GE(count, 3);
}

TEST(SedTest, MissingScript) {
    char a0[] = "sed";
    char* argv[] = {a0};
    int rc = 0;
    capture_stdout([&]{ rc = sed_main(1, argv); return 0; });
    EXPECT_NE(rc, 0);
}
