// hostname.cpp — show the system hostname. BusyBox-compatible options.
//
// -s  short (cut at first dot)         -i  IPv4 address(es)
// -f  FQDN (canonical via getaddrinfo)  -d  DNS domain (FQDN minus short part)
//
// Setting the hostname (hostname NAME) and sethostname(2) are not in this cut.

#include <arpa/inet.h>
#include <netdb.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name = "hostname",
    .version = CFBOX_VERSION_STRING,
    .one_line = "show or set the system host name",
    .usage = "hostname [-s|-i|-f|-d]",
    .options = "  -s    short hostname (cut at first dot)\n"
               "  -i    IP address(es) for the host name\n"
               "  -f    long host name (FQDN)\n"
               "  -d    DNS domain name",
    .extra = "",
};

auto get_hostname() -> std::string {
    char buf[256] = {};
    if (gethostname(buf, sizeof(buf) - 1) != 0)
        return {};
    return buf;
}

// Resolve name → IPv4 dotted strings. memcpy ai_addr to avoid -Wcast-align.
auto resolve_ipv4(const std::string& name) -> std::vector<std::string> {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(name.c_str(), nullptr, &hints, &res) != 0)
        return {};
    std::vector<std::string> out;
    for (addrinfo* p = res; p; p = p->ai_next) {
        sockaddr_in sa;
        std::memcpy(&sa, p->ai_addr, sizeof(sa));
        char buf[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, &sa.sin_addr, buf, sizeof(buf)))
            out.push_back(buf);
    }
    freeaddrinfo(res);
    return out;
}

// Canonical FQDN via getaddrinfo(AI_CANONNAME); falls back to gethostname().
auto resolve_fqdn(const std::string& name) -> std::string {
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_flags = AI_CANONNAME;
    addrinfo* res = nullptr;
    if (getaddrinfo(name.c_str(), nullptr, &hints, &res) != 0 || !res)
        return name;
    std::string fqdn = res->ai_canonname ? res->ai_canonname : name;
    freeaddrinfo(res);
    return fqdn;
}

} // namespace

auto hostname_main(int argc, char* argv[]) -> int {
    using namespace cfbox;
    auto parsed = args::parse(argc, argv,
                              {
                                  args::OptSpec{'s', false, "short"},
                                  args::OptSpec{'i', false, "ip-address"},
                                  args::OptSpec{'f', false, "fqdn"},
                                  args::OptSpec{'d', false, "domain"},
                              });

    if (parsed.has_long("help")) {
        help::print_help(HELP);
        return 0;
    }
    if (parsed.has_long("version")) {
        help::print_version(HELP);
        return 0;
    }

    std::string name = get_hostname();
    if (name.empty()) {
        CFBOX_ERR("hostname", "cannot get hostname");
        return 1;
    }

    if (parsed.has('i')) {
        auto addrs = resolve_ipv4(name);
        if (addrs.empty()) {
            CFBOX_ERR("hostname", "cannot resolve %s", name.c_str());
            return 1;
        }
        for (std::size_t k = 0; k < addrs.size(); ++k) {
            if (k)
                std::putchar(' ');
            std::fputs(addrs[k].c_str(), stdout);
        }
        std::putchar('\n');
        return 0;
    }
    if (parsed.has('f')) {
        std::puts(resolve_fqdn(name).c_str());
        return 0;
    }
    if (parsed.has('d')) {
        std::string fqdn = resolve_fqdn(name);
        auto dot = fqdn.find('.');
        if (dot != std::string::npos)
            std::puts(fqdn.substr(dot + 1).c_str());
        return 0;
    }

    std::string out = name;
    if (parsed.has('s')) {
        auto dot = out.find('.');
        if (dot != std::string::npos)
            out.erase(dot);
    }
    std::puts(out.c_str());
    return 0;
}
