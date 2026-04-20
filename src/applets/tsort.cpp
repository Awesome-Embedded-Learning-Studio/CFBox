#include <cstdio>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "tsort",
    .version = CFBOX_VERSION_STRING,
    .one_line = "perform topological sort",
    .usage   = "tsort [FILE]",
    .options = "",
    .extra   = "Input is pairs of items (whitespace-separated). Output is topologically sorted.",
};
} // namespace

auto tsort_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    const auto& pos = parsed.positional();
    auto path = pos.empty() ? std::string_view{"-"} : pos[0];

    std::map<std::string, std::set<std::string>> graph;
    std::map<std::string, int> in_degree;

    auto result = cfbox::stream::for_each_line(path, [&](const std::string& line, std::size_t) {
        auto fields = cfbox::stream::split_whitespace(line);
        for (std::size_t i = 0; i + 1 < fields.size(); i += 2) {
            auto& a = fields[i];
            auto& b = fields[i + 1];
            if (graph[a].insert(b).second) {
                ++in_degree[b];
            }
            in_degree.try_emplace(a, 0);
        }
        return true;
    });
    if (!result) {
        std::fprintf(stderr, "cfbox tsort: %s\n", result.error().msg.c_str());
        return 1;
    }

    // Kahn's algorithm
    std::queue<std::string> q;
    for (const auto& [node, deg] : in_degree) {
        if (deg == 0) q.push(node);
    }

    while (!q.empty()) {
        auto node = q.front();
        q.pop();
        std::puts(node.c_str());
        if (graph.count(node)) {
            for (const auto& neighbor : graph[node]) {
                if (--in_degree[neighbor] == 0) {
                    q.push(neighbor);
                }
            }
        }
    }
    return 0;
}
