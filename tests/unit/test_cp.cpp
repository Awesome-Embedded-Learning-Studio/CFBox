#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

using namespace cfbox::test;

TEST(CpTest, CopySingleFile) {
    TempDir tmp;
    auto src = tmp.write_file("src.txt", "hello world");
    auto dst = (tmp.path / "dst.txt").string();
    char a0[] = "cp", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", src.c_str());
    std::snprintf(a2, sizeof(a2), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_EQ(cp_main(3, argv), 0);
    auto content = cfbox::io::read_all(dst);
    ASSERT_TRUE(content.has_value());
    EXPECT_EQ(content.value(), "hello world");
}

TEST(CpTest, CopyIntoDirectory) {
    TempDir tmp;
    auto src = tmp.write_file("file.txt", "data");
    std::filesystem::create_directory(tmp.path / "dir");
    auto dir = (tmp.path / "dir").string();
    char a0[] = "cp", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", src.c_str());
    std::snprintf(a2, sizeof(a2), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_EQ(cp_main(3, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dir" / "file.txt"));
}

TEST(CpTest, CopyRecursive) {
    TempDir tmp;
    std::filesystem::create_directory(tmp.path / "srcdir");
    tmp.write_file("srcdir/a.txt", "aaa");
    tmp.write_file("srcdir/b.txt", "bbb");
    auto srcdir = (tmp.path / "srcdir").string();
    auto dstdir = (tmp.path / "dstdir").string();
    char a0[] = "cp", a1[] = "-r", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", srcdir.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dstdir.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dstdir" / "a.txt"));
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dstdir" / "b.txt"));
}

TEST(CpTest, MissingOperands) {
    char a0[] = "cp";
    char* argv[] = {a0};
    EXPECT_NE(cp_main(1, argv), 0);
}

TEST(CpTest, SourceNotExist) {
    TempDir tmp;
    auto fake = (tmp.path / "nonexistent").string();
    auto dst = (tmp.path / "dst").string();
    char a0[] = "cp", a1[256], a2[256];
    std::snprintf(a1, sizeof(a1), "%s", fake.c_str());
    std::snprintf(a2, sizeof(a2), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_NE(cp_main(3, argv), 0);
}
