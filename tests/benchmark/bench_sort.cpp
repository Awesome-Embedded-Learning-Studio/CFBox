// Micro-benchmark harness — see document/ai/PERFORMANCE.md.
// Build: cmake -B build-bench -DCMAKE_BUILD_TYPE=Release -DCFBOX_ENABLE_BENCHMARK=ON
//        cmake --build build-bench -j$(nproc)
// Run:   ./build-bench/cfbox_bench
//
// This first bench just validates the harness and gives a sort baseline; later
// batches add benches per hot path (sed regex, md5sum streaming, cmp, ...).

#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>

#include <cfbox/applet_config.hpp>
#include <cfbox/applets.hpp>

#if CFBOX_ENABLE_SORT

// Lazily create one big pseudo-random input (~50k lines) and reuse it across
// iterations, so the benchmark measures sort itself, not fixture generation.
static auto big_sort_file() -> const char* {
    static char path[] = "/tmp/cfbox_bench_sort_XXXXXX";
    static bool made = false;
    if (!made) {
        int fd = mkstemp(path);
        std::FILE* f = (fd >= 0) ? fdopen(fd, "w") : nullptr;
        if (f) {
            for (unsigned i = 0; i < 50000u; ++i) {
                std::fprintf(f, "%u\n", (i * 2654435761u) & 0xffffu);
            }
            std::fclose(f);
        }
        made = true;
    }
    return path;
}

// Benchmarks cfbox sort on the big input. Output is discarded to /dev/null so
// printing is never the bottleneck. Re-reads the file each iteration, which
// matches real sort usage (sort is not a pure in-memory transform).
static void BenchSort(benchmark::State& state) {
    const char* f = big_sort_file();
    // Discard sort's stdout inside the loop (so printing isn't the bottleneck),
    // but save/restore so google-benchmark's own report still reaches stdout.
    int saved_stdout = dup(STDOUT_FILENO);
    int devnull = open("/dev/null", O_WRONLY);
    char arg0[] = "sort";
    char* argv[] = {arg0, const_cast<char*>(f)};
    for (auto _ : state) {
        dup2(devnull, STDOUT_FILENO);
        int rc = sort_main(2, argv);
        benchmark::DoNotOptimize(rc);
    }
    std::fflush(stdout);
    if (saved_stdout >= 0) dup2(saved_stdout, STDOUT_FILENO);
    if (devnull >= 0) close(devnull);
    if (saved_stdout >= 0) close(saved_stdout);
}
BENCHMARK(BenchSort)->Unit(benchmark::kMillisecond)->UseRealTime();

#endif  // CFBOX_ENABLE_SORT
