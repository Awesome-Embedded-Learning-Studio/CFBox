#pragma once

// socket.hpp — minimal RAII socket primitives shared by the Phase 3 network
// applets (nc / wget / tftp / netstat / ...). Reuses cfbox::io::unique_fd for
// fd lifetime on purpose (DRY: a socket *is* a file descriptor — we do not
// wrap it again in a SocketFd class). Errors flow through cfbox::base::Result;
// no exceptions/RTTI. Plaintext only — TLS is out of scope for Phase 3.

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <cfbox/error.hpp>
#include <cfbox/io.hpp> // unique_fd

namespace cfbox::socket {

enum class Family : int { IPv4 = AF_INET, IPv6 = AF_INET6 };
enum class Kind : int { Stream = SOCK_STREAM, Datagram = SOCK_DGRAM };

// Create an unbound socket of the requested family/kind.
[[nodiscard]] inline auto make(Family fam, Kind kind) -> base::Result<io::unique_fd> {
    int fd = ::socket(static_cast<int>(fam), static_cast<int>(kind), 0);
    if (fd < 0)
        return std::unexpected(base::Error{errno, "socket() failed"});
    return io::unique_fd{fd};
}

// One resolved endpoint. sockaddr_storage holds either v4 or v6.
struct AddrInfo {
    sockaddr_storage addr{};
    socklen_t len = 0;
};

// Resolve host:port via getaddrinfo (numeric or named hosts). Returns every
// candidate so the caller can try them in order until one connects.
[[nodiscard]] inline auto resolve(std::string_view host, uint16_t port, Kind kind)
    -> base::Result<std::vector<AddrInfo>> {
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = static_cast<int>(kind);

    addrinfo* res = nullptr;
    int rc = ::getaddrinfo(std::string{host}.c_str(), std::to_string(port).c_str(), &hints, &res);
    if (rc != 0) {
        return std::unexpected(base::Error{rc, std::string{"getaddrinfo: "} + gai_strerror(rc)});
    }
    std::vector<AddrInfo> out;
    for (addrinfo* p = res; p; p = p->ai_next) {
        AddrInfo info;
        std::memcpy(&info.addr, p->ai_addr, p->ai_addrlen);
        info.len = p->ai_addrlen;
        out.push_back(info);
    }
    ::freeaddrinfo(res);
    if (out.empty())
        return std::unexpected(base::Error{0, "no addresses resolved"});
    return out;
}

[[nodiscard]] inline auto connect_fd(int fd, const sockaddr* addr, socklen_t len)
    -> base::Result<void> {
    if (::connect(fd, addr, len) < 0)
        return std::unexpected(base::Error{errno, "connect() failed"});
    return {};
}

[[nodiscard]] inline auto set_reuseaddr(int fd) -> base::Result<void> {
    int on = 1;
    if (::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
        return std::unexpected(base::Error{errno, "setsockopt(SO_REUSEADDR) failed"});
    return {};
}

// Client convenience: resolve, create a matching socket, connect — trying each
// resolved address until one succeeds (happy-eyeballs-lite).
[[nodiscard]] inline auto dial(std::string_view host, uint16_t port, Kind kind = Kind::Stream)
    -> base::Result<io::unique_fd> {
    CFBOX_TRY(addrs, resolve(host, port, kind));
    for (const auto& a : *addrs) {
        auto fam = (a.addr.ss_family == AF_INET6) ? Family::IPv6 : Family::IPv4;
        auto s = make(fam, kind);
        if (!s)
            continue;
        if (connect_fd(s->get(), reinterpret_cast<const sockaddr*>(&a.addr), a.len))
            return s;
    }
    return std::unexpected(base::Error{ECONNREFUSED, "connection refused"});
}

// Server convenience: socket + SO_REUSEADDR + bind + listen. port=0 lets the OS
// pick; retrieve the chosen port with bound_port(). addr may be "0.0.0.0", "::",
// or a literal v4/v6 address.
[[nodiscard]] inline auto listen_on(std::string_view addr, uint16_t port, Kind kind = Kind::Stream)
    -> base::Result<io::unique_fd> {
    bool v6 = addr.find(':') != std::string_view::npos;
    CFBOX_TRY(fd, make(v6 ? Family::IPv6 : Family::IPv4, kind));
    if (auto r = set_reuseaddr(fd->get()); !r)
        return std::unexpected(r.error());

    if (v6) {
        sockaddr_in6 sa{};
        sa.sin6_family = AF_INET6;
        sa.sin6_port = htons(port);
        if (addr != "::") {
            if (::inet_pton(AF_INET6, std::string{addr}.c_str(), &sa.sin6_addr) != 1)
                return std::unexpected(base::Error{EINVAL, "invalid IPv6 address"});
        }
        if (::bind(fd->get(), reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0)
            return std::unexpected(base::Error{errno, "bind() failed"});
    } else {
        sockaddr_in sa{};
        sa.sin_family = AF_INET;
        sa.sin_port = htons(port);
        if (addr != "0.0.0.0" && !addr.empty()) {
            if (::inet_pton(AF_INET, std::string{addr}.c_str(), &sa.sin_addr) != 1)
                return std::unexpected(base::Error{EINVAL, "invalid IPv4 address"});
        }
        if (::bind(fd->get(), reinterpret_cast<sockaddr*>(&sa), sizeof(sa)) < 0)
            return std::unexpected(base::Error{errno, "bind() failed"});
    }
    if (kind == Kind::Stream) {
        if (::listen(fd->get(), SOMAXCONN) < 0)
            return std::unexpected(base::Error{errno, "listen() failed"});
    }
    return fd;
}

// Read back the OS-assigned port after listen_on(port=0).
[[nodiscard]] inline auto bound_port(int fd) -> base::Result<uint16_t> {
    sockaddr_storage sa{};
    socklen_t len = sizeof(sa);
    if (::getsockname(fd, reinterpret_cast<sockaddr*>(&sa), &len) < 0)
        return std::unexpected(base::Error{errno, "getsockname() failed"});
    if (sa.ss_family == AF_INET6)
        return ntohs(reinterpret_cast<sockaddr_in6*>(&sa)->sin6_port);
    return ntohs(reinterpret_cast<sockaddr_in*>(&sa)->sin_port);
}

// Accept one connection on a listening socket.
[[nodiscard]] inline auto accept_one(int listen_fd) -> base::Result<io::unique_fd> {
    int c = ::accept(listen_fd, nullptr, nullptr);
    if (c < 0)
        return std::unexpected(base::Error{errno, "accept() failed"});
    return io::unique_fd{c};
}

// Format a sockaddr_storage as "host:port" (inet_ntop — never sprintf).
[[nodiscard]] inline auto format_addr(const sockaddr_storage& ss) -> std::string {
    char buf[INET6_ADDRSTRLEN] = {};
    uint16_t port = 0;
    if (ss.ss_family == AF_INET6) {
        ::inet_ntop(AF_INET6, &reinterpret_cast<const sockaddr_in6*>(&ss)->sin6_addr, buf,
                    sizeof(buf));
        port = ntohs(reinterpret_cast<const sockaddr_in6*>(&ss)->sin6_port);
    } else {
        ::inet_ntop(AF_INET, &reinterpret_cast<const sockaddr_in*>(&ss)->sin_addr, buf,
                    sizeof(buf));
        port = ntohs(reinterpret_cast<const sockaddr_in*>(&ss)->sin_port);
    }
    return std::string{buf} + ':' + std::to_string(port);
}

} // namespace cfbox::socket
