// Unit tests for icmp.hpp pure functions (checksum / build_echo_request / parse_icmp).
// No raw sockets needed — these cover the byte-level contracts ping/traceroute rely on.

#include <netinet/ip_icmp.h>

#include <cstdint>

#include <cfbox/icmp.hpp>

#include <gtest/gtest.h>

TEST(IcmpChecksumTest, AllZeroTwoBytes) {
    const std::uint8_t z[] = {0, 0};
    EXPECT_EQ(cfbox::icmp::checksum(z), 0xffff);
}

TEST(IcmpChecksumTest, AllOnesTwoBytes) {
    const std::uint8_t o[] = {0xff, 0xff};
    EXPECT_EQ(cfbox::icmp::checksum(o), 0x0000);
}

TEST(IcmpChecksumTest, OddLengthHighOctet) {
    // Single 0x00 byte → treated as high octet of a 16-bit word → sum 0 → ~ = 0xffff.
    const std::uint8_t o[] = {0x00};
    EXPECT_EQ(cfbox::icmp::checksum(o), 0xffff);
}

TEST(IcmpChecksumTest, CarryFold) {
    // 0xffff*3 = 0x2fffd → fold (0xfffd + 0x2) = 0xffff → ~ = 0x0000
    const std::uint8_t o[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
    EXPECT_EQ(cfbox::icmp::checksum(o), 0x0000);
}

TEST(IcmpBuildEchoRequestTest, HeaderLayout) {
    std::uint8_t out[16] = {};
    const std::uint8_t payload[] = {0xaa, 0xbb};
    auto n = cfbox::icmp::build_echo_request(0x1234, 0x0001, payload, out);
    EXPECT_EQ(n, 10u);
    EXPECT_EQ(out[0], ICMP_ECHO);
    EXPECT_EQ(out[1], 0);
    EXPECT_EQ(out[4], 0x12); // id hi
    EXPECT_EQ(out[5], 0x34); // id lo
    EXPECT_EQ(out[6], 0x00); // seq hi
    EXPECT_EQ(out[7], 0x01); // seq lo
    EXPECT_EQ(out[8], 0xaa);
    EXPECT_EQ(out[9], 0xbb);
}

TEST(IcmpBuildEchoRequestTest, ChecksumSelfConsistent) {
    // A correct ICMP checksum makes the whole packet checksum to 0 (the receiver
    // sums header+payload including the checksum field → 0).
    std::uint8_t out[64] = {};
    std::uint8_t payload[56];
    for (int i = 0; i < 56; ++i)
        payload[i] = static_cast<std::uint8_t>(i);
    auto n = cfbox::icmp::build_echo_request(0xbeef, 0x0007, payload, out);
    ASSERT_GT(n, 0u);
    EXPECT_EQ(cfbox::icmp::checksum({out, n}), 0u);
}

TEST(IcmpBuildEchoRequestTest, TooSmallOut) {
    std::uint8_t out[4] = {};
    const std::uint8_t payload[] = {1, 2};
    EXPECT_EQ(cfbox::icmp::build_echo_request(1, 1, payload, out), 0u);
}

TEST(IcmpParseTest, EchoReplyStripsIpHeader) {
    // 20-byte IP header (IHL=5) + 8-byte ICMP echo reply.
    std::uint8_t pkt[28] = {};
    pkt[0] = 0x45; // version 4, IHL 5
    pkt[8] = 64;   // TTL
    pkt[20] = ICMP_ECHOREPLY;
    pkt[24] = 0x12;
    pkt[25] = 0x34; // id
    pkt[26] = 0x00;
    pkt[27] = 0x05; // seq
    auto m = cfbox::icmp::parse_icmp({pkt, 28});
    ASSERT_TRUE(m.has_value()) << m.error().msg;
    EXPECT_EQ(m->type, ICMP_ECHOREPLY);
    EXPECT_EQ(m->id, 0x1234);
    EXPECT_EQ(m->seq, 0x0005);
    EXPECT_EQ(m->recv_ttl, 64);
}

TEST(IcmpParseTest, HandlesIpOptions) {
    // IHL=6 → 24-byte IP header (4 bytes options). ICMP starts at offset 24.
    std::uint8_t pkt[32] = {};
    pkt[0] = 0x46;
    pkt[24] = ICMP_ECHOREPLY;
    pkt[28] = 0xab;
    pkt[29] = 0xcd; // id
    auto m = cfbox::icmp::parse_icmp({pkt, 32});
    ASSERT_TRUE(m.has_value());
    EXPECT_EQ(m->type, ICMP_ECHOREPLY);
    EXPECT_EQ(m->id, 0xabcd);
}

TEST(IcmpParseTest, TooShort) {
    const std::uint8_t pkt[10] = {};
    auto m = cfbox::icmp::parse_icmp({pkt, 10});
    EXPECT_FALSE(m.has_value());
}

TEST(IcmpParseTest, BadIhl) {
    // IHL=2 (8 bytes) — invalid, below the 20-byte minimum.
    std::uint8_t pkt[28] = {};
    pkt[0] = 0x42;
    auto m = cfbox::icmp::parse_icmp({pkt, 28});
    EXPECT_FALSE(m.has_value());
}
