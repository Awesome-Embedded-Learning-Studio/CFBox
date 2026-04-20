#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>
#include <cfbox/stream.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "cut",
    .version = CFBOX_VERSION_STRING,
    .one_line = "remove sections from each line of files",
    .usage   = "cut -d DELIM -f LIST [FILE]...",
    .options = "  -d DELIM  use DELIM instead of TAB for field delimiter\n"
               "  -f LIST   select only these fields\n"
               "  -c LIST   select only these characters\n"
               "  -s        do not print lines without delimiter",
    .extra   = "LIST: comma-separated ranges (e.g., 1,3-5,7-)",
};
} // namespace

static auto parse_range_list(const std::string& list) -> std::set<int> {
    std::set<int> fields;
    std::string token;
    for (std::size_t i = 0; i <= list.size(); ++i) {
        if (i == list.size() || list[i] == ',') {
            auto dash = token.find('-');
            if (dash == std::string::npos) {
                fields.insert(std::stoi(token));
            } else if (dash == 0) {
                int end = std::stoi(token.substr(1));
                for (int j = 1; j <= end; ++j) fields.insert(j);
            } else if (dash == token.size() - 1) {
                int start = std::stoi(token.substr(0, dash));
                for (int j = start; j <= 1024; ++j) fields.insert(j);
            } else {
                int start = std::stoi(token.substr(0, dash));
                int end = std::stoi(token.substr(dash + 1));
                for (int j = start; j <= end; ++j) fields.insert(j);
            }
            token.clear();
        } else {
            token += list[i];
        }
    }
    return fields;
}

auto cut_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'d', true, "delimiter"},
        cfbox::args::OptSpec{'f', true, "fields"},
        cfbox::args::OptSpec{'c', true, "characters"},
        cfbox::args::OptSpec{'s', false},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    char delim = '\t';
    if (auto d = parsed.get_any('d', "delimiter")) {
        if (d->size() != 1) {
            std::fprintf(stderr, "cfbox cut: delimiter must be a single character\n");
            return 1;
        }
        delim = (*d)[0];
    }

    bool skip_no_delim = parsed.has('s');
    bool field_mode = parsed.has_any('f', "fields");
    bool char_mode = parsed.has_any('c', "characters");

    if (!field_mode && !char_mode) {
        std::fprintf(stderr, "cfbox cut: you must specify a list of fields or characters\n");
        return 1;
    }

    std::set<int> indices;
    if (field_mode) {
        auto list = parsed.get_any('f', "fields");
        if (!list) {
            std::fprintf(stderr, "cfbox cut: missing list for -f\n");
            return 1;
        }
        indices = parse_range_list(std::string{*list});
    } else {
        auto list = parsed.get_any('c', "characters");
        if (!list) {
            std::fprintf(stderr, "cfbox cut: missing list for -c\n");
            return 1;
        }
        indices = parse_range_list(std::string{*list});
    }

    const auto& pos = parsed.positional();
    auto paths = pos.empty() ? std::vector<std::string_view>{"-"} : pos;

    int rc = 0;
    for (auto p : paths) {
        auto result = cfbox::stream::for_each_line(p, [&](const std::string& line, std::size_t) {
            if (char_mode) {
                bool first = true;
                for (int idx : indices) {
                    if (idx >= 1 && static_cast<std::size_t>(idx - 1) < line.size()) {
                        if (!first) std::putchar(delim);
                        std::putchar(line[idx - 1]);
                        first = false;
                    }
                }
                std::putchar('\n');
            } else {
                auto fields = cfbox::stream::split_fields(line, delim);
                if (fields.size() <= 1 && skip_no_delim) {
                    return true;
                }
                bool first = true;
                for (int idx : indices) {
                    if (idx >= 1 && static_cast<std::size_t>(idx - 1) < fields.size()) {
                        if (!first) std::putchar(delim);
                        std::fputs(fields[idx - 1].c_str(), stdout);
                        first = false;
                    }
                }
                std::putchar('\n');
            }
            return true;
        });
        if (!result) {
            std::fprintf(stderr, "cfbox cut: %s\n", result.error().msg.c_str());
            rc = 1;
        }
    }
    return rc;
}
