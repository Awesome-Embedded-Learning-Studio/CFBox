#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "ed",
    .version = CFBOX_VERSION_STRING,
    .one_line = "line-oriented text editor",
    .usage   = "ed [FILE]",
    .options = "",
    .extra   = "",
};
} // namespace

auto ed_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {});

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    std::vector<std::string> buffer;
    std::string filename;
    bool dirty = false;
    int current = 0;

    const auto& pos = parsed.positional();
    if (!pos.empty()) {
        filename = std::string{pos[0]};
        auto result = cfbox::io::read_lines(filename);
        if (result) buffer = std::move(*result);
    }

    auto prompt = (buffer.empty()) ? "" : "";
    char line[4096];
    while (std::fputs(prompt, stdout), std::fgets(line, sizeof(line), stdin)) {
        std::string cmd(line);
        if (!cmd.empty() && cmd.back() == '\n') cmd.pop_back();

        if (cmd == "q") {
            if (dirty) { std::fprintf(stderr, "?\n"); dirty = false; continue; }
            break;
        } else if (cmd == "Q") {
            break;
        } else if (cmd == "p" || cmd == ".") {
            if (current >= 1 && current <= static_cast<int>(buffer.size())) {
                std::puts(buffer[current - 1].c_str());
            }
        } else if (cmd == "n") {
            if (current >= 1 && current <= static_cast<int>(buffer.size())) {
                std::printf("%d\t%s\n", current, buffer[current - 1].c_str());
            }
        } else if (cmd == ",p" || cmd == "%p") {
            for (std::size_t i = 0; i < buffer.size(); ++i) {
                std::puts(buffer[i].c_str());
            }
        } else if (cmd.starts_with("w")) {
            auto fname = cmd.substr(cmd[0] == 'w' ? 1 : 0);
            while (!fname.empty() && fname.front() == ' ') fname = fname.substr(1);
            std::string target = fname.empty() ? filename : fname;
            if (target.empty()) { std::fprintf(stderr, "?\n"); continue; }
            if (fname.empty() && filename.empty()) { std::fprintf(stderr, "?\n"); continue; }
            std::string output;
            for (const auto& l : buffer) output += l + "\n";
            auto r = cfbox::io::write_all(target, output);
            if (!r) { std::fprintf(stderr, "?\n"); continue; }
            if (filename.empty()) filename = target;
            std::printf("%zu\n", buffer.size());
            dirty = false;
        } else if (cmd == "d") {
            if (current >= 1 && current <= static_cast<int>(buffer.size())) {
                buffer.erase(buffer.begin() + current - 1);
                dirty = true;
            }
        } else if (cmd == "a") {
            while (std::fgets(line, sizeof(line), stdin)) {
                std::string input(line);
                if (!input.empty() && input.back() == '\n') input.pop_back();
                if (input == ".") break;
                auto idx = static_cast<std::size_t>(current);
                buffer.insert(buffer.begin() + idx, input);
                ++current;
                dirty = true;
            }
        } else if (cmd == "i") {
            while (std::fgets(line, sizeof(line), stdin)) {
                std::string input(line);
                if (!input.empty() && input.back() == '\n') input.pop_back();
                if (input == ".") break;
                auto idx = static_cast<std::size_t>(current - 1);
                if (idx > buffer.size()) idx = buffer.size();
                buffer.insert(buffer.begin() + idx, input);
                ++current;
                dirty = true;
            }
        } else if (cmd == "$") {
            current = static_cast<int>(buffer.size());
            std::printf("%d\n", current);
        } else if (!cmd.empty()) {
            // Try as line number
            char* end = nullptr;
            long n = std::strtol(cmd.c_str(), &end, 10);
            if (*end == '\0' && n >= 1 && n <= static_cast<long>(buffer.size())) {
                current = static_cast<int>(n);
                std::puts(buffer[current - 1].c_str());
            }
        }
    }
    return 0;
}
