#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>

#include <cfbox/checksum.hpp>
#include <cfbox/deflate.hpp>
#include <cfbox/inflate.hpp>

namespace cfbox::compress {

// Write a little-endian 32-bit value
inline auto write_le32(std::uint32_t val, std::string& out) -> void {
    out += static_cast<char>(val & 0xFF);
    out += static_cast<char>((val >> 8) & 0xFF);
    out += static_cast<char>((val >> 16) & 0xFF);
    out += static_cast<char>((val >> 24) & 0xFF);
}

// Read a little-endian 32-bit value
inline auto read_le32(const std::uint8_t* p) -> std::uint32_t {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}

// Gzip compress: RFC 1952 header + deflate + CRC32 + size trailer
inline auto gzip_compress(std::string_view data) -> std::string {
    std::string out;

    // Gzip header (10 bytes)
    out += static_cast<char>(0x1F); // ID1
    out += static_cast<char>(0x8B); // ID2
    out += static_cast<char>(8);    // CM = deflate
    out += static_cast<char>(0);    // FLG
    out += static_cast<char>(0);    // MTIME (4 bytes)
    out += static_cast<char>(0);
    out += static_cast<char>(0);
    out += static_cast<char>(0);
    out += static_cast<char>(0);    // XFL
    out += static_cast<char>(255);  // OS = unknown

    // Deflate compressed data
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
    out.append(reinterpret_cast<const char*>(compressed.data()),
               static_cast<std::size_t>(compressed.size()));

    // Trailer: CRC32 + ISIZE
    auto crc = checksum::crc32(data);
    write_le32(crc, out);
    write_le32(static_cast<std::uint32_t>(data.size() & 0xFFFFFFFF), out);

    return out;
}

// Gzip decompress: parse RFC 1952 header + inflate + verify CRC32
inline auto gzip_decompress(std::string_view data) -> std::string {
    if (data.size() < 18) return {};
    auto* p = reinterpret_cast<const std::uint8_t*>(data.data());

    // Check gzip magic
    if (p[0] != 0x1F || p[1] != 0x8B || p[2] != 8) return {};

    std::uint8_t flg = p[3];
    std::size_t offset = 10;

    // Skip optional fields based on FLG
    if (flg & 0x04) { // FEXTRA
        auto xlen = static_cast<std::size_t>(p[offset]) |
                    (static_cast<std::size_t>(p[offset + 1]) << 8);
        offset += 2 + xlen;
    }
    if (flg & 0x08) { // FNAME
        while (offset < data.size() && p[offset] != 0) ++offset;
        ++offset; // skip null terminator
    }
    if (flg & 0x10) { // FCOMMENT
        while (offset < data.size() && p[offset] != 0) ++offset;
        ++offset;
    }
    if (flg & 0x02) { // FHCRC
        offset += 2;
    }

    if (offset + 8 > data.size()) return {};

    // Compressed data is between offset and (end - 8)
    std::size_t compressed_size = data.size() - offset - 8;

    // Read trailer
    auto* trailer = p + data.size() - 8;
    auto expected_crc = read_le32(trailer);
    auto expected_size = read_le32(trailer + 4);

    // Inflate
    auto result = deflate::inflate(p + offset, compressed_size, expected_size);

    // Verify
    auto actual_crc = checksum::crc32(result);
    if (actual_crc != expected_crc) return {};
    if ((result.size() & 0xFFFFFFFF) != expected_size) return {};

    return result;
}

// Raw deflate decompression (for unzip method 8)
inline auto raw_inflate(std::string_view compressed, std::size_t expected_size) -> std::string {
    return deflate::inflate(
        reinterpret_cast<const std::uint8_t*>(compressed.data()),
        compressed.size(), expected_size);
}

} // namespace cfbox::compress
