#include <charconv>
#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <string_view>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/error.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name     = "tail",
    .version  = CFBOX_VERSION_STRING,
    .one_line = "output the last part of files",
    .usage    = "tail [OPTIONS] [FILE]...",
    .options  = "  -n N   output the last N lines (default 10)\n"
                "  -c N   output the last N bytes\n"
                "  -f     follow appended data on the descriptor\n"
                "  -F     follow and retry on rotation/truncation\n"
                "  -s SEC follow poll interval in seconds (default 1)",
    .extra    = "",
};

// ===== static (non-follow) tail =====================================

auto tail_lines(const std::vector<std::string>& lines, long n, bool from_start, bool trailing_nl) -> void {
    auto emit = [&](long i) {
        // Only the file's actual last line can lack a terminator; preserve those
        // bytes instead of synthesizing a newline (coreutils/tail behavior).
        if (i == static_cast<long>(lines.size()) - 1 && !trailing_nl) {
            std::fputs(lines[static_cast<std::size_t>(i)].c_str(), stdout);
        } else {
            std::printf("%s\n", lines[static_cast<std::size_t>(i)].c_str());
        }
    };
    if (from_start) {
        long start = n - 1;
        if (start < 0) start = 0;
        for (long i = start; i < static_cast<long>(lines.size()); ++i) emit(i);
    } else {
        if (n <= 0) return;
        long start = static_cast<long>(lines.size()) - n;
        if (start < 0) start = 0;
        for (long i = start; i < static_cast<long>(lines.size()); ++i) emit(i);
    }
}

auto tail_bytes(const std::string& content, long n) -> void {
    if (n <= 0) return;
    long start = static_cast<long>(content.size()) - n;
    if (start < 0) start = 0;
    std::fwrite(content.data() + start, 1, static_cast<std::size_t>(n), stdout);
}

auto tail_file(std::string_view path, long n, bool use_bytes,
               bool from_start, bool show_header) -> int {
    bool use_stdin = (path == "-");

    auto result = use_stdin ? cfbox::io::read_all_stdin() : cfbox::io::read_all(path);
    if (!result) {
        CFBOX_ERR("tail", "%s", result.error().msg.c_str());
        return 1;
    }

    const auto& content = result.value();

    if (show_header) {
        std::printf("==> %s <==\n", use_stdin ? "standard input" : std::string{path}.c_str());
    }

    if (use_bytes) {
        tail_bytes(content, n);
    } else {
        auto lines = cfbox::io::split_lines(content);
        bool trailing_nl = !content.empty() && content.back() == '\n';
        tail_lines(lines, n, from_start, trailing_nl);
    }
    return 0;
}

auto parse_count(std::string_view val, bool& from_start) -> long {
    if (!val.empty() && val[0] == '+') {
        from_start = true;
        return std::strtol(std::string{val.substr(1)}.c_str(), nullptr, 10);
    }
    return std::strtol(std::string{val}.c_str(), nullptr, 10);
}

// ===== follow (-f / -F) =============================================
//
// Model (per external design review, BusyBox-aligned): the open descriptor is
// the source of truth and its offset is the consumption position. Each sweep
// fstat()s the descriptor to detect in-place truncation, then read()s to EOF.
// Only -F inspects the pathname (to detect rotation by dev/ino). inotify is
// intentionally absent: it is Linux-only, adds a second state machine, and
// polling at a 1s default is the BusyBox-class tradeoff. It can later be added
// purely as a wake accelerator behind a build switch.

constexpr long kFollowQuantum = 64 * 1024;  // max bytes per file per sweep (fairness)

volatile sig_atomic_t g_stop = 0;

extern "C" void on_stop_signal(int) noexcept {
    g_stop = 1;
}

// Installs SIGINT/SIGTERM handlers, restores the previous actions on
// destruction. CFBox is a multi-call binary, so a tail run must not leave
// process-global signal state mutated for any later applet invocation.
class signal_guard {
public:
    signal_guard() noexcept {
        struct sigaction sa{};
        sa.sa_handler = on_stop_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;  // no SA_RESTART: surface EINTR to blocking calls
        installed_ = ::sigaction(SIGINT, &sa, &old_int_) == 0
                     && ::sigaction(SIGTERM, &sa, &old_term_) == 0;
    }
    ~signal_guard() noexcept {
        if (installed_) {
            ::sigaction(SIGINT, &old_int_, nullptr);
            ::sigaction(SIGTERM, &old_term_, nullptr);
        }
        g_stop = 0;
    }
    signal_guard(const signal_guard&) = delete;
    auto operator=(const signal_guard&) = delete;
    [[nodiscard]] auto installed() const noexcept -> bool { return installed_; }

private:
    struct sigaction old_int_{};
    struct sigaction old_term_{};
    bool installed_{false};
};

