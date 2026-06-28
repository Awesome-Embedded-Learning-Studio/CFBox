#include <cstdio>

#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_GREP

using namespace cfbox::test;

TEST(GrepTest, BasicMatch) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello world\nfoo bar\nbaz qux\n");
    char a0[] = "grep", a1[] = "hello", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return grep_main(3, argv); });
    EXPECT_EQ(out, "hello world\n");
}

TEST(GrepTest, NoMatchReturnsOne) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello world\n");
    char a0[] = "grep", a1[] = "nonexistent", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    int rc = 1;
    capture_stdout([&]{ rc = grep_main(3, argv); return 0; });
    EXPECT_EQ(rc, 1);
}

TEST(GrepTest, InvertMatch) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\nfoo\nbar\n");
    char a0[] = "grep", a1[] = "-v", a2[] = "hello", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_NE(out.find("foo\n"), std::string::npos);
    EXPECT_NE(out.find("bar\n"), std::string::npos);
    EXPECT_EQ(out.find("hello"), std::string::npos);
}

TEST(GrepTest, LineNumbers) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "aaa\nbbb\naaa\n");
    char a0[] = "grep", a1[] = "-n", a2[] = "aaa", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_NE(out.find("1:aaa"), std::string::npos);
    EXPECT_NE(out.find("3:aaa"), std::string::npos);
}

TEST(GrepTest, CountOnly) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "hello\nworld\nhello\n");
    char a0[] = "grep", a1[] = "-c", a2[] = "hello", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_EQ(out, "2\n");
}

TEST(GrepTest, MultiFileShowsFilename) {
    TempDir tmp;
    auto f1 = tmp.write_file("a.txt", "match\nnope\n");
    auto f2 = tmp.write_file("b.txt", "match\n");
    char a0[] = "grep", a1[] = "match", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", f1.c_str());
    std::snprintf(a3, sizeof(a3), "%s", f2.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_NE(out.find("a.txt:match"), std::string::npos);
    EXPECT_NE(out.find("b.txt:match"), std::string::npos);
}

TEST(GrepTest, IgnoreCase) {
    TempDir tmp;
    auto f = tmp.write_file("data.txt", "Hello World\n");
    char a0[] = "grep", a1[] = "-i", a2[] = "hello", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_EQ(out, "Hello World\n");
}

TEST(GrepTest, RecursiveSearch) {
    TempDir tmp;
    std::filesystem::create_directory(tmp.path / "sub");
    tmp.write_file("sub/deep.txt", "found_it\nnope\n");
    char a0[] = "grep", a1[] = "-r", a2[] = "found_it", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_NE(out.find("found_it"), std::string::npos);
}

TEST(GrepTest, MissingPattern) {
    char a0[] = "grep";
    char* argv[] = {a0};
    int rc = 0;
    capture_stdout([&]{ rc = grep_main(1, argv); return 0; });
    EXPECT_EQ(rc, 2);
}

// --- -A/-B/-C context windows ---
TEST(GrepTest, ContextAfter) {
    TempDir tmp;
    auto f = tmp.write_file("d.txt", "match\nx\nmatch\n");
    char a0[] = "grep", a1[] = "-A1", a2[] = "match", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_EQ(out, "match\nx\nmatch\n");
}

TEST(GrepTest, ContextBefore) {
    TempDir tmp;
    auto f = tmp.write_file("d.txt", "x\nmatch\n");
    char a0[] = "grep", a1[] = "-B1", a2[] = "match", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_EQ(out, "x\nmatch\n");
}

TEST(GrepTest, ContextBoth) {
    TempDir tmp;
    auto f = tmp.write_file("d.txt", "x\nmatch\ny\n");
    char a0[] = "grep", a1[] = "-C1", a2[] = "match", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_EQ(out, "x\nmatch\ny\n");
}

TEST(GrepTest, ContextSeparatorBetweenGroups) {
    TempDir tmp;
    auto f = tmp.write_file("d.txt", "match\na\nb\nmatch\n");
    char a0[] = "grep", a1[] = "-A1", a2[] = "match", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_EQ(out, "match\na\n--\nmatch\n");
}

TEST(GrepTest, ContextZeroIsPlainGrep) {
    TempDir tmp;
    auto f = tmp.write_file("d.txt", "match\nx\nmatch\n");
    char a0[] = "grep", a1[] = "-A0", a2[] = "match", a3[256];
    std::snprintf(a3, sizeof(a3), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return grep_main(4, argv); });
    EXPECT_EQ(out, "match\nmatch\n");
}

TEST(GrepTest, ContextWithLineNumbers) {
    TempDir tmp;
    auto f = tmp.write_file("d.txt", "match\nx\nmatch\n");
    char a0[] = "grep", a1[] = "-n", a2[] = "-A1", a3[] = "match", a4[256];
    std::snprintf(a4, sizeof(a4), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3, a4};
    auto out = capture_stdout([&]{ return grep_main(5, argv); });
    EXPECT_EQ(out, "1:match\n2:x\n3:match\n");
}

TEST(GrepTest, ContextInvalidNumberExits2) {
    TempDir tmp;
    auto f = tmp.write_file("d.txt", "match\n");
    char a0[] = "grep", a1[] = "-A", a2[] = "abc", a3[] = "match", a4[256];
    std::snprintf(a4, sizeof(a4), "%s", f.c_str());
    char* argv[] = {a0, a1, a2, a3, a4};
    int rc = 0;
    capture_stdout([&]{ rc = grep_main(5, argv); return 0; });
    EXPECT_EQ(rc, 2);
}

#endif // CFBOX_ENABLE_GREP
