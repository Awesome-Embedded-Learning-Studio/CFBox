// Micro-benchmark: io::for_each_line on a large file (the shared line-reader
// used by grep/cut/expand/fold/nl/paste/tac/shuf/tsort). Isolates the reader.
#include <benchmark/benchmark.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>

#include <cfbox/io.hpp>

static auto big_lines_file() -> const char* {
    static char path[] = "/tmp/cfbox_bench_lines_XXXXXX";
    static bool made = false;
    if (!made) {
        int fd = mkstemp(path);
        std::FILE* f = (fd >= 0) ? fdopen(fd, "w") : nullptr;
        if (f) {
            for (unsigned i = 0; i < 200000u; ++i) {  // 200k lines
                std::fprintf(f, "line number %u has some words to read\n", i);
            }
            std::fclose(f);
        }
        made = true;
    }
    return path;
}

static void BenchForEachLine(benchmark::State& state) {
    const char* f = big_lines_file();
    for (auto _ : state) {
        std::FILE* fp = std::fopen(f, "r");
        if (!fp) continue;
        std::size_t count = 0;
        auto result = cfbox::io::for_each_line(fp, [&count](const std::string&) {
            ++count;
            return true;
        });
        std::fclose(fp);
        benchmark::DoNotOptimize(result);
        benchmark::DoNotOptimize(count);
    }
}
BENCHMARK(BenchForEachLine)->Unit(benchmark::kMillisecond)->UseRealTime();