struct FollowFile {
    std::string path;
    cfbox::io::unique_fd fd;
    cfbox::io::unique_fd pending;  // -F: opened replacement, swapped in once old drains
    dev_t dev{};
    ino_t ino{};
    off_t offset{};
    bool regular{};
    bool missing{};  // -F: path currently absent
    bool reported{};  // dedup: state-change messages announced at most once
};

auto open_follow(const std::string& path, FollowFile& ff) -> bool {
    int fd = ::open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        return false;
    }
    ff.fd.reset(fd);
    ff.dev = st.st_dev;
    ff.ino = st.st_ino;
    ff.regular = S_ISREG(st.st_mode);
    ff.reported = false;
    return true;
}

// Read the whole descriptor from offset 0 into a string (initial tail). Used on
// the follow descriptor itself so the initial tail and the follow phase share
// one open file — no reopen race that could drop bytes written in between.
auto read_fd_all(int fd) -> std::string {
    ::lseek(fd, 0, SEEK_SET);  // no-op (returns -1) on non-seekable streams
    std::string content;
    char buf[4096];
    for (;;) {
        ssize_t n = ::read(fd, buf, sizeof(buf));
        if (n <= 0) break;
        content.append(buf, static_cast<std::size_t>(n));
    }
    return content;
}

// Sleep for one poll interval. nanosleep returns EINTR on signal; we bail once
// g_stop is set. Known race: a signal landing between the g_stop check and
// nanosleep can delay exit by one interval. ppoll closes the window but is
// deferred — acceptable for 1s-class follow.
auto follow_sleep(double seconds) -> void {
    if (seconds <= 0) return;
    struct timespec ts{};
    ts.tv_sec = static_cast<time_t>(seconds);
    ts.tv_nsec = static_cast<long>((seconds - static_cast<double>(ts.tv_sec)) * 1e9);
    while (::nanosleep(&ts, &ts) == -1 && errno == EINTR) {
        if (g_stop) return;
    }
}

enum class PumpResult { Idle, Backlog };

// Read up to one quantum from ff, emitting a multi-file header on source
// change. Detects in-place truncation (same inode, size shrank below offset)
// and rewinds. On EOF with a pending replacement, takes it over.
auto pump_one(FollowFile& ff, std::string& last_printed, bool print_headers)
    -> PumpResult {
    if (ff.regular) {
        struct stat st{};
        if (::fstat(ff.fd.get(), &st) == 0 && st.st_size < ff.offset) {
            ::lseek(ff.fd.get(), 0, SEEK_SET);
            ff.offset = 0;
        }
    }

    char buf[kFollowQuantum];
    long got = 0;
    while (got < kFollowQuantum) {
        if (g_stop) return PumpResult::Idle;
        ssize_t n = ::read(ff.fd.get(), buf + got,
                           static_cast<std::size_t>(kFollowQuantum - got));
        if (n > 0) {
            got += n;
            ff.offset += n;
            continue;
        }
        if (n == 0) break;  // EOF
        if (errno == EINTR) continue;
        if (!ff.reported) {
            CFBOX_ERR("tail", "read error on '%s': %s", ff.path.c_str(), std::strerror(errno));
            ff.reported = true;
        }
        ff.fd.reset();
        break;
    }

    if (got > 0) {
        if (print_headers && last_printed != ff.path) {
            std::printf("==> %s <==\n", ff.path.c_str());
            last_printed = ff.path;
        }
        std::fwrite(buf, 1, static_cast<std::size_t>(got), stdout);
        std::fflush(stdout);
        return got >= kFollowQuantum ? PumpResult::Backlog : PumpResult::Idle;
    }

    // nothing new this sweep: hand off to a pending replacement if -F opened one
    if (ff.pending) {
        ff.fd = std::move(ff.pending);
        struct stat st{};
        if (::fstat(ff.fd.get(), &st) == 0) {
            ff.dev = st.st_dev;
            ff.ino = st.st_ino;
            ff.regular = S_ISREG(st.st_mode);
        }
        ff.offset = 0;
        return PumpResult::Backlog;  // pump the new descriptor immediately
    }
    return PumpResult::Idle;
}

// -F: compare the pathname's identity to the open descriptor. On replace, open
// a pending descriptor (the old one drains first, then pump_one swaps it in).
// On missing, keep the old fd — an unlinked file may still be written — and
// retry the path each interval.
auto check_rotation(FollowFile& ff) -> void {
    struct stat path_st{};
    if (::stat(ff.path.c_str(), &path_st) != 0) {
        if (!ff.missing && !ff.reported) {
            CFBOX_ERR("tail", "%s: %s", ff.path.c_str(), std::strerror(errno));
            ff.reported = true;
        }
        ff.missing = true;
        return;
    }
    ff.missing = false;
    ff.reported = false;

    if (!ff.fd) {
        open_follow(ff.path, ff);  // reappeared: fresh open, read from start
        return;
    }
    if (!ff.pending && (path_st.st_dev != ff.dev || path_st.st_ino != ff.ino)) {
        int nfd = ::open(ff.path.c_str(), O_RDONLY | O_CLOEXEC);
        if (nfd >= 0) ff.pending.reset(nfd);
    }
}

