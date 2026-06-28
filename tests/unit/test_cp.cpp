#include <sys/stat.h>

#include <ctime>
#include <filesystem>

#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>
#include <cfbox/fs_util.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_CP

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

// --- attribute-preservation helpers (raw stat avoids file_time_type glue) ---
namespace {
auto set_mtime(const std::string& path, time_t t) -> void {
    struct timespec times[2] = {{t, 0}, {t, 0}};
    auto r = cfbox::fs::set_times(path, times, /*no_follow=*/false);
    (void)r;
}
auto mode_of(const std::string& path) -> unsigned {
    struct stat st{};
    ::stat(path.c_str(), &st);
    return st.st_mode & 0777;
}
auto mtime_of(const std::string& path) -> time_t {
    struct stat st{};
    ::stat(path.c_str(), &st);
    return st.st_mtime;
}
} // namespace

TEST(CpTest, ArchiveCopyPreservesMode) {
    TempDir tmp;
    auto src = tmp.write_file("src.txt", "data");
    std::filesystem::permissions(src, std::filesystem::perms::owner_read | std::filesystem::perms::owner_exec);
    auto dst = (tmp.path / "dst.txt").string();
    char a0[] = "cp", a1[] = "-a", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", src.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    EXPECT_EQ(mode_of(src), mode_of(dst));
}

TEST(CpTest, ArchiveCopyPreservesTimestamp) {
    TempDir tmp;
    auto src = tmp.write_file("src.txt", "data");
    set_mtime(src, 1'000'000'000);  // fixed old time, distinct from "now"
    auto dst = (tmp.path / "dst.txt").string();
    char a0[] = "cp", a1[] = "-a", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", src.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    EXPECT_EQ(mtime_of(src), mtime_of(dst));
}

TEST(CpTest, ArchiveCopySymlinkAsLink) {
    TempDir tmp;
    auto target = tmp.write_file("target.txt", "x");
    auto link = (tmp.path / "link").string();
    std::filesystem::create_symlink(target, link);
    auto dstlink = (tmp.path / "dstlink").string();
    char a0[] = "cp", a1[] = "-a", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", link.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dstlink.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    // Destination must be a symlink, not a followed copy of the target.
    EXPECT_TRUE(std::filesystem::is_symlink(std::filesystem::symlink_status(dstlink)));
    auto t1 = cfbox::fs::read_symlink(link);
    auto t2 = cfbox::fs::read_symlink(dstlink);
    ASSERT_TRUE(t1.has_value() && t2.has_value());
    EXPECT_EQ(t1.value(), t2.value());
}

TEST(CpTest, ArchiveCopyBrokenSymlink) {
    TempDir tmp;
    auto link = (tmp.path / "broken").string();
    std::filesystem::create_symlink("/nonexistent/cfbox-target", link);
    auto dst = (tmp.path / "dst").string();
    char a0[] = "cp", a1[] = "-a", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", link.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    EXPECT_TRUE(std::filesystem::is_symlink(std::filesystem::symlink_status(dst)));
}

TEST(CpTest, ArchiveCopyTree) {
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "srcdir" / "sub");
    auto a = tmp.write_file("srcdir/a.txt", "aaa");
    tmp.write_file("srcdir/sub/b.txt", "bbb");
    std::filesystem::permissions(a, std::filesystem::perms::owner_read | std::filesystem::perms::group_read);
    auto srcdir = (tmp.path / "srcdir").string();
    auto dstdir = (tmp.path / "dstdir").string();
    char a0[] = "cp", a1[] = "-a", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", srcdir.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dstdir.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dstdir" / "a.txt"));
    EXPECT_TRUE(std::filesystem::exists(tmp.path / "dstdir" / "sub" / "b.txt"));
    EXPECT_EQ(mode_of((tmp.path / "dstdir" / "a.txt").string()), mode_of(a));
}

TEST(CpTest, ArchiveCopyDirectoryTimestamp) {
    TempDir tmp;
    std::filesystem::create_directories(tmp.path / "srcdir" / "sub");
    tmp.write_file("srcdir/sub/inner.txt", "x");  // child creation would reset mtime
    auto srcdir = (tmp.path / "srcdir").string();
    auto dstdir = (tmp.path / "dstdir").string();
    set_mtime(srcdir, 1'500'000'000);
    char a0[] = "cp", a1[] = "-a", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", srcdir.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dstdir.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    EXPECT_EQ(mtime_of(srcdir), mtime_of(dstdir));
}

TEST(CpTest, PreserveKeepsTimestamp) {
    TempDir tmp;
    auto src = tmp.write_file("src.txt", "data");
    set_mtime(src, 1'200'000'000);
    auto dst = (tmp.path / "dst.txt").string();
    char a0[] = "cp", a1[] = "-p", a2[256], a3[256];
    std::snprintf(a2, sizeof(a2), "%s", src.c_str());
    std::snprintf(a3, sizeof(a3), "%s", dst.c_str());
    char* argv[] = {a0, a1, a2, a3};
    EXPECT_EQ(cp_main(4, argv), 0);
    EXPECT_EQ(mtime_of(src), mtime_of(dst));
}

#endif // CFBOX_ENABLE_CP
