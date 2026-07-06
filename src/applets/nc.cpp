// nc.cpp — netcat: TCP connect/listen and bidirectional relay.
//
// Two modes (BusyBox nc subset):
//   nc [-s ADDR] -l -p PORT          listen, accept one connection, relay
//   nc [-s ADDR] HOST PORT           connect, relay stdin/stdout<->socket
//
// Relay bridges stdin→socket and socket→stdout (two different fds — a common
// mistake is to treat one fd as both read and write; that writes reply data into
// fd 0). Single-process poll(2), no fork, so cfbox process state is not polluted
// (relevant when nc is one applet of many in a single binary). On stdin EOF we
// half-close the socket (SHUT_WR) so a request/response peer can still answer,
// matching traditional netcat semantics.

#include <poll.h>
#include <unistd.h>

#include <cstddef>
#include <cstdio>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/socket.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "nc",
    .version = CFBOX_VERSION_STRING,
    .one_line = "TCP connect/listen and relay (netcat)",
    .usage = "nc [-l] [-p PORT] [-s ADDR] [HOST] [PORT]",
    .options = "  -l            listen mode (accept one connection)\n"
               "  -p PORT       port to listen on / connect to\n"
               "  -s ADDR       bind address (default 0.0.0.0)",
    .extra = "",
};

// Write exactly n bytes; returns false if the write end closes mid-way.
auto write_all(int fd, const char* buf, std::size_t n) -> bool {
    std::size_t off = 0;
    while (off < n) {
        ssize_t w = ::write(fd, buf + off, n - off);
        if (w <= 0)
            return false;
        off += static_cast<std::size_t>(w);
    }
    return true;
}

// Bridge stdin(0)→sock and sock→stdout(1) via poll. Exits when both directions
// close. stdin EOF triggers SHUT_WR on the socket (half-close) so a peer can
// still send its reply.
auto relay(int sock_fd) -> void {
    pollfd fds[2] = {{STDIN_FILENO, POLLIN, 0}, {sock_fd, POLLIN, 0}};
    bool in_eof = false;
    bool sock_eof = false;
    char buf[4096];

    while (!in_eof || !sock_eof) {
        if (::poll(fds, 2, -1) <= 0)
            break;

        if (!in_eof && fds[0].revents) {
            ssize_t n = ::read(STDIN_FILENO, buf, sizeof(buf));
            if (n <= 0) {
                in_eof = true;
                fds[0].fd = -1;
                ::shutdown(sock_fd, SHUT_WR);
            } else {
                if (!write_all(sock_fd, buf, static_cast<std::size_t>(n)))
                    sock_eof = true;
            }
        }
        if (!sock_eof && fds[1].revents) {
            ssize_t n = ::read(sock_fd, buf, sizeof(buf));
            if (n <= 0) {
                sock_eof = true;
                fds[1].fd = -1;
            } else {
                if (!write_all(STDOUT_FILENO, buf, static_cast<std::size_t>(n)))
                    in_eof = true;
            }
        }
    }
}

} // namespace

auto nc_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    auto parsed = args::parse(argc, argv,
                              {
                                  args::OptSpec{'l', false, "listen"},
                                  args::OptSpec{'p', true, "port"},
                                  args::OptSpec{'s', true, "source"},
                              });

    if (parsed.has_long("help")) {
        help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        help::print_version(HELP);
        return 0;
    }

    std::string bind_addr = "0.0.0.0";
    if (auto s = parsed.get('s'))
        bind_addr = std::string{*s};

    if (parsed.has('l')) {
        // Port from -p, else first positional (nc -l PORT).
        std::string_view port_str;
        if (auto v = parsed.get('p'))
            port_str = *v;
        else if (!parsed.positional().empty())
            port_str = parsed.positional()[0];
        else {
            CFBOX_ERR("nc", "listen mode needs -p PORT");
            return 2;
        }
        auto pi = args::parse_int(port_str);
        if (!pi) {
            CFBOX_ERR("nc", "invalid port: %s", pi.error().msg.c_str());
            return 2;
        }
        if (*pi < 0 || *pi > 65535) {
            CFBOX_ERR("nc", "port out of range: %d", *pi);
            return 2;
        }
        auto srv = socket::listen_on(bind_addr, static_cast<uint16_t>(*pi));
        if (!srv) {
            CFBOX_ERR("nc", "listen failed: %s", srv.error().msg.c_str());
            return 1;
        }
        auto conn = socket::accept_one(srv->get());
        if (!conn) {
            CFBOX_ERR("nc", "accept failed: %s", conn.error().msg.c_str());
            return 1;
        }
        relay(conn->get());
        return 0;
    }

    // connect mode: HOST PORT
    auto& pos = parsed.positional();
    if (pos.size() < 2) {
        CFBOX_ERR("nc", "usage: nc [-l] [-p PORT] HOST PORT");
        return 2;
    }
    auto port_i = args::parse_int(pos[1]);
    if (!port_i) {
        CFBOX_ERR("nc", "invalid port: %s", port_i.error().msg.c_str());
        return 2;
    }
    if (*port_i < 0 || *port_i > 65535) {
        CFBOX_ERR("nc", "port out of range: %d", *port_i);
        return 2;
    }
    auto conn = socket::dial(pos[0], static_cast<uint16_t>(*port_i));
    if (!conn) {
        CFBOX_ERR("nc", "connect failed: %s", conn.error().msg.c_str());
        return 1;
    }
    relay(conn->get());
    return 0;
}
