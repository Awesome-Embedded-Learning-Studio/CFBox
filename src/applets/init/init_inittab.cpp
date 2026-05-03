#include "init.hpp"

namespace cfbox::init {

auto parse_inittab_line(std::string_view line) -> InittabEntry {
    InittabEntry entry;

    // Skip empty lines and comments
    if (line.empty() || line[0] == '#') return entry;

    // Format: id:runlevels:action:process
    // Find colons
    auto c1 = line.find(':');
    if (c1 == std::string_view::npos) return entry;
    auto c2 = line.find(':', c1 + 1);
    if (c2 == std::string_view::npos) return entry;
    auto c3 = line.find(':', c2 + 1);
    if (c3 == std::string_view::npos) return entry;

    entry.id = std::string(line.substr(0, c1));
    entry.runlevels = std::string(line.substr(c1 + 1, c2 - c1 - 1));
    entry.action = std::string(line.substr(c2 + 1, c3 - c2 - 1));
    entry.process = std::string(line.substr(c3 + 1));

    // Trim trailing whitespace from process
    while (!entry.process.empty() && (entry.process.back() == ' ' || entry.process.back() == '\t' || entry.process.back() == '\r'))
        entry.process.pop_back();

    return entry;
}

auto parse_inittab(const std::string& path) -> std::vector<InittabEntry> {
    std::vector<InittabEntry> entries;
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) return entries;

    char buf[1024];
    while (std::fgets(buf, sizeof(buf), f)) {
        std::string_view line(buf);
        // Remove newline
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line = line.substr(0, line.size() - 1);

        auto entry = parse_inittab_line(line);
        if (!entry.action.empty() && !entry.process.empty()) {
            entries.push_back(std::move(entry));
        }
    }

    std::fclose(f);
    return entries;
}

} // namespace cfbox::init
