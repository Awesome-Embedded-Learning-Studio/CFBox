#pragma once

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace cfbox::checksum {

// CRC-32 using the POSIX polynomial (0xEDB88320 reflected)
inline auto crc32(std::string_view data) -> std::uint32_t {
    std::uint32_t crc = 0xFFFFFFFF;
    for (auto byte : data) {
        crc ^= static_cast<std::uint8_t>(byte);
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        }
    }
    return ~crc;
}

struct MD5Hash {
    std::array<std::uint8_t, 16> bytes{};
};

inline auto md5_to_hex(const MD5Hash& hash) -> std::string {
    static constexpr char hex[] = "0123456789abcdef";
    std::string result;
    result.reserve(32);
    for (auto b : hash.bytes) {
        result += hex[b >> 4];
        result += hex[b & 0x0F];
    }
    return result;
}

inline auto md5(std::string_view data) -> MD5Hash {
    MD5Hash result;

    static constexpr std::uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee,
        0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be,
        0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa,
        0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed,
        0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c,
        0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039,
        0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1,
        0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391,
    };

    static constexpr unsigned s[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21,
    };

    auto left_rotate = [](std::uint32_t x, unsigned c) -> std::uint32_t {
        return (x << c) | (x >> (32 - c));
    };

    // Padding
    std::size_t orig_len = data.size();
    auto bit_len = static_cast<std::uint64_t>(orig_len) * 8;
    std::size_t padded_len = ((orig_len + 8) / 64 + 1) * 64;
    std::vector<std::uint8_t> msg(padded_len, 0);
    std::memcpy(msg.data(), data.data(), orig_len);
    msg[orig_len] = 0x80;
    for (int j = 0; j < 8; ++j) {
        msg[padded_len - 8 + j] = static_cast<std::uint8_t>(bit_len >> (j * 8));
    }

    std::uint32_t a0 = 0x67452301;
    std::uint32_t b0 = 0xEFCDAB89;
    std::uint32_t c0 = 0x98BADCFE;
    std::uint32_t d0 = 0x10325476;

    for (std::size_t offset = 0; offset < padded_len; offset += 64) {
        std::uint32_t M[16];
        for (std::size_t j = 0; j < 16; ++j) {
            auto base = offset + j * 4;
            M[j] = static_cast<std::uint32_t>(msg[base])
                 | (static_cast<std::uint32_t>(msg[base + 1]) << 8)
                 | (static_cast<std::uint32_t>(msg[base + 2]) << 16)
                 | (static_cast<std::uint32_t>(msg[base + 3]) << 24);
        }

        std::uint32_t A = a0, B = b0, C = c0, D = d0;

        for (int j = 0; j < 64; ++j) {
            std::uint32_t F;
            int idx;
            if (j < 16) {
                F = (B & C) | (~B & D);
                idx = j;
            } else if (j < 32) {
                F = (D & B) | (~D & C);
                idx = (5 * j + 1) % 16;
            } else if (j < 48) {
                F = B ^ C ^ D;
                idx = (3 * j + 5) % 16;
            } else {
                F = C ^ (B | ~D);
                idx = (7 * j) % 16;
            }
            F = F + A + K[j] + M[idx];
            A = D;
            D = C;
            C = B;
            B = B + left_rotate(F, s[j]);
        }

        a0 += A; b0 += B; c0 += C; d0 += D;
    }

    for (int j = 0; j < 4; ++j) {
        result.bytes[j]      = static_cast<std::uint8_t>(a0 >> (j * 8));
        result.bytes[j + 4]  = static_cast<std::uint8_t>(b0 >> (j * 8));
        result.bytes[j + 8]  = static_cast<std::uint8_t>(c0 >> (j * 8));
        result.bytes[j + 12] = static_cast<std::uint8_t>(d0 >> (j * 8));
    }
    return result;
}

struct SumResult {
    std::uint16_t checksum;
    unsigned blocks;
};

inline auto bsd_sum(std::string_view data) -> SumResult {
    SumResult result{0, 0};
    for (auto byte : data) {
        auto rot = static_cast<std::uint32_t>(result.checksum >> 1) +
                   static_cast<std::uint32_t>((result.checksum & 1) << 15);
        result.checksum = static_cast<std::uint16_t>(rot + static_cast<std::uint8_t>(byte));
    }
    result.blocks = (static_cast<unsigned>(data.size()) + 1023) / 1024;
    if (result.blocks == 0) result.blocks = 1;
    return result;
}

inline auto sysv_sum(std::string_view data) -> SumResult {
    SumResult result{0, 0};
    unsigned long s = 0;
    for (auto byte : data) {
        s += static_cast<std::uint8_t>(byte);
    }
    auto val = static_cast<std::uint32_t>((s & 0xFFFF) + ((s >> 16) & 0xFFFF));
    result.checksum = static_cast<std::uint16_t>(val);
    result.blocks = (static_cast<unsigned>(data.size()) + 511) / 512;
    if (result.blocks == 0) result.blocks = 1;
    return result;
}

} // namespace cfbox::checksum
