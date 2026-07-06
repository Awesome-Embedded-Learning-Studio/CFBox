// Unit tests for net_util socket parsing (netstat path). Pure functions fed
// fixed /proc/net/tcp|udp|unix snapshots — no live privileges required.

#include <string>

#include <cfbox/net_util.hpp>

#include <gtest/gtest.h>

TEST(NetUtilSplitFieldsTest, TabsAndSpaces) {
    auto f = cfbox::net::split_fields("eth0\t00000000\t00000000\t0001  \t");
    ASSERT_EQ(f.size(), 4u);
    EXPECT_EQ(f[0], "eth0");
    EXPECT_EQ(f[3], "0001");
}

TEST(NetUtilSplitFieldsTest, CollapsesRunsAndTrims) {
    auto f = cfbox::net::split_fields("   a   b   c   ");
    ASSERT_EQ(f.size(), 3u);
    EXPECT_EQ(f[2], "c");
}

TEST(NetUtilSplitFieldsTest, Empty) {
    EXPECT_TRUE(cfbox::net::split_fields("").empty());
    EXPECT_TRUE(cfbox::net::split_fields("    ").empty());
}

TEST(NetUtilTcpStateTest, KnownCodes) {
    EXPECT_STREQ(cfbox::net::tcp_state_name(0x0A), "LISTEN");
    EXPECT_STREQ(cfbox::net::tcp_state_name(0x01), "ESTABLISHED");
    EXPECT_STREQ(cfbox::net::tcp_state_name(0x06), "TIME_WAIT");
    EXPECT_STREQ(cfbox::net::tcp_state_name(0xFF), "UNKNOWN");
}

TEST(NetUtilParseInetLineTest, ListenRow) {
    // 0100007F = 127.0.0.1 (little-endian hex), port 0x16=22, state 0A=LISTEN
    auto e = cfbox::net::parse_inet_line(
        "   0: 0100007F:0016 00000000:0000 0A 00000000:00000000 00:00000000 00000000 0 0 12345",
        "tcp");
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->proto, "tcp");
    EXPECT_EQ(e->local_addr, "127.0.0.1");
    EXPECT_EQ(e->local_port, 22u);
    EXPECT_EQ(e->remote_addr, "0.0.0.0");
    EXPECT_EQ(e->remote_port, 0u);
    EXPECT_EQ(e->state, 0x0A);
}

TEST(NetUtilParseInetLineTest, RejectsHeader) {
    auto e = cfbox::net::parse_inet_line(
        "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt", "tcp");
    EXPECT_FALSE(e.has_value());
}

TEST(NetUtilParseInetSocketsTest, SkipsHeaderParsesRows) {
    std::string dump =
        "  sl  local_address rem_address   st tx_queue rx_queue tr tm->when retrnsmt   uid  "
        "timeout inode\n"
        "   0: 0100007F:0016 00000000:0000 0A 00000000:00000000 00:00000000 00000000     0     "
        "0 12345 1 0 0 10 0\n"
        "   1: 0100007F:D0A8 0100007F:0016 01 00000000:00000000 00:00000000 00000000     0     "
        "0 67890 1 0 0 20 4 30 10\n";
    auto v = cfbox::net::parse_inet_sockets(dump, "tcp");
    ASSERT_EQ(v.size(), 2u);
    EXPECT_EQ(v[0].local_port, 22u);
    EXPECT_EQ(v[0].state, 0x0A);
    EXPECT_EQ(v[1].local_port, 0xD0A8u);
    EXPECT_EQ(v[1].state, 0x01);
    EXPECT_EQ(v[1].remote_port, 22u);
}

TEST(NetUtilFormatNetstatInetTest, HidesListenByDefault) {
    std::vector<cfbox::net::SocketEntry> v(2);
    v[0].proto = "tcp";
    v[0].local_addr = "127.0.0.1";
    v[0].local_port = 22;
    v[0].remote_addr = "0.0.0.0";
    v[0].state = 0x0A; // LISTEN
    v[1].proto = "tcp";
    v[1].local_addr = "127.0.0.1";
    v[1].local_port = 50000;
    v[1].remote_addr = "127.0.0.1";
    v[1].remote_port = 22;
    v[1].state = 0x01; // ESTABLISHED

    auto no_servers = cfbox::net::format_netstat_inet(v, false);
    EXPECT_NE(no_servers.find("(w/o servers)"), std::string::npos);
    EXPECT_EQ(no_servers.find("LISTEN"), std::string::npos); // skipped
    EXPECT_NE(no_servers.find("ESTABLISHED"), std::string::npos);

    auto with_servers = cfbox::net::format_netstat_inet(v, true);
    EXPECT_NE(with_servers.find("servers and established"), std::string::npos);
    EXPECT_NE(with_servers.find("LISTEN"), std::string::npos);
    // remote port 0 renders as "*"
    EXPECT_NE(with_servers.find("0.0.0.0:*"), std::string::npos);
}

TEST(NetUtilParseUnixLineTest, StreamListening) {
    auto e = cfbox::net::parse_unix_line(
        "7c8c: 00000002 00000000 00000000 0001 01 12345 /dev/log");
    ASSERT_TRUE(e.has_value());
    EXPECT_EQ(e->refcount, 2);
    EXPECT_EQ(e->type, 1);
    EXPECT_EQ(e->state, 0x01);
    EXPECT_EQ(e->inode, 12345u);
    EXPECT_EQ(e->path, "/dev/log");
    EXPECT_STREQ(cfbox::net::unix_type_name(e->type), "STREAM");
    EXPECT_STREQ(cfbox::net::unix_state_name(e->state), "LISTENING");
}

TEST(NetUtilFormatNetstatUnixTest, EmitsRow) {
    std::vector<cfbox::net::UnixSocketEntry> v(1);
    v[0].refcount = 2;
    v[0].type = 1;
    v[0].state = 0x01;
    v[0].inode = 12345;
    v[0].path = "/dev/log";
    auto s = cfbox::net::format_netstat_unix(v);
    EXPECT_NE(s.find("Active UNIX domain sockets"), std::string::npos);
    EXPECT_NE(s.find("/dev/log"), std::string::npos);
    EXPECT_NE(s.find("STREAM"), std::string::npos);
}