auto follow_files(std::vector<std::string> files, long n, bool use_bytes,
                  bool from_start, double interval, bool retry) -> int {
    signal_guard guard;
    if (!guard.installed()) {
        CFBOX_ERR("tail", "cannot install signal handlers");
        return 1;
    }

    bool multi = files.size() > 1;
    std::vector<FollowFile> ffs;
    ffs.reserve(files.size());
    std::string last_printed;
    int rc = 0;

    for (auto& path : files) {
        FollowFile ff;
        ff.path = path;
        if (open_follow(path, ff)) {
            auto content = read_fd_all(ff.fd.get());  // initial tail on same fd
            if (multi && last_printed != path) {
                std::printf("==> %s <==\n", path.c_str());
                last_printed = path;
            }
            if (use_bytes) {
                tail_bytes(content, n);
            } else {
                // Follow streams appended bytes raw after this initial tail, so keep
                // the historical always-newline behavior here; the trailing-newline
                // fix only applies to the static tail_file path (what we test/diff).
                tail_lines(cfbox::io::split_lines(content), n, from_start, true);
            }
            std::fflush(stdout);
            ff.offset = ff.regular ? ::lseek(ff.fd.get(), 0, SEEK_END) : 0;
        } else if (retry) {
            ff.missing = true;
        } else {
            CFBOX_ERR("tail", "cannot open '%s' for reading", path.c_str());
            rc = 1;
        }
        ffs.push_back(std::move(ff));
    }

    while (!g_stop) {
        bool backlog = false;
        for (auto& ff : ffs) {
            if (retry) check_rotation(ff);
            if (!ff.fd) continue;
            if (pump_one(ff, last_printed, multi) == PumpResult::Backlog) backlog = true;
        }
        std::fflush(stdout);
        if (g_stop) break;
        if (backlog) continue;
        follow_sleep(interval);
    }
    std::fflush(stdout);
    return rc;
}

auto parse_interval(std::string_view val) -> double {
    long v = 0;
    auto res = std::from_chars(val.data(), val.data() + val.size(), v);
    if (res.ec != std::errc{} || res.ptr != val.data() + val.size() || v < 0) {
        return -1.0;  // sentinel: invalid
    }
    return static_cast<double>(v);
}

} // namespace

auto tail_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'n', true},
        cfbox::args::OptSpec{'c', true},
        cfbox::args::OptSpec{'f', false},
        cfbox::args::OptSpec{'F', false},
        cfbox::args::OptSpec{'s', true, "sleep-interval"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool use_lines = true;
    bool use_bytes = false;
    bool from_start = false;
    long n = 10;

    if (parsed.has('n')) {
        n = parse_count(parsed.get('n').value_or("10"), from_start);
    }
    if (parsed.has('c')) {
        n = parse_count(parsed.get('c').value_or("0"), from_start);
        use_bytes = true;
        use_lines = false;
    }

    bool follow = parsed.has('f') || parsed.has('F');
    bool retry = parsed.has('F');

    double interval = 1.0;
    if (follow && parsed.has('s')) {
        interval = parse_interval(parsed.get('s').value_or("1"));
        if (interval < 0) {
            CFBOX_ERR("tail", "invalid sleep interval");
            return 1;
        }
    }

    const auto& pos = parsed.positional();

    // Check for +N positional arg
    std::vector<std::string_view> files;
    for (const auto& p : pos) {
        if (!p.empty() && p[0] == '+' && use_lines && !parsed.has('n')) {
            from_start = true;
            n = std::strtol(std::string{p.substr(1)}.c_str(), nullptr, 10);
        } else {
            files.push_back(p);
        }
    }

    if (follow) {
        if (files.empty()) {
            CFBOX_ERR("tail", "cannot follow standard input");
            return 1;
        }
        std::vector<std::string> ffiles;
        ffiles.reserve(files.size());
        for (auto sv : files) ffiles.emplace_back(sv);
        return follow_files(std::move(ffiles), n, use_bytes, from_start, interval, retry);
    }

    bool multi = files.size() > 1;

    if (files.empty()) {
        return tail_file("-", n, use_bytes, from_start, false);
    }

    int rc = 0;
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (tail_file(files[i], n, use_bytes, from_start, multi) != 0) rc = 1;
    }
    return rc;
}
