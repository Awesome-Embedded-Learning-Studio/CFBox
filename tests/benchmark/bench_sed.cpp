// Micro-benchmark: sed 's/foo/QUUX/g' on a big input.
// Targets the per-line regex recompile hot path (see document/ai/PERFORMANCE.md 批1).
#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>

#if CFBOX_ENABLE_SED

static auto big_sed_file() -> const char* {
    static char path[] = "/tmp/cfbox_bench_sed_XXXXXX";
    static bool made = false;
    if (!made) {
        int fd = mkstemp(path);
        std::FILE* f = (fd >= 0) ? fdopen(fd, "w") : nullptr;
        if (f) {
            for (unsigned i = 0; i < 50000u; ++i) {
                std::fprintf(f, "line %u foo bar baz\n", i);
            }
            std::fclose(f);
        }
        made = true;
    }
    return path;
}

// sed 's/foo/QUUX/g' on 50k lines. Before 批1 each line recompiles the regex
// (apply_substitute builds a fresh scoped_regex); after 批1 it is compiled once
// at parse time. Same output either way (verified by test_sed + test_sed.sh).
static void BenchSedSubstitute(benchmark::State& state) {
    const char* f = big_sed_file();
    int saved_stdout = dup(STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    char a0[] = "sed", a1[] = "s/foo/QUUX/g";
    char* argv[] = {a0, a1, const_cast<char*>(f)};
    for (auto _ : state) {
        dup2(devnull, STDOUT_FILENO);
        int rc = sed_main(3, argv);
        benchmark::DoNotOptimize(rc);
    }
    std::fflush(stdout);
    if (saved_stdout >= 0) dup2(saved_stdout, STDOUT_FILENO);
    if (devnull >= 0) close(devnull);
    if (saved_stdout >= 0) close(saved_stdout);
}
BENCHMARK(BenchSedSubstitute)->Unit(benchmark::kMillisecond)->UseRealTime();

#endif  // CFBOX_ENABLE_SED
