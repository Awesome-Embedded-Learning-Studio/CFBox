#pragma once

// icmp.hpp — ICMP echo primitives shared by ping and traceroute. Header-only,
// errors via cfbox::base::Result, no exceptions/RTTI. Uses SOCK_RAW
// IPPROTO_ICMP (needs CAP_NET_RAW); without it open_raw reports EPERM and the
// applet prints a clear message instead of silently failing.
//
// Raw recv buffers carry a full IP packet (IP header + ICMP). parse_icmp strips
// the IP header via the IHL field. The internet checksum (RFC 1071) is pure and
// unit-tested with known vectors.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <poll.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>
#include <string>

#include <cfbox/error.hpp>
#include <cfbox/io.hpp>

namespace cfbox::icmp {

// Monotonic clock in microseconds — used both for RTT math (ping) and for the
// recv_icmp deadline. CLOCK_MONOTONIC is unaffected by wall-clock jumps.
inline auto now_us() -> long long {
    timespec ts{};
    ::clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<long long>(ts.tv_sec) * 1000000LL + ts.tv_nsec / 1000;
}

// RFC 1071 internet checksum over a byte span: sum 16-bit big-endian words,
// fold carries, complement. Endian-agnostic in result. Pure.
[[nodiscard]] inline auto checksum(std::span<const std::uint8_t> data) -> std::uint16_t {
    std::uint32_t sum = 0;
    std::size_t i = 0;
    for (; i + 1 < data.size(); i += 2)
        sum += (static_cast<std::uint32_t>(data[i]) << 8) | data[i + 1];
    if (i < data.size()) // leftover odd byte → high octet
        sum += static_cast<std::uint32_t>(data[i]) << 8;
    while (sum >> 16)
        sum = (sum & 0xffff) + (sum >> 16);
    return static_cast<std::uint16_t>(~sum);
}

// Build an ICMP echo request (type=ICMP_ECHO, code=0) into `out`: 8-byte header
// + payload, checksum filled. Returns bytes written, or 0 if `out` too small.
[[nodiscard]] inline auto build_echo_request(std::uint16_t id, std::uint16_t seq,
                                             std::span<const std::uint8_t> payload,
                                             std::span<std::uint8_t> out) -> std::size_t {
    if (out.size() < 8 + payload.size())
        return 0;
    out[0] = ICMP_ECHO;
    out[1] = 0;
    out[2] = 0;
    out[3] = 0;
    out[4] = static_cast<std::uint8_t>(id >> 8);
    out[5] = static_cast<std::uint8_t>(id & 0xff);
    out[6] = static_cast<std::uint8_t>(seq >> 8);
    out[7] = static_cast<std::uint8_t>(seq & 0xff);
    if (!payload.empty())
        std::memcpy(out.data() + 8, payload.data(), payload.size());
    auto total = 8 + payload.size();
    auto csum = checksum(out.subspan(0, total));
    out[2] = static_cast<std::uint8_t>(csum >> 8);
    out[3] = static_cast<std::uint8_t>(csum & 0xff);
    return total;
}

struct ParsedIcmp {
    std::uint8_t type;
    std::uint8_t code;
    std::uint16_t id;      // host byte order
    std::uint16_t seq;     // host byte order
    std::uint8_t recv_ttl; // TTL from the received IP header (byte offset 8)
};

// Parse ICMP from a raw recv buffer (INCLUDES the IP header). Strips IP via IHL.
// EINVAL on too-short / malformed input.
[[nodiscard]] inline auto parse_icmp(std::span<const std::uint8_t> raw) -> base::Result<ParsedIcmp> {
    if (raw.size() < 28) // 20 (min IP) + 8 (ICMP header)
        return std::unexpected(base::Error{EINVAL, "icmp packet too short"});
    std::size_t ihl = static_cast<std::size_t>(raw[0] & 0x0f) * 4;
    if (ihl < 20 || raw.size() < ihl + 8)
        return std::unexpected(base::Error{EINVAL, "bad IP header length"});
    const std::uint8_t* p = raw.data() + ihl;
    ParsedIcmp m;
    m.type = p[0];
    m.code = p[1];
    m.id = (static_cast<std::uint16_t>(p[4]) << 8) | p[5];
    m.seq = (static_cast<std::uint16_t>(p[6]) << 8) | p[7];
    m.recv_ttl = raw[8]; // IP header TTL is byte offset 8
    return m;
}

// Open a SOCK_RAW IPPROTO_ICMP socket. Returns EPERM without CAP_NET_RAW.
[[nodiscard]] inline auto open_raw() -> base::Result<io::unique_fd> {
    int fd = ::socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
    if (fd < 0)
        return std::unexpected(base::Error{
            errno, "socket(SOCK_RAW, ICMP) failed: " + std::string(std::strerror(errno))});
    return io::unique_fd{fd};
}

// Classify a received ICMP for traceroute: TIME_EXCEEDED → intermediate hop
// (TTL hit 0 at a router), DEST_UNREACH+PORT_UNREACH → reached destination
// (our UDP high port has no listener), anything else → "other" (logged but
// treated as a response from that hop). Pure — unit-testable without sockets.
enum class reply_class { intermediate, reached, other };
inline auto classify_reply(std::uint8_t type, std::uint8_t code) -> reply_class {
    if (type == ICMP_TIME_EXCEEDED)
        return reply_class::intermediate;
    if (type == ICMP_DEST_UNREACH && code == ICMP_PORT_UNREACH)
        return reply_class::reached;
    return reply_class::other;
}

struct RecvResult {
    sockaddr_storage from;
    ParsedIcmp msg;
};

// Block up to `timeout_ms` for the next ICMP packet. When match_id != 0, packets
// whose id doesn't match are skipped (ping mode — ignores foreign echoes so the
// caller isn't confused by unrelated ICMP on the host). match_id == 0 accepts
// any ICMP (traceroute mode — receives time-exceeded for its UDP probes).
// ETIMEDOUT on timeout.
[[nodiscard]] inline auto recv_icmp(int fd, std::uint16_t match_id, int timeout_ms)
    -> base::Result<RecvResult> {
    auto deadline = now_us() + static_cast<long long>(timeout_ms) * 1000;
    for (;;) {
        auto remaining_us = deadline - now_us();
        if (remaining_us <= 0)
            return std::unexpected(base::Error{ETIMEDOUT, "request timed out"});
        pollfd pfd{};
        pfd.fd = fd;
        pfd.events = POLLIN;
        int pr = ::poll(&pfd, 1, static_cast<int>(remaining_us / 1000));
        if (pr < 0) {
            if (errno == EINTR)
                continue;
            return std::unexpected(base::Error{errno, "poll() failed"});
        }
        if (pr == 0)
            return std::unexpected(base::Error{ETIMEDOUT, "request timed out"});
        std::uint8_t buf[1500];
        sockaddr_storage from{};
        socklen_t fromlen = sizeof(from);
        ssize_t n = ::recvfrom(fd, buf, sizeof(buf), 0, reinterpret_cast<sockaddr*>(&from), &fromlen);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return std::unexpected(base::Error{errno, "recvfrom() failed"});
        }
        auto msg = parse_icmp(std::span<const std::uint8_t>{buf, static_cast<std::size_t>(n)});
        if (!msg)
            continue; // malformed
        if (match_id != 0 && msg->id != match_id)
            continue; // foreign echo
        return RecvResult{from, *msg};
    }
}

} // namespace cfbox::icmp
