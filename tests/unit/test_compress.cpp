#include <cfbox/compress.hpp>
#include <cfbox/deflate.hpp>
#include <gtest/gtest.h>

using namespace cfbox;

// === Deflate/Inflate unit tests ===

TEST(DeflateTest, EmptyInput) {
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(""), 0);
    auto result = deflate::inflate(compressed.data(), compressed.size(), 0);
    EXPECT_TRUE(result.empty());
}

TEST(DeflateTest, SingleByte) {
    const std::uint8_t data[] = {'A'};
    auto compressed = deflate::deflate_compress(data, 1);
    auto result = deflate::inflate(compressed.data(), compressed.size(), 1);
    EXPECT_EQ(result, "A");
}

TEST(DeflateTest, ShortString) {
    std::string input = "Hello World";
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    auto result = deflate::inflate(compressed.data(), compressed.size(), input.size());
    EXPECT_EQ(result, input);
}

TEST(DeflateTest, RepeatedPattern) {
    std::string input;
    for (int i = 0; i < 100; ++i) input += "abcabc";
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    auto result = deflate::inflate(compressed.data(), compressed.size(), input.size());
    EXPECT_EQ(result, input);
    // Repeated patterns should compress well
    EXPECT_LT(compressed.size(), input.size());
}

TEST(DeflateTest, LongInput) {
    std::string input;
    for (int i = 0; i < 10000; ++i) input += "The quick brown fox jumps over the lazy dog. ";
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    auto result = deflate::inflate(compressed.data(), compressed.size(), input.size());
    EXPECT_EQ(result, input);
    EXPECT_LT(compressed.size(), input.size() / 2);
}

TEST(DeflateTest, BinaryData) {
    std::string input(256, '\0');
    for (int i = 0; i < 256; ++i) input[static_cast<std::size_t>(i)] = static_cast<char>(i);
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    auto result = deflate::inflate(compressed.data(), compressed.size(), input.size());
    EXPECT_EQ(result, input);
}

TEST(DeflateTest, AllSameByte) {
    std::string input(1000, 'X');
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    auto result = deflate::inflate(compressed.data(), compressed.size(), input.size());
    EXPECT_EQ(result, input);
    // All same byte should compress very well
    EXPECT_LT(compressed.size(), 100);
}

// === Gzip compress/decompress ===

TEST(GzipTest, RoundTrip) {
    std::string input = "This is a test of the cfbox gzip implementation!";
    auto compressed = compress::gzip_compress(input);
    auto decompressed = compress::gzip_decompress(compressed);
    EXPECT_EQ(input, decompressed);
}

TEST(GzipTest, EmptyRoundTrip) {
    std::string input;
    auto compressed = compress::gzip_compress(input);
    auto decompressed = compress::gzip_decompress(compressed);
    EXPECT_EQ(input, decompressed);
}

TEST(GzipTest, LongRoundTrip) {
    std::string input;
    for (int i = 0; i < 1000; ++i) input += "Test data for compression testing. ";
    auto compressed = compress::gzip_compress(input);
    auto decompressed = compress::gzip_decompress(compressed);
    EXPECT_EQ(input, decompressed);
    EXPECT_LT(compressed.size(), input.size());
}

TEST(GzipTest, BinaryRoundTrip) {
    std::string input(1024, '\0');
    for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<char>(i & 0x7F);
    auto compressed = compress::gzip_compress(input);
    auto decompressed = compress::gzip_decompress(compressed);
    EXPECT_EQ(input, decompressed);
}

TEST(GzipTest, GzipHeaderValid) {
    auto compressed = compress::gzip_compress("test");
    ASSERT_GE(compressed.size(), 18u);
    EXPECT_EQ(static_cast<unsigned char>(compressed[0]), 0x1F);
    EXPECT_EQ(static_cast<unsigned char>(compressed[1]), 0x8B);
    EXPECT_EQ(static_cast<unsigned char>(compressed[2]), 8);
}

TEST(GzipTest, InvalidGzipData) {
    auto result = compress::gzip_decompress("not gzip data");
    EXPECT_TRUE(result.empty());
}

TEST(GzipTest, CorruptedTrailer) {
    auto compressed = compress::gzip_compress("test data");
    // Corrupt the CRC32 in trailer
    if (compressed.size() >= 4) {
        compressed[compressed.size() - 4] = static_cast<char>(compressed[compressed.size() - 4] ^ 0xFF);
    }
    auto result = compress::gzip_decompress(compressed);
    EXPECT_TRUE(result.empty());
}

// === Raw inflate (for unzip) ===

TEST(RawInflateTest, DeflateData) {
    std::string input = "Hello from raw deflate!";
    auto compressed = deflate::deflate_compress(
        reinterpret_cast<const std::uint8_t*>(input.data()), input.size());
    auto result = compress::raw_inflate(
        std::string_view(reinterpret_cast<const char*>(compressed.data()), compressed.size()),
        input.size());
    EXPECT_EQ(result, input);
}
