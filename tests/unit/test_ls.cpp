#include <cstdio>
#include <filesystem>

#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_LS

using namespace cfbox::test;

TEST(LsTest, ListDirectory) {
    TempDir tmp;
    tmp.write_file("apple.txt", "");
    tmp.write_file("banana.txt", "");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", dir.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return ls_main(2, argv); });
    EXPECT_NE(out.find("apple.txt"), std::string::npos);
    EXPECT_NE(out.find("banana.txt"), std::string::npos);
}

TEST(LsTest, ListAllWithHidden) {
    TempDir tmp;
    tmp.write_file("visible.txt", "");
    tmp.write_file(".hidden", "");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[] = "-a", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_NE(out.find("visible.txt"), std::string::npos);
    EXPECT_NE(out.find(".hidden"), std::string::npos);
}

TEST(LsTest, ListHiddenNotShownByDefault) {
    TempDir tmp;
    tmp.write_file("visible.txt", "");
    tmp.write_file(".hidden", "");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", dir.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return ls_main(2, argv); });
    EXPECT_NE(out.find("visible.txt"), std::string::npos);
    EXPECT_EQ(out.find(".hidden"), std::string::npos);
}

TEST(LsTest, LongFormatContainsPermissions) {
    TempDir tmp;
    tmp.write_file("file.txt", "data");
    auto f = (tmp.path / "file.txt").string();
    char a0[] = "ls", a1[] = "-l", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", f.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_NE(out.find("-rw"), std::string::npos);
    EXPECT_NE(out.find("file.txt"), std::string::npos);
}

TEST(LsTest, NonexistentPath) {
    char a0[] = "ls", a1[] = "/no/such/path";
    char* argv[] = {a0, a1};
    EXPECT_NE(ls_main(2, argv), 0);
}

TEST(LsTest, SingleFileNoDirectory) {
    TempDir tmp;
    tmp.write_file("only.txt", "data");
    auto f = (tmp.path / "only.txt").string();
    char a0[] = "ls", a1[256];
    std::snprintf(a1, sizeof(a1), "%s", f.c_str());
    char* argv[] = {a0, a1};
    auto out = capture_stdout([&]{ return ls_main(2, argv); });
    EXPECT_EQ(out, "only.txt\n");
}

TEST(LsTest, RecursiveListsNestedDirs) {
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "a" / "b");
    tmp.write_file("a/d.txt", "");
    tmp.write_file("a/b/c.txt", "");
    auto a = (tmp.path / "a").string();
    char a0[] = "ls", a1[] = "-R", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", a.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_NE(out.find("d.txt"), std::string::npos);   // top-level entry
    EXPECT_NE(out.find("c.txt"), std::string::npos);   // proves descent into a/b
}

TEST(LsTest, RecursiveDoesNotFollowSymlinkDir) {
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "real");
    tmp.write_file("real/f.txt", "");
    std::filesystem::create_symlink(tmp.path / "real", tmp.path / "link");
    auto top = tmp.path.string();
    char a0[] = "ls", a1[] = "-R", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", top.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    // f.txt must appear exactly once (under real/); following the symlink would
    // duplicate it via the link -> real descent.
    auto count = [](const std::string& s, const std::string& sub) {
        int n = 0;
        std::string::size_type p = 0;
        while ((p = s.find(sub, p)) != std::string::npos) { ++n; p += sub.size(); }
        return n;
    };
    EXPECT_EQ(count(out, "f.txt"), 1);
}

TEST(LsTest, RecursivePlusLong) {
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "a" / "b");
    tmp.write_file("a/f.txt", "");
    auto a = (tmp.path / "a").string();
    char a0[] = "ls", a1[] = "-Rl", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", a.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_NE(out.find("drwx"), std::string::npos);  // directory long-format lines
    EXPECT_NE(out.find("-rw"), std::string::npos);   // regular file f.txt
}

TEST(LsTest, ColorAlwaysWrapsDirectory) {
    TempDir tmp;
    std::filesystem::create_directory(tmp.path / "sub");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[] = "--color=always", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_NE(out.find("\033[01;34m"), std::string::npos);  // dir = bold blue
}

TEST(LsTest, ColorAutoOffWhenNotTty) {
    TempDir tmp;
    tmp.write_file("f.txt", "");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[] = "--color=auto", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_EQ(out.find("\033["), std::string::npos);  // capture_stdout is non-tty
}

TEST(LsTest, ColorNeverExplicit) {
    TempDir tmp;
    std::filesystem::create_directory(tmp.path / "sub");
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[] = "--color=never", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2};
    auto out = capture_stdout([&]{ return ls_main(3, argv); });
    EXPECT_EQ(out.find("\033["), std::string::npos);
}

TEST(LsTest, ColorInvalidValueExits2) {
    TempDir tmp;
    auto dir = tmp.path.string();
    char a0[] = "ls", a1[] = "--color=bogus", a2[256];
    std::snprintf(a2, sizeof(a2), "%s", dir.c_str());
    char* argv[] = {a0, a1, a2};
    EXPECT_EQ(ls_main(3, argv), 2);
}

#endif // CFBOX_ENABLE_LS
