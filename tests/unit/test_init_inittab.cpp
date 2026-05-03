#include <gtest/gtest.h>
#include <string>
#include <string_view>

// Inline the inittab parser for unit testing (it's in src/applets/init/ which
// is not on the include path for tests). The actual implementation is in
// init_inittab.cpp and tested via integration tests.

namespace {

struct InittabEntry {
    std::string id;
    std::string runlevels;
    std::string action;
    std::string process;
};

auto parse_inittab_line(std::string_view line) -> InittabEntry {
    InittabEntry entry;
    if (line.empty() || line[0] == '#') return entry;

    auto c1 = line.find(':');
    if (c1 == std::string_view::npos) return entry;
    auto c2 = line.find(':', c1 + 1);
    if (c2 == std::string_view::npos) return entry;
    auto c3 = line.find(':', c2 + 1);
    if (c3 == std::string_view::npos) return entry;

    entry.id = std::string(line.substr(0, c1));
    entry.runlevels = std::string(line.substr(c1 + 1, c2 - c1 - 1));
    entry.action = std::string(line.substr(c2 + 1, c3 - c2 - 1));
    entry.process = std::string(line.substr(c3 + 1));

    while (!entry.process.empty() && (entry.process.back() == ' ' || entry.process.back() == '\t' || entry.process.back() == '\r'))
        entry.process.pop_back();

    return entry;
}

} // anonymous namespace

TEST(InittabTest, ParseFullLine) {
    auto entry = parse_inittab_line("tty1::respawn:/bin/sh");
    EXPECT_EQ(entry.id, "tty1");
    EXPECT_EQ(entry.runlevels, "");
    EXPECT_EQ(entry.action, "respawn");
    EXPECT_EQ(entry.process, "/bin/sh");
}

TEST(InittabTest, ParseWithRunlevels) {
    auto entry = parse_inittab_line("id:3:once:/usr/bin/foo");
    EXPECT_EQ(entry.id, "id");
    EXPECT_EQ(entry.runlevels, "3");
    EXPECT_EQ(entry.action, "once");
    EXPECT_EQ(entry.process, "/usr/bin/foo");
}

TEST(InittabTest, ParseSysinit) {
    auto entry = parse_inittab_line("::sysinit:/bin/mount -a");
    EXPECT_EQ(entry.id, "");
    EXPECT_EQ(entry.action, "sysinit");
    EXPECT_EQ(entry.process, "/bin/mount -a");
}

TEST(InittabTest, ParseComment) {
    auto entry = parse_inittab_line("# this is a comment");
    EXPECT_TRUE(entry.action.empty());
}

TEST(InittabTest, ParseEmpty) {
    auto entry = parse_inittab_line("");
    EXPECT_TRUE(entry.action.empty());
}

TEST(InittabTest, ParseTrimsWhitespace) {
    auto entry = parse_inittab_line("id::once:/bin/true  ");
    EXPECT_EQ(entry.process, "/bin/true");
}

TEST(InittabTest, ParseCtrlAltDel) {
    auto entry = parse_inittab_line("::ctrlaltdel:/sbin/reboot");
    EXPECT_EQ(entry.action, "ctrlaltdel");
    EXPECT_EQ(entry.process, "/sbin/reboot");
}

TEST(InittabTest, ParseShutdown) {
    auto entry = parse_inittab_line("::shutdown:/bin/umount -a");
    EXPECT_EQ(entry.action, "shutdown");
    EXPECT_EQ(entry.process, "/bin/umount -a");
}
