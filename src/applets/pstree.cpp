#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <algorithm>

#include <cfbox/applet.hpp>
#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/proc.hpp>

namespace {

constexpr cfbox::help::HelpEntry HELP = {
    .name    = "pstree",
    .version = CFBOX_VERSION_STRING,
    .one_line = "display a tree of processes",
    .usage   = "pstree [-p]",
    .options = "  -p     show PIDs",
    .extra   = "",
};

struct PNode {
    pid_t pid = 0;
    pid_t ppid = 0;
    std::string comm;
    std::vector<pid_t> children;
};

auto print_tree(const std::map<pid_t, PNode>& nodes, pid_t pid,
                const std::string& prefix, bool show_pids, bool is_last) -> void {
    auto it = nodes.find(pid);
    if (it == nodes.end()) return;
    const auto& node = it->second;

    // Connector for this level
    std::string connector;
    if (pid != 1 || !prefix.empty()) {
        connector = is_last ? "\xe2\x94\x94\xe2\x94\x80\xe2\x94\x80"   // └──
                            : "\xe2\x94\x9c\xe2\x94\x80\xe2\x94\x80";   // ├──
    }

    std::printf("%s%s%s", prefix.c_str(), connector.c_str(), node.comm.c_str());
    if (show_pids) std::printf("(%d)", node.pid);
    std::printf("\n");

    // Child prefix: vertical line or spaces
    std::string child_prefix = prefix;
    if (pid != 1 || !prefix.empty()) {
        child_prefix += is_last ? "    "
                               : "\xe2\x94\x82   ";   // │
    }

    const auto& kids = node.children;
    for (size_t i = 0; i < kids.size(); ++i) {
        print_tree(nodes, kids[i], child_prefix, show_pids, i + 1 == kids.size());
    }
}

} // anonymous namespace

auto pstree_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'p', false, "show-pids"},
    });
    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool show_pids = parsed.has('p') || parsed.has_long("show-pids");

    auto result = cfbox::proc::read_all_processes();
    if (!result) {
        std::fprintf(stderr, "cfbox pstree: %s\n", result.error().msg.c_str());
        return 1;
    }

    // Build node map
    std::map<pid_t, PNode> nodes;
    for (const auto& proc : *result) {
        PNode node;
        node.pid = proc.pid;
        node.ppid = proc.ppid;
        node.comm = proc.comm;
        nodes[proc.pid] = std::move(node);
    }

    // Link children
    std::vector<pid_t> roots;
    for (auto& [pid, node] : nodes) {
        if (node.ppid == 0 || nodes.find(node.ppid) == nodes.end()) {
            roots.push_back(pid);
        } else {
            nodes[node.ppid].children.push_back(pid);
        }
    }

    // Sort children by PID for deterministic output
    for (auto& [pid, node] : nodes) {
        std::sort(node.children.begin(), node.children.end());
    }
    std::sort(roots.begin(), roots.end());

    // Print trees from root nodes
    for (size_t i = 0; i < roots.size(); ++i) {
        print_tree(nodes, roots[i], "", show_pids, i + 1 == roots.size());
    }

    return 0;
}
