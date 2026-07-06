#include <net/if.h>

#include <string>

#include <cfbox/net_util.hpp>

#include <gtest/gtest.h>

// Live /proc/net/dev read — every Linux has loopback, so we can assert on it
// without mocking the filesystem.
TEST(NetUtilTest, ReadInterfacesHasLoopback) {
    auto ifs = cfbox::net::read_interfaces();
    ASSERT_TRUE(ifs.has_value()) << ifs.error().msg;
    EXPECT_FALSE(ifs->empty());
    bool has_lo = false;
    for (const auto& it : *ifs) {
        if (it.loopback())
            has_lo = true;
    }
    EXPECT_TRUE(has_lo) << "loopback interface missing";
}

TEST(NetUtilTest, LoopbackHasLocalAddress) {
    auto ifs = cfbox::net::read_interfaces();
    ASSERT_TRUE(ifs.has_value());
    for (const auto& it : *ifs) {
        if (it.loopback()) {
            EXPECT_EQ(it.ipv4_addr, "127.0.0.1");
            EXPECT_GT(it.mtu, 0);
            return;
        }
    }
    FAIL() << "no loopback interface";
}

TEST(NetUtilTest, ParseDevFields) {
    auto nums = cfbox::net::parse_dev_fields("  123   456  0  0  junk 789");
    ASSERT_EQ(nums.size(), 5u);
    EXPECT_EQ(nums[0], 123u);
    EXPECT_EQ(nums[1], 456u);
    EXPECT_EQ(nums[2], 0u);
    EXPECT_EQ(nums[3], 0u);
    EXPECT_EQ(nums[4], 789u);
}

TEST(NetUtilTest, ParseDevFieldsEmpty) {
    auto nums = cfbox::net::parse_dev_fields("   ");
    EXPECT_TRUE(nums.empty());
}

TEST(NetUtilTest, FormatIfconfigLoopback) {
    cfbox::net::InterfaceInfo lo;
    lo.name = "lo";
    lo.flags = IFF_UP | IFF_LOOPBACK | IFF_RUNNING;
    lo.mtu = 65536;
    lo.ipv4_addr = "127.0.0.1";
    lo.netmask = "255.0.0.0";
    lo.rx_bytes = 1024;
    lo.tx_bytes = 2048;
    auto s = cfbox::net::format_ifconfig(lo);
    EXPECT_NE(s.find("lo"), std::string::npos);
    EXPECT_NE(s.find("Local Loopback"), std::string::npos);
    EXPECT_NE(s.find("127.0.0.1"), std::string::npos);
    EXPECT_NE(s.find("LOOPBACK"), std::string::npos);
    EXPECT_NE(s.find("MTU:65536"), std::string::npos);
}

TEST(NetUtilTest, FormatIfconfigEthernet) {
    cfbox::net::InterfaceInfo eth;
    eth.name = "eth0";
    eth.flags = IFF_UP | IFF_BROADCAST | IFF_RUNNING | IFF_MULTICAST;
    eth.mtu = 1500;
    eth.ipv4_addr = "10.0.0.1";
    eth.ipv4_bcast = "10.0.0.255";
    eth.netmask = "255.255.255.0";
    eth.mac_addr = "aa:bb:cc:dd:ee:ff";
    auto s = cfbox::net::format_ifconfig(eth);
    EXPECT_NE(s.find("Ethernet"), std::string::npos);
    EXPECT_NE(s.find("HWaddr aa:bb:cc:dd:ee:ff"), std::string::npos);
    EXPECT_NE(s.find("Bcast:10.0.0.255"), std::string::npos);
    EXPECT_NE(s.find("BROADCAST"), std::string::npos);
    EXPECT_EQ(s.find("LOOPBACK"), std::string::npos); // ethernet line must not say LOOPBACK
}

TEST(NetUtilTest, HexToIpv4RoundTrip) {
    // /proc/net/route stores 192.168.2.1 as host-endian hex 0102A8C0
    EXPECT_EQ(cfbox::net::hex_to_ipv4("0102A8C0"), "192.168.2.1");
    EXPECT_EQ(cfbox::net::hex_to_ipv4("00000000"), "0.0.0.0");
    // 255.255.255.0 = bytes FF FF FF 00 → little-endian hex 00FFFFFF
    EXPECT_EQ(cfbox::net::hex_to_ipv4("00FFFFFF"), "255.255.255.0");
}

TEST(NetUtilTest, PrefixLen) {
    EXPECT_EQ(cfbox::net::prefix_len("255.0.0.0"), 8);
    EXPECT_EQ(cfbox::net::prefix_len("255.255.255.0"), 24);
    EXPECT_EQ(cfbox::net::prefix_len("255.255.255.255"), 32);
    EXPECT_EQ(cfbox::net::prefix_len("0.0.0.0"), 0);
}

TEST(NetUtilTest, FormatRouteTableHasHeader) {
    std::vector<cfbox::net::RouteEntry> r(1);
    r[0].destination = "0.0.0.0";
    r[0].gateway = "192.168.1.1";
    r[0].genmask = "0.0.0.0";
    r[0].iface = "eth0";
    r[0].flags = 0x3; // UP | GATEWAY
    r[0].metric = 0;
    auto s = cfbox::net::format_route_table(r);
    EXPECT_NE(s.find("Kernel IP routing table"), std::string::npos);
    EXPECT_NE(s.find("UG"), std::string::npos);
    EXPECT_NE(s.find("192.168.1.1"), std::string::npos);
}
