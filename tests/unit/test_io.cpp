#include <cfbox/io.hpp>
#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>

using namespace cfbox::io;

namespace {

class IOTest : public ::testing::Test {
protected:
    std::string test_dir_;

    void SetUp() override {
        test_dir_ = "/tmp/cfbox_test_" + std::to_string(::getpid());
        std::string cmd = "mkdir -p " + test_dir_;
        int ret [[maybe_unused]] = std::system(cmd.c_str());
    }

    void TearDown() override {
        std::string cmd = "rm -rf " + test_dir_;
        int ret [[maybe_unused]] = std::system(cmd.c_str());
    }

    auto test_file(const std::string& name) const -> std::string {
        return test_dir_ + "/" + name;
    }
};

} // namespace

TEST_F(IOTest, WriteAndReadAll) {
    std::string path = test_file("hello.txt");
    auto w = write_all(path, "hello world");
    ASSERT_TRUE(w.has_value());

    auto r = read_all(path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), "hello world");
}

TEST_F(IOTest, ReadNonexistent) {
    auto r = read_all(test_file("nope.txt"));
    EXPECT_FALSE(r.has_value());
    EXPECT_GT(r.error().code, 0);
}

TEST_F(IOTest, WriteNonexistentDir) {
    auto r = write_all("/tmp/cfbox_test_noexist_dir/file.txt", "data");
    EXPECT_FALSE(r.has_value());
}

TEST_F(IOTest, SplitLines) {
    std::string path = test_file("lines.txt");
    static_cast<void>(write_all(path, "line1\nline2\nline3"));

    auto r = read_all(path);
    ASSERT_TRUE(r.has_value());

    auto lines = split_lines(r.value());
    ASSERT_EQ(lines.size(), 3u);
    EXPECT_EQ(lines[0], "line1");
    EXPECT_EQ(lines[1], "line2");
    EXPECT_EQ(lines[2], "line3");
}

TEST_F(IOTest, SplitLinesTrailingNewline) {
    std::string path = test_file("trailing.txt");
    static_cast<void>(write_all(path, "a\nb\n"));

    auto r = read_all(path);
    ASSERT_TRUE(r.has_value());

    auto lines = split_lines(r.value());
    ASSERT_EQ(lines.size(), 2u);
    EXPECT_EQ(lines[0], "a");
    EXPECT_EQ(lines[1], "b");
}

TEST_F(IOTest, ReadLines) {
    std::string path = test_file("readlines.txt");
    static_cast<void>(write_all(path, "foo\nbar"));

    auto lines = read_lines(path);
    ASSERT_TRUE(lines.has_value());
    ASSERT_EQ(lines->size(), 2u);
    EXPECT_EQ((*lines)[0], "foo");
    EXPECT_EQ((*lines)[1], "bar");
}

TEST_F(IOTest, EmptyFile) {
    std::string path = test_file("empty.txt");
    static_cast<void>(write_all(path, ""));

    auto r = read_all(path);
    ASSERT_TRUE(r.has_value());
    EXPECT_EQ(r.value(), "");

    auto lines = split_lines(r.value());
    EXPECT_TRUE(lines.empty());
}
