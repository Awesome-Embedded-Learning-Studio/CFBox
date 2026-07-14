#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

#include <fcntl.h>
#include <unistd.h>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "dd",
    .version = CFBOX_VERSION_STRING,
    .one_line = "convert and copy a file",
    .usage   = "dd [OPERAND...]",
    .options = "  bs=BYTES     read/write BYTES at a time (sets ibs and obs)\n"
               "  ibs=BYTES    read BYTES at a time (default 512)\n"
               "  obs=BYTES    write BYTES at a time (default 512)\n"
               "  count=N      copy only N input blocks\n"
               "  if=FILE      read from FILE (default stdin)\n"
               "  of=FILE      write to FILE (default stdout)\n"
               "  skip=N       skip N ibs-sized blocks at start of input\n"
               "  seek=N       skip N obs-sized blocks at start of output\n"
               "  conv=CONVS   comma-separated: notrunc,noerror,sync,fsync,fdatasync\n"
               "  status=LV    none | noxfer | progress",
    .extra   = "BYTES suffix: c=1 w=2 b=512 K=1024 M=1048576 G T (KiB/MiB/GiB also)",
};

// Parse a byte-count / numeric operand with optional suffix. Returns false on
// malformed input; never throws.
auto parse_num(std::string_view s, std::uint64_t& out) -> bool {
    if (s.empty()) return false;
    std::uint64_t n = 0;
    std::size_t i = 0;
    while (i < s.size() && s[i] >= '0' && s[i] <= '9') {
        n = n * 10 + static_cast<std::uint64_t>(s[i] - '0');
        ++i;
    }
    if (i == 0) return false; // must start with a digit
    std::uint64_t mult = 1;
    if (i < s.size()) {
        std::string_view suf = s.substr(i);
        if (suf == "c")               mult = 1;
        else if (suf == "w")          mult = 2;
        else if (suf == "b")          mult = 512;
        else if (suf == "K" || suf == "KiB" || suf == "k") mult = 1024;
        else if (suf == "M" || suf == "MiB") mult = 1024ull * 1024;
        else if (suf == "G" || suf == "GiB") mult = 1024ull * 1024 * 1024;
        else if (suf == "T" || suf == "TiB") mult = 1024ull * 1024 * 1024 * 1024;
        else return false;
    }
    out = n * mult;
    return true;
}

auto to_lower(std::string_view s) -> std::string {
    std::string out(s);
    for (auto& c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return out;
}

struct DdOpts {
    std::uint64_t ibs = 512;
    std::uint64_t obs = 512;
    std::uint64_t count = 0;        // 0 = unlimited
    std::uint64_t skip = 0;
    std::uint64_t seek = 0;
    std::string if_path;
    std::string of_path;
    bool has_if = false;
    bool has_of = false;
    bool conv_notrunc = false;
    bool conv_noerror = false;
    bool conv_sync = false;
    bool conv_fsync = false;
    bool conv_fdatasync = false;
    enum Status { kDefault, kNone, kNoxfer };
    Status status = kDefault;
};

auto parse_conv(DdOpts& o, std::string_view csv) -> bool {
    std::size_t start = 0;
    while (start <= csv.size()) {
        auto comma = csv.find(',', start);
        auto end = (comma == std::string_view::npos) ? csv.size() : comma;
        std::string token = to_lower(csv.substr(start, end - start));
        if (token == "notrunc")         o.conv_notrunc = true;
        else if (token == "noerror")    o.conv_noerror = true;
        else if (token == "sync")       o.conv_sync = true;
        else if (token == "fsync")      o.conv_fsync = true;
        else if (token == "fdatasync")  o.conv_fdatasync = true;
        else return false;
        if (comma == std::string_view::npos) break;
        start = comma + 1;
    }
    return true;
}

// Write exactly n bytes; loop over short writes. Returns false on hard error.
auto write_full(int fd, const unsigned char* buf, std::size_t n) -> bool {
    std::size_t done = 0;
    while (done < n) {
        ssize_t w = ::write(fd, buf + done, n - done);
        if (w < 0) {
            if (errno == EINTR) continue;
            return false;
        }
        done += static_cast<std::size_t>(w);
    }
    return true;
}

} // namespace

