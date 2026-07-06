#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>

#include <cfbox/socket.hpp>

#include <gtest/gtest.h>

// End-to-end loopback echo. listen_on(port=0) → bound_port → dial lands in the
// accept backlog before accept is called (no thread/race needed), then one round
// trip each way exercises make/bind/listen/connect/accept together.
TEST(SocketTest, LoopbackEcho) {
    auto srv = cfbox::socket::listen_on("127.0.0.1", 0);
    ASSERT_TRUE(srv.has_value()) << srv.error().msg;
    auto port = cfbox::socket::bound_port(srv->get());
    ASSERT_TRUE(port.has_value()) << port.error().msg;
    ASSERT_GT(*port, 0);

    auto client = cfbox::socket::dial("127.0.0.1", *port);
    ASSERT_TRUE(client.has_value()) << client.error().msg;

    auto conn = cfbox::socket::accept_one(srv->get());
    ASSERT_TRUE(conn.has_value()) << conn.error().msg;

    ASSERT_EQ(::write(client->get(), "ping", 4), 4);
    char buf[64] = {};
    ASSERT_EQ(::read(conn->get(), buf, 4), 4);
    EXPECT_EQ(std::string(buf, 4), "ping");

    ASSERT_EQ(::write(conn->get(), "pong", 4), 4);
    ASSERT_EQ(::read(client->get(), buf, 4), 4);
    EXPECT_EQ(std::string(buf, 4), "pong");
}

TEST(SocketTest, ResolveLoopback) {
    auto addrs = cfbox::socket::resolve("127.0.0.1", 8080, cfbox::socket::Kind::Stream);
    ASSERT_TRUE(addrs.has_value()) << addrs.error().msg;
    EXPECT_FALSE(addrs->empty());
}

TEST(SocketTest, FormatAddrV4) {
    sockaddr_in sa{};
    sa.sin_family = AF_INET;
    sa.sin_port = htons(8080);
    ::inet_pton(AF_INET, "127.0.0.1", &sa.sin_addr);
    sockaddr_storage ss{};
    std::memcpy(&ss, &sa, sizeof(sa));
    EXPECT_EQ(cfbox::socket::format_addr(ss), "127.0.0.1:8080");
}

TEST(SocketTest, FormatAddrV6) {
    sockaddr_in6 sa{};
    sa.sin6_family = AF_INET6;
    sa.sin6_port = htons(9999);
    ::inet_pton(AF_INET6, "::1", &sa.sin6_addr);
    sockaddr_storage ss{};
    std::memcpy(&ss, &sa, sizeof(sa));
    EXPECT_EQ(cfbox::socket::format_addr(ss), "::1:9999");
}
