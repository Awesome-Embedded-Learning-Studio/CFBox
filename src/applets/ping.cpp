// ping.cpp — ICMP echo client (SOCK_RAW IPPROTO_ICMP, needs CAP_NET_RAW).
//
// `ping [OPTIONS] HOST` — sends ICMP echo requests and prints per-reply RTT,
// then a summary on exit (Ctrl-C or -c reached). EPERM (no CAP_NET_RAW) is
// reported clearly and exits 2, distinct from a host-resolve failure (exit 1).
// IPv4 only this cut.

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/icmp.hpp>
#include <cfbox/net_util.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "ping",
    .version = CFBOX_VERSION_STRING,
    .one_line = "send ICMP ECHO_REQUEST packets to a host (needs CAP_NET_RAW)",
    .usage = "ping [-c COUNT] [-i INTERVAL] [-W TIMEOUT] [-s SIZE] [-qn] HOST",
    .options =
        "  -c COUNT    stop after COUNT replies (default: run until Ctrl-C)\n"
        "  -i INTERVAL seconds between requests (default: 1)\n"
        "  -W TIMEOUT  seconds to wait for each reply (default: 1)\n"
        "  -s SIZE     payload bytes (default: 56, max: 1400)\n"
        "  -q          quiet — summary only\n"
        "  -n          numeric output (no reverse DNS)",
    .extra = "",
};

// SIGINT handler + RAII restore (multi-call binary must not leak signal state).
volatile sig_atomic_t g_interrupted = 0;

struct sigint_guard {
    struct sigaction old_act{};
    bool installed = false;
    sigint_guard() {
        struct sigaction sa{};
        sa.sa_handler = [](int) { g_interrupted = 1; };
        ::sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0; // no SA_RESTART — usleep/poll wake up promptly
        installed = (::sigaction(SIGINT, &sa, &old_act) == 0);
    }
    ~sigint_guard() {
        if (installed)
            ::sigaction(SIGINT, &old_act, nullptr);
    }
};

} // namespace