auto dd_main(int argc, char* argv[]) -> int {
    DdOpts o;

    for (int i = 1; i < argc; ++i) {
        std::string_view arg(argv[i]);
        if (arg == "--help")    { cfbox::help::print_help(HELP); return 0; }
        if (arg == "--version") { cfbox::help::print_version(HELP); return 0; }

        auto eq = arg.find('=');
        if (eq == std::string_view::npos) {
            CFBOX_ERR("dd", "unrecognized operand '%s'", std::string(arg).c_str());
            return 1;
        }
        std::string_view key = arg.substr(0, eq);
        std::string_view val = arg.substr(eq + 1);
        std::uint64_t n = 0;

        if (key == "bs") {
            if (!parse_num(val, n) || n == 0) { CFBOX_ERR("dd", "invalid bs: %s", std::string(val).c_str()); return 1; }
            o.ibs = o.obs = n;
        } else if (key == "ibs") {
            if (!parse_num(val, n) || n == 0) { CFBOX_ERR("dd", "invalid ibs: %s", std::string(val).c_str()); return 1; }
            o.ibs = n;
        } else if (key == "obs") {
            if (!parse_num(val, n) || n == 0) { CFBOX_ERR("dd", "invalid obs: %s", std::string(val).c_str()); return 1; }
            o.obs = n;
        } else if (key == "count" || key == "skip" || key == "seek") {
            if (!parse_num(val, n)) { CFBOX_ERR("dd", "invalid %.*s: %s", static_cast<int>(key.size()), key.data(), std::string(val).c_str()); return 1; }
            if (key == "count") o.count = n;
            else if (key == "skip") o.skip = n;
            else o.seek = n;
        } else if (key == "if") {
            o.if_path = std::string(val); o.has_if = true;
        } else if (key == "of") {
            o.of_path = std::string(val); o.has_of = true;
        } else if (key == "conv") {
            if (!parse_conv(o, val)) { CFBOX_ERR("dd", "invalid conv: %s", std::string(val).c_str()); return 1; }
        } else if (key == "status") {
            std::string s = to_lower(val);
            if (s == "none")            o.status = DdOpts::kNone;
            else if (s == "noxfer")     o.status = DdOpts::kNoxfer;
            else if (s == "progress")   o.status = DdOpts::kDefault;
            else { CFBOX_ERR("dd", "invalid status: %s", std::string(val).c_str()); return 1; }
        } else {
            CFBOX_ERR("dd", "unrecognized operand '%s'", std::string(arg).c_str());
            return 1;
        }
    }

    // open input (default stdin)
    int ifd = STDIN_FILENO;
    cfbox::io::unique_fd ifd_guard;
    if (o.has_if) {
        int fd = ::open(o.if_path.c_str(), O_RDONLY);
        if (fd < 0) { CFBOX_ERR("dd", "cannot open '%s': %s", o.if_path.c_str(), std::strerror(errno)); return 1; }
        ifd = fd; ifd_guard.reset(fd);
    }

    // open output (default stdout). notrunc keeps existing content; otherwise O_TRUNC.
    int ofd = STDOUT_FILENO;
    cfbox::io::unique_fd ofd_guard;
    if (o.has_of) {
        int flags = O_WRONLY | O_CREAT | (o.conv_notrunc ? 0 : O_TRUNC);
        int fd = ::open(o.of_path.c_str(), flags, 0666);
        if (fd < 0) { CFBOX_ERR("dd", "cannot open '%s': %s", o.of_path.c_str(), std::strerror(errno)); return 1; }
        ofd = fd; ofd_guard.reset(fd);
    }

    // skip input blocks (lseek if seekable, else drain by reading)
    std::vector<unsigned char> skip_buf(o.ibs);
    if (o.skip > 0 && ::lseek(ifd, static_cast<off_t>(o.skip * o.ibs), SEEK_SET) < 0) {
        for (std::uint64_t k = 0; k < o.skip; ++k) {
            std::size_t got = 0;
            while (got < o.ibs) {
                ssize_t r = ::read(ifd, skip_buf.data() + got, o.ibs - got);
                if (r == 0) break; // EOF
                if (r < 0) {
                    if (errno == EINTR) continue;
                    if (o.conv_noerror) break;
                    CFBOX_ERR("dd", "read error (skip): %s", std::strerror(errno)); return 1;
                }
                got += static_cast<std::size_t>(r);
            }
        }
    }

    // seek output blocks (only meaningful for seekable of)
    if (o.seek > 0) {
        ::lseek(ofd, static_cast<off_t>(o.seek * o.obs), SEEK_SET);
    }

    // main copy loop. Input is read in ibs blocks; output is assembled into obs
    // blocks (reblocking when ibs != obs). conv=sync zero-pads short reads to ibs.
    std::vector<unsigned char> in_buf(o.ibs);
    std::vector<unsigned char> out_buf(o.obs);
    std::size_t out_len = 0;
    std::uint64_t rec_in_full = 0, rec_in_partial = 0;
    std::uint64_t rec_out_full = 0, rec_out_partial = 0;
    std::uint64_t total_bytes = 0;
    bool eof = false;

    auto flush_out = [&]() {
        if (out_len > 0) {
            if (!write_full(ofd, out_buf.data(), out_len)) {
                CFBOX_ERR("dd", "write error: %s", std::strerror(errno));
                return false;
            }
            ++rec_out_partial;
            total_bytes += out_len;
            out_len = 0;
        }
        return true;
    };

    while (!eof && (o.count == 0 || rec_in_full + rec_in_partial < o.count)) {
        ssize_t r = ::read(ifd, in_buf.data(), o.ibs);
        if (r < 0) {
            if (errno == EINTR) continue;
            if (o.conv_noerror) { ++rec_in_partial; continue; }
            CFBOX_ERR("dd", "read error: %s", std::strerror(errno)); return 1;
        }
        if (r == 0) { eof = true; break; }
        std::size_t got = static_cast<std::size_t>(r);
        if (got == o.ibs) ++rec_in_full; else ++rec_in_partial;
        if (o.conv_sync) std::memset(in_buf.data() + got, 0, o.ibs - got);

        // reblock got bytes (padded to ibs if sync) into obs-sized output
        std::size_t in_off = 0;
        std::size_t avail = o.conv_sync ? o.ibs : got;
        while (in_off < avail) {
            std::size_t space = o.obs - out_len;
            std::size_t chunk = (space < avail - in_off) ? space : (avail - in_off);
            std::memcpy(out_buf.data() + out_len, in_buf.data() + in_off, chunk);
            out_len += chunk;
            in_off += chunk;
            if (out_len == o.obs) {
                if (!write_full(ofd, out_buf.data(), o.obs)) {
                    CFBOX_ERR("dd", "write error: %s", std::strerror(errno)); return 1;
                }
                ++rec_out_full;
                total_bytes += o.obs;
                out_len = 0;
            }
        }
    }
    if (!flush_out()) return 1;

    if (o.conv_fdatasync) ::fdatasync(ofd);
    if (o.conv_fsync)     ::fsync(ofd);

    // transfer statistics to stderr (unless status=none)
    if (o.status != DdOpts::kNone) {
        std::fflush(nullptr);
        std::fprintf(stderr, "%llu+%llu records in\n",
                     static_cast<unsigned long long>(rec_in_full),
                     static_cast<unsigned long long>(rec_in_partial));
        std::fprintf(stderr, "%llu+%llu records out\n",
                     static_cast<unsigned long long>(rec_out_full),
                     static_cast<unsigned long long>(rec_out_partial));
        if (o.status != DdOpts::kNoxfer) {
            std::fprintf(stderr, "%llu bytes transferred\n",
                         static_cast<unsigned long long>(total_bytes));
        }
    }
    return 0;
}
