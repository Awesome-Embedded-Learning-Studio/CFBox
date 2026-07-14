#include <cfbox/applets.hpp>
#include <cfbox/applet_config.hpp>
#include <cfbox/io.hpp>
#include <gtest/gtest.h>
#include "test_capture.hpp"

#if CFBOX_ENABLE_DD

using namespace cfbox::test;

namespace {
// Run dd with the given operand strings (e.g. "if=...", "bs=1").
auto run_dd(std::vector<std::string> args) -> int {
    std::vector<std::vector<char>> storage;
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>("dd"));
    for (auto& a : args) {
        storage.emplace_back(a.begin(), a.end());
        storage.back().push_back('\0');
        argv.push_back(storage.back().data());
    }
    argv.push_back(nullptr);
    return dd_main(static_cast<int>(argv.size() - 1), argv.data());
}

auto read_file(const std::string& p) -> std::string {
    auto r = cfbox::io::read_all(p);
    return r ? *r : std::string{};
}
} // namespace

TEST(DdTest, BasicFileCopy) {
    TempDir tmp;
    tmp.write_file("in", "hello world");
    auto in = (tmp.path / "in").string();
    auto out = (tmp.path / "out").string();
    EXPECT_EQ(run_dd({"if=" + in, "of=" + out, "bs=1"}), 0);
    EXPECT_EQ(read_file(out), "hello world");
}

TEST(DdTest, CountLimitsBytes) {
    TempDir tmp;
    tmp.write_file("in", "hello world");
    auto in = (tmp.path / "in").string();
    auto out = (tmp.path / "out").string();
    EXPECT_EQ(run_dd({"if=" + in, "of=" + out, "bs=1", "count=5"}), 0);
    EXPECT_EQ(read_file(out), "hello");
}

TEST(DdTest, BsSizeSuffix) {
    TempDir tmp;
    tmp.write_file("in", std::string(2048, 'x'));
    auto in = (tmp.path / "in").string();
    auto out = (tmp.path / "out").string();
    EXPECT_EQ(run_dd({"if=" + in, "of=" + out, "bs=1K"}), 0);
    EXPECT_EQ(read_file(out).size(), 2048u);
}

TEST(DdTest, SkipAndSeek) {
    TempDir tmp;
    tmp.write_file("in", "ABCDEFGHIJ");
    auto in = (tmp.path / "in").string();
    auto out = (tmp.path / "out").string();
    EXPECT_EQ(run_dd({"if=" + in, "of=" + out, "bs=1", "skip=2", "seek=1", "count=3"}), 0);
    // skip 2 input bytes (CDE), seek 1 output byte → CDE at offset 1; byte 0 is a hole (\0)
    auto r = read_file(out);
    EXPECT_EQ(r.size(), 4u);
    EXPECT_EQ(r.substr(1), "CDE");
}

TEST(DdTest, ConvNotruncKeepsTail) {
    TempDir tmp;
    tmp.write_file("out", "XXXXXXXXXX");
    tmp.write_file("in", std::string(3, 'Y'));
    auto in = (tmp.path / "in").string();
    auto out = (tmp.path / "out").string();
    EXPECT_EQ(run_dd({"if=" + in, "of=" + out, "bs=1", "count=3", "conv=notrunc"}), 0);
    EXPECT_EQ(read_file(out), "YYYXXXXXXX");
}

TEST(DdTest, ConvSyncPadsBlock) {
    TempDir tmp;
    tmp.write_file("in", "hello"); // 5 bytes; ibs=8 short-read, sync zero-pads to 8
    auto in = (tmp.path / "in").string();
    auto out = (tmp.path / "out").string();
    EXPECT_EQ(run_dd({"if=" + in, "of=" + out, "bs=8", "count=1", "conv=sync"}), 0);
    auto r = read_file(out);
    EXPECT_EQ(r.size(), 8u);
    EXPECT_EQ(r, std::string("hello\0\0\0", 8));
}

TEST(DdTest, LargeBsReblocks) {
    TempDir tmp;
    tmp.write_file("in", std::string(10000, 'z'));
    auto in = (tmp.path / "in").string();
    auto out = (tmp.path / "out").string();
    // ibs=512 obs=4096: read 512 blocks, write 4096 blocks → must reblock correctly
    EXPECT_EQ(run_dd({"if=" + in, "of=" + out, "ibs=512", "obs=4096"}), 0);
    EXPECT_EQ(read_file(out), std::string(10000, 'z'));
}

TEST(DdTest, InvalidOperandRejected) {
    TempDir tmp;
    auto out = (tmp.path / "out").string();
    EXPECT_NE(run_dd({"bogus=1", "of=" + out}), 0);
}

TEST(DdTest, InvalidBsRejected) {
    TempDir tmp;
    auto out = (tmp.path / "out").string();
    EXPECT_NE(run_dd({"of=" + out, "bs=xyz"}), 0);
}

TEST(DdTest, ZeroBsRejected) {
    TempDir tmp;
    auto out = (tmp.path / "out").string();
    EXPECT_NE(run_dd({"of=" + out, "bs=0"}), 0);
}

#endif // CFBOX_ENABLE_DD