auto ping_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    auto parsed = args::parse(argc, argv,
                              {
                                  args::OptSpec{'c', true, "count"},
                                  args::OptSpec{'i', true, "interval"},
                                  args::OptSpec{'W', true, "timeout"},
                                  args::OptSpec{'s', true, "size"},
                                  args::OptSpec{'q', false, "quiet"},
                                  args::OptSpec{'n', false, "numeric"},
                              });

    if (parsed.has_long("help")) {
        help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        help::print_version(HELP);
        return 0;
    }

    if (parsed.positional().empty()) {
        CFBOX_ERR("ping", "usage: ping [OPTIONS] HOST");
        return 2;
    }
    const std::string host = std::string{parsed.positional()[0]};

    long count = -1; // -1 = infinite
    if (auto v = parsed.get('c')) {
        auto n = args::parse_int(*v);
        if (!n || *n < 1) {
            CFBOX_ERR("ping", "invalid count '%.*s'", static_cast<int>(v->size()), v->data());
            return 2;
        }
        count = *n;
    }
    int interval = 1;
    if (auto v = parsed.get('i')) {
        auto n = args::parse_int(*v);
        if (!n || *n < 0) {
            CFBOX_ERR("ping", "invalid interval '%.*s'", static_cast<int>(v->size()), v->data());
            return 2;
        }
        interval = *n;
    }
    int timeout_s = 1;
    if (auto v = parsed.get('W')) {
        auto n = args::parse_int(*v);
        if (!n || *n < 1) {
            CFBOX_ERR("ping", "invalid timeout '%.*s'", static_cast<int>(v->size()), v->data());
            return 2;
        }
        timeout_s = *n;
    }
    int pktsize = 56;
    if (auto v = parsed.get('s')) {
        auto n = args::parse_int(*v);
        if (!n || *n < 8 || *n > 1400) {
            CFBOX_ERR("ping", "invalid size '%.*s'", static_cast<int>(v->size()), v->data());
            return 2;
        }
        pktsize = *n;
    }
    const bool quiet = parsed.has('q');
    const bool numeric = parsed.has('n');

    sockaddr_storage dst{};
    if (!net::resolve_ipv4(host, dst)) {
        CFBOX_ERR("ping", "cannot resolve '%s'", host.c_str());
        return 1;
    }

    auto raw = icmp::open_raw();
    if (!raw) {
        CFBOX_ERR("ping", "%s", raw.error().msg.c_str());
        return 2;
    }

    const std::string dst_name = net::name_of(dst, numeric);
    sockaddr_in din{};
    std::memcpy(&din, &dst, sizeof(din)); // sockaddr_in memcpy (cast-align safe)
    char addr_buf[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &din.sin_addr, addr_buf, sizeof(addr_buf));

    if (!quiet)
        std::printf("PING %s (%s): %d data bytes\n", dst_name.c_str(), addr_buf, pktsize);

    sigint_guard sig;

    const std::uint16_t id = static_cast<std::uint16_t>(::getpid() & 0xffff);
    long tx = 0, rx = 0;
    long long rtt_min = 0, rtt_max = 0, rtt_sum = 0;

    std::uint8_t out[1500];
    std::uint8_t payload[1400];
    for (int i = 0; i < pktsize; ++i)
        payload[i] = static_cast<std::uint8_t>(i);

    for (long seq = 1; (count < 0 || seq <= count) && !g_interrupted; ++seq) {
        const std::uint16_t sid = static_cast<std::uint16_t>(seq & 0xffff);
        auto n = icmp::build_echo_request(
            id, sid, std::span<const std::uint8_t>{payload, static_cast<std::size_t>(pktsize)},
            std::span<std::uint8_t>{out, sizeof(out)});
        const auto t0 = icmp::now_us();
        if (::sendto(raw->get(), out, n, 0, reinterpret_cast<sockaddr*>(&dst), sizeof(dst)) < 0) {
            CFBOX_ERR("ping", "sendto: %s", std::strerror(errno));
            return 1;
        }
        ++tx;

        auto reply = icmp::recv_icmp(raw->get(), id, timeout_s * 1000);
        if (reply) {
            ++rx;
            const auto rtt = icmp::now_us() - t0;
            rtt_sum += rtt;
            if (rx == 1)
                rtt_min = rtt_max = rtt;
            else {
                if (rtt < rtt_min)
                    rtt_min = rtt;
                if (rtt > rtt_max)
                    rtt_max = rtt;
            }
            if (!quiet)
                std::printf("%zu bytes from %s: icmp_seq=%ld ttl=%d time=%.3f ms\n", n, addr_buf,
                            seq, reply->msg.recv_ttl, static_cast<double>(rtt) / 1000.0);
        } else if (!quiet) {
            if (reply.error().code == ETIMEDOUT)
                std::printf("no response from %s (icmp_seq=%ld)\n", addr_buf, seq);
            else
                CFBOX_ERR("ping", "%s", reply.error().msg.c_str());
        }

        if (count < 0 || seq < count) {
            if (interval > 0)
                ::usleep(static_cast<useconds_t>(interval) * 1000000);
        }
    }

    if (tx > 0) {
        const long loss = (tx - rx) * 100 / tx;
        std::printf("--- %s ping statistics ---\n", dst_name.c_str());
        std::printf("%ld packets transmitted, %ld received, %ld%% packet loss\n", tx, rx, loss);
        if (rx > 0)
            std::printf("rtt min/avg/max = %.3f/%.3f/%.3f ms\n",
                        static_cast<double>(rtt_min) / 1000.0,
                        static_cast<double>(rtt_sum) / static_cast<double>(rx) / 1000.0,
                        static_cast<double>(rtt_max) / 1000.0);
    }
    return rx > 0 ? 0 : 1;
}
