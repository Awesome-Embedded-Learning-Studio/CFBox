#include <cstdio>
#include <ctime>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#include <cfbox/args.hpp>
#include <cfbox/help.hpp>
#include <cfbox/io.hpp>

namespace {
constexpr cfbox::help::HelpEntry HELP = {
    .name    = "tar",
    .version = CFBOX_VERSION_STRING,
    .one_line = "create, extract, or list tar archives",
    .usage   = "tar -[cxt] [-f ARCHIVE] [-C DIR] [FILE]...",
    .options = "  -c     create a new archive\n"
               "  -x     extract from archive\n"
               "  -t     list archive contents\n"
               "  -f FILE  use FILE as the archive\n"
               "  -C DIR  change to DIR before operating",
    .extra   = "",
};

struct TarHeader {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char padding[12];
};

static auto write_octal(char* buf, int len, unsigned long val) -> void {
    std::snprintf(buf, static_cast<std::size_t>(len), "%0*lo", len - 1, val);
}

static auto compute_checksum(TarHeader& hdr) -> unsigned {
    std::memset(hdr.chksum, ' ', 8);
    auto* p = reinterpret_cast<unsigned char*>(&hdr);
    unsigned sum = 0;
    for (std::size_t i = 0; i < sizeof(hdr); ++i) sum += p[i];
    return sum;
}

static auto create_header(const std::string& name, unsigned long size, char type) -> TarHeader {
    TarHeader hdr{};
    std::strncpy(hdr.name, name.c_str(), 99);
    write_octal(hdr.mode, 8, 0644);
    write_octal(hdr.uid, 8, 0);
    write_octal(hdr.gid, 8, 0);
    write_octal(hdr.size, 12, size);
    write_octal(hdr.mtime, 12, static_cast<unsigned long>(::time(nullptr)));
    hdr.typeflag = type;
    std::memcpy(hdr.magic, "ustar", 5);
    hdr.version[0] = '0'; hdr.version[1] = '0';
    auto chk = compute_checksum(hdr);
    std::snprintf(hdr.chksum, 7, "%06o", chk);
    hdr.chksum[6] = '\0';
    hdr.chksum[7] = ' ';
    return hdr;
}

static auto collect_files(const std::filesystem::path& base,
                          std::vector<std::pair<std::string, std::filesystem::path>>& out) -> void {
    if (!std::filesystem::exists(base)) return;
    auto cwd = std::filesystem::current_path();
    if (std::filesystem::is_directory(base)) {
        out.emplace_back(std::filesystem::relative(base, cwd).string() + "/", base);
        for (const auto& entry : std::filesystem::recursive_directory_iterator(base)) {
            auto rel = std::filesystem::relative(entry.path(), cwd);
            out.emplace_back(rel.string(), entry.path());
        }
    } else {
        out.emplace_back(std::filesystem::relative(base, cwd).string(), base);
    }
}
} // namespace

auto tar_main(int argc, char* argv[]) -> int {
    auto parsed = cfbox::args::parse(argc, argv, {
        cfbox::args::OptSpec{'c', false},
        cfbox::args::OptSpec{'x', false},
        cfbox::args::OptSpec{'t', false},
        cfbox::args::OptSpec{'f', true, "file"},
        cfbox::args::OptSpec{'C', true, "directory"},
    });

    if (parsed.has_long("help"))    { cfbox::help::print_help(HELP); return 0; }
    if (parsed.has_long("version")) { cfbox::help::print_version(HELP); return 0; }

    bool create = parsed.has('c');
    bool extract = parsed.has('x');
    bool list = parsed.has('t');
    std::string archive = "-";
    if (auto f = parsed.get_any('f', "file")) archive = std::string{*f};

    std::string dir;
    if (auto d = parsed.get_any('C', "directory")) dir = std::string{*d};

    const auto& pos = parsed.positional();

    if (create) {
        if (!dir.empty()) std::filesystem::current_path(dir);

        std::string archive_data;
        auto targets = pos.empty() ? std::vector<std::string_view>{"."} : pos;

        std::vector<std::pair<std::string, std::filesystem::path>> files;
        for (auto t : targets) {
            collect_files(std::filesystem::path{t}, files);
        }

        for (auto& [relpath, fullpath] : files) {
            if (std::filesystem::is_directory(fullpath)) {
                auto hdr = create_header(relpath, 0, '5');
                archive_data.append(reinterpret_cast<const char*>(&hdr), 512);
                continue;
            }
            auto data = cfbox::io::read_all(fullpath.string());
            if (!data) {
                std::fprintf(stderr, "cfbox tar: %s: %s\n", relpath.c_str(), data.error().msg.c_str());
                continue;
            }
            auto hdr = create_header(relpath, data->size(), '0');
            archive_data.append(reinterpret_cast<const char*>(&hdr), 512);
            archive_data.append(*data);
            auto rem = data->size() % 512;
            if (rem > 0) archive_data.append(512 - rem, '\0');
        }
        archive_data.append(1024, '\0');

        if (archive == "-") {
            std::fwrite(archive_data.data(), 1, archive_data.size(), stdout);
        } else {
            auto wresult = cfbox::io::write_all(archive, archive_data);
            if (!wresult) {
                std::fprintf(stderr, "cfbox tar: %s\n", wresult.error().msg.c_str());
                return 1;
            }
        }
        return 0;
    }

    if (list || extract) {
        if (!dir.empty()) std::filesystem::current_path(dir);

        auto input = (archive == "-") ? cfbox::io::read_all_stdin() : cfbox::io::read_all(archive);
        if (!input) {
            std::fprintf(stderr, "cfbox tar: %s\n", input.error().msg.c_str());
            return 1;
        }
        const auto& data = *input;
        std::size_t offset = 0;
        while (offset + 512 <= data.size()) {
            auto* hdr = reinterpret_cast<const TarHeader*>(data.data() + offset);
            if (hdr->name[0] == '\0') break;

            unsigned long fsize = std::strtoul(hdr->size, nullptr, 8);
            std::string name{hdr->name, static_cast<std::size_t>(strnlen(hdr->name, 100))};

            if (list) {
                std::puts(name.c_str());
            } else if (extract) {
                if (hdr->typeflag == '5') {
                    std::filesystem::create_directories(name);
                } else {
                    auto parent = std::filesystem::path{name}.parent_path();
                    if (!parent.empty()) std::filesystem::create_directories(parent);
                    auto content = data.substr(offset + 512, fsize);
                    auto wresult = cfbox::io::write_all(name, content);
                    if (!wresult) {
                        std::fprintf(stderr, "cfbox tar: %s\n", wresult.error().msg.c_str());
                    }
                }
            }

            auto blocks = (fsize + 511) / 512;
            offset += 512 + blocks * 512;
        }
        return 0;
    }

    std::fprintf(stderr, "cfbox tar: must specify -c, -x, or -t\n");
    return 1;
}
