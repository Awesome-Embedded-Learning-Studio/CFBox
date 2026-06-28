#include <cstdio>
#include <filesystem>

#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_FIND

using namespace cfbox::test;

TEST(FindTest, FindAllFiles) {
    TempDir tmp;
    tmp.write_file("a.txt", "");
    tmp.write_file("b.cpp", "");
    std::filesystem::create_directory(tmp.path / "sub");
    tmp.write_file("sub/c.txt", "");
    char a0[] = "find", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return find_main(2, argv); });
    EXPECT_NE(out.find("a.txt"), std::string::npos);
    EXPECT_NE(out.find("b.cpp"), std::string::npos);
    EXPECT_NE(out.find("sub"), std::string::npos);
    EXPECT_NE(out.find("c.txt"), std::string::npos);
}

TEST(FindTest, FindByName) {
    TempDir tmp;
    tmp.write_file("match.txt", "");
    tmp.write_file("nomatch.cpp", "");
    tmp.write_file("also.txt", "");
    char a0[] = "find", a1[256], a2[] = "-name", a3[] = "*.txt";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return find_main(4, argv); });
    EXPECT_NE(out.find("match.txt"), std::string::npos);
    EXPECT_NE(out.find("also.txt"), std::string::npos);
    EXPECT_EQ(out.find("nomatch.cpp"), std::string::npos);
}

TEST(FindTest, FindByTypeDir) {
    TempDir tmp;
    tmp.write_file("file.txt", "");
    std::filesystem::create_directory(tmp.path / "mydir");
    char a0[] = "find", a1[256], a2[] = "-type", a3[] = "d";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return find_main(4, argv); });
    EXPECT_NE(out.find("mydir"), std::string::npos);
    EXPECT_EQ(out.find("file.txt"), std::string::npos);
}

TEST(FindTest, FindByTypeFile) {
    TempDir tmp;
    tmp.write_file("file.txt", "");
    std::filesystem::create_directory(tmp.path / "mydir");
    char a0[] = "find", a1[256], a2[] = "-type", a3[] = "f";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return find_main(4, argv); });
    EXPECT_NE(out.find("file.txt"), std::string::npos);
    EXPECT_EQ(out.find("mydir"), std::string::npos);
}

TEST(FindTest, MaxDepth) {
    TempDir tmp;
    std::filesystem::create_directory(tmp.path / "sub");
    tmp.write_file("file1.txt", "");
    tmp.write_file("sub/file2.txt", "");
    char a0[] = "find", a1[256], a2[] = "-maxdepth", a3[] = "1";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3};
    auto out = capture_stdout([&]{ return find_main(4, argv); });
    EXPECT_NE(out.find("file1.txt"), std::string::npos);
    EXPECT_NE(out.find("sub"), std::string::npos);
    EXPECT_EQ(out.find("file2.txt"), std::string::npos);
}

TEST(FindTest, NameAndTypeCombined) {
    TempDir tmp;
    tmp.write_file("data.txt", "");
    std::filesystem::create_directory(tmp.path / "docs.txt");
    char a0[] = "find", a1[256], a2[] = "-name", a3[] = "*.txt",
         a4[] = "-type", a5[] = "f";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3, a4, a5};
    auto out = capture_stdout([&]{ return find_main(6, argv); });
    EXPECT_NE(out.find("data.txt"), std::string::npos);
    EXPECT_EQ(out.find("docs.txt"), std::string::npos);
}

// --- boolean operators ---
TEST(FindTest, FindByOr) {
    TempDir tmp;
    tmp.write_file("a.txt", "");
    tmp.write_file("b.cpp", "");
    tmp.write_file("c.md", "");
    char a0[] = "find", a1[256], a2[] = "-name", a3[] = "*.txt",
         a4[] = "-o", a5[] = "-name", a6[] = "*.cpp";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3, a4, a5, a6};
    auto out = capture_stdout([&]{ return find_main(7, argv); });
    EXPECT_NE(out.find("a.txt"), std::string::npos);
    EXPECT_NE(out.find("b.cpp"), std::string::npos);
    EXPECT_EQ(out.find("c.md"), std::string::npos);
}

TEST(FindTest, FindByNotType) {
    TempDir tmp;
    tmp.write_file("file.txt", "");
    std::filesystem::create_directory(tmp.path / "dir");
    char a0[] = "find", a1[256], a2[] = "!", a3[] = "-type", a4[] = "f";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3, a4};
    auto out = capture_stdout([&]{ return find_main(5, argv); });
    EXPECT_EQ(out.find("file.txt"), std::string::npos);  // excluded by !-type f
    EXPECT_NE(out.find("dir"), std::string::npos);
}

TEST(FindTest, ExplicitAnd) {
    TempDir tmp;
    tmp.write_file("match.txt", "");
    tmp.write_file("nomatch.cpp", "");
    char a0[] = "find", a1[256], a2[] = "-name", a3[] = "*.txt",
         a4[] = "-a", a5[] = "-type", a6[] = "f";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3, a4, a5, a6};
    auto out = capture_stdout([&]{ return find_main(7, argv); });
    EXPECT_NE(out.find("match.txt"), std::string::npos);
    EXPECT_EQ(out.find("nomatch.cpp"), std::string::npos);
}

TEST(FindTest, ParensGrouping) {
    TempDir tmp;
    tmp.write_file("x.txt", "");
    tmp.write_file("y.cpp", "");
    tmp.write_file("z.md", "");
    char a0[] = "find", a1[256], a2[] = "(", a3[] = "-name", a4[] = "*.txt",
         a5[] = "-o", a6[] = "-name", a7[] = "*.cpp", a8[] = ")";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2, a3, a4, a5, a6, a7, a8};
    auto out = capture_stdout([&]{ return find_main(9, argv); });
    EXPECT_NE(out.find("x.txt"), std::string::npos);
    EXPECT_NE(out.find("y.cpp"), std::string::npos);
    EXPECT_EQ(out.find("z.md"), std::string::npos);
}

TEST(FindTest, UnknownPredicateExits1) {
    TempDir tmp;
    char a0[] = "find", a1[256], a2[] = "-badop";
    std::snprintf(a1, sizeof(a1), "%s", tmp.path.string().c_str());
    char* argv[] = {a0, a1, a2};
    int rc = 0;
    capture_stdout([&]{ rc = find_main(3, argv); return 0; });
    EXPECT_EQ(rc, 1);
}

#endif // CFBOX_ENABLE_FIND
