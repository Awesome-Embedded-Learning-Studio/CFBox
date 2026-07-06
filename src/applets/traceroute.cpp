// traceroute.cpp — UDP-probe traceroute with ICMP time-exceeded reception.
//
// `traceroute [OPTIONS] HOST` — sends UDP probes to high ports with increasing
// TTL, receives ICMP time-exceeded (intermediate hops) or port-unreachable
// (destination reached) via a SOCK_RAW ICMP socket. Needs CAP_NET_RAW for the
// ICMP socket; the UDP socket needs no privilege. EPERM exits 2, host-resolve
// failure exits 1. IPv4 only this cut.

#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip_icmp.h>
#include <signal.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/icmp.hpp>
#include <cfbox/net_util.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "traceroute",
    .version = CFBOX_VERSION_STRING,
    .one_line = "trace the route to a host (UDP probes, needs CAP_NET_RAW)",
    .usage = "traceroute [-m MAXTTL] [-q NQUERIES] [-w WAIT] [-n] HOST",
    .options =
        "  -m MAXTTL     max time-to-live (hops), default 30\n"
        "  -q NQUERIES   probes per hop, default 3\n"
        "  -w WAIT       seconds to wait per probe, default 5\n"
        "  -n            numeric output (no reverse DNS)",
    .extra = "",
};

// RAII SIGINT handler (shared shape with ping.cpp — could lift to a common
// signal_guard.hpp if a third applet needs it).
volatile sig_atomic_t g_interrupted = 0;

struct sigint_guard {
    struct sigaction old_act{};
    bool installed = false;
    sigint_guard() {
        struct sigaction sa{};
        sa.sa_handler = [](int) { g_interrupted = 1; };
        ::sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        installed = (::sigaction(SIGINT, &sa, &old_act) == 0);
    }
    ~sigint_guard() {
        if (installed)
            ::sigaction(SIGINT, &old_act, nullptr);
    }
};

} // namespace

auto traceroute_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    auto parsed = args::parse(argc, argv,
                              {
                                  args::OptSpec{'m', true, "maxttl"},
                                  args::OptSpec{'q', true, "nqueries"},
                                  args::OptSpec{'w', true, "wait"},
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
        CFBOX_ERR("traceroute", "usage: traceroute [OPTIONS] HOST");
        return 2;
    }
    const std::string host = std::string{parsed.positional()[0]};

    int maxttl = 30;
    if (auto v = parsed.get('m')) {
        auto n = args::parse_int(*v);
        if (!n || *n < 1 || *n > 64) {
            CFBOX_ERR("traceroute", "invalid maxttl '%.*s'", static_cast<int>(v->size()), v->data());
            return 2;
        }
        maxttl = *n;
    }
    int nqueries = 3;
    if (auto v = parsed.get('q')) {
        auto n = args::parse_int(*v);
        if (!n || *n < 1 || *n > 10) {
            CFBOX_ERR("traceroute", "invalid nqueries '%.*s'", static_cast<int>(v->size()), v->data());
            return 2;
        }
        nqueries = *n;
    }
    int wait_s = 5;
    if (auto v = parsed.get('w')) {
        auto n = args::parse_int(*v);
        if (!n || *n < 1) {
            CFBOX_ERR("traceroute", "invalid wait '%.*s'", static_cast<int>(v->size()), v->data());
            return 2;
        }
        wait_s = *n;
    }
    const bool numeric = parsed.has('n');

    sockaddr_storage dst{};
    if (!net::resolve_ipv4(host, dst)) {
        CFBOX_ERR("traceroute", "cannot resolve '%s'", host.c_str());
        return 1;
    }

    auto raw = icmp::open_raw();
    if (!raw) {
        CFBOX_ERR("traceroute", "%s", raw.error().msg.c_str());
        return 2;
    }
    int udp = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (udp < 0) {
        CFBOX_ERR("traceroute", "socket(UDP): %s", std::strerror(errno));
        return 1;
    }
    io::unique_fd udp_fd{udp};

    sockaddr_in din{};
    std::memcpy(&din, &dst, sizeof(din)); // sockaddr_in via memcpy (cast-align safe)
    char addr_buf[INET_ADDRSTRLEN];
    ::inet_ntop(AF_INET, &din.sin_addr, addr_buf, sizeof(addr_buf));

    std::printf("traceroute to %s (%s), %d hops max, 38 byte packets\n", host.c_str(), addr_buf,
                maxttl);

    sigint_guard sig;
    bool reached = false;

    for (int ttl = 1; ttl <= maxttl && !g_interrupted && !reached; ++ttl) {
        const int t = ttl;
        if (::setsockopt(udp_fd.get(), IPPROTO_IP, IP_TTL, &t, sizeof(t)) < 0) {
            CFBOX_ERR("traceroute", "setsockopt(IP_TTL): %s", std::strerror(errno));
            return 1;
        }

        std::printf("%2d", ttl);
        std::string hop_name;
        bool hop_responded = false;

        for (int q = 0; q < nqueries && !g_interrupted; ++q) {
            // Bump destination port per probe so NAT/conntrack doesn't fold hops together.
            din.sin_port = htons(static_cast<std::uint16_t>(33000 + ttl * 100 + q));
            const char probe = '\0';
            const auto t0 = icmp::now_us();
            if (::sendto(udp_fd.get(), &probe, 1, 0, reinterpret_cast<sockaddr*>(&din), sizeof(din)) < 0) {
                CFBOX_ERR("traceroute", "sendto: %s", std::strerror(errno));
                return 1;
            }
            // match_id=0 — accept any ICMP (time-exceeded has our UDP's inner id, not ours).
            auto reply = icmp::recv_icmp(raw->get(), 0, wait_s * 1000);
            if (reply) {
                if (!hop_responded) {
                    hop_name = net::name_of(reply->from, numeric);
                    hop_responded = true;
                }
                const auto rtt = icmp::now_us() - t0;
                std::printf("  %.3f ms", static_cast<double>(rtt) / 1000.0);
                if (icmp::classify_reply(reply->msg.type, reply->msg.code) == icmp::reply_class::reached)
                    reached = true;
            } else {
                std::printf("  *");
            }
            std::fflush(stdout);
        }

        std::printf("  %s\n", hop_responded ? hop_name.c_str() : "*");
    }

    return reached ? 0 : 1;
}
