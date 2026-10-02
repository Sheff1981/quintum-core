#include "crypto/sha256.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace quintum::crypto {
namespace {

constexpr std::array<std::uint32_t, 64> kRoundConstants{
    0x428a2f98U, 0x71374491U, 0xb5c0fbcfU, 0xe9b5dba5U,
    0x3956c25bU, 0x59f111f1U, 0x923f82a4U, 0xab1c5ed5U,
    0xd807aa98U, 0x12835b01U, 0x243185beU, 0x550c7dc3U,
    0x72be5d74U, 0x80deb1feU, 0x9bdc06a7U, 0xc19bf174U,
    0xe49b69c1U, 0xefbe4786U, 0x0fc19dc6U, 0x240ca1ccU,
    0x2de92c6fU, 0x4a7484aaU, 0x5cb0a9dcU, 0x76f988daU,
    0x983e5152U, 0xa831c66dU, 0xb00327c8U, 0xbf597fc7U,
    0xc6e00bf3U, 0xd5a79147U, 0x06ca6351U, 0x14292967U,
    0x27b70a85U, 0x2e1b2138U, 0x4d2c6dfcU, 0x53380d13U,
    0x650a7354U, 0x766a0abbU, 0x81c2c92eU, 0x92722c85U,
    0xa2bfe8a1U, 0xa81a664bU, 0xc24b8b70U, 0xc76c51a3U,
    0xd192e819U, 0xd6990624U, 0xf40e3585U, 0x106aa070U,
    0x19a4c116U, 0x1e376c08U, 0x2748774cU, 0x34b0bcb5U,
    0x391c0cb3U, 0x4ed8aa4aU, 0x5b9cca4fU, 0x682e6ff3U,
    0x748f82eeU, 0x78a5636fU, 0x84c87814U, 0x8cc70208U,
    0x90befffaU, 0xa4506cebU, 0xbef9a3f7U, 0xc67178f2U,
};

constexpr std::array<std::uint32_t, 8> kInitialState{
    0x6a09e667U, 0xbb67ae85U, 0x3c6ef372U, 0xa54ff53aU,
    0x510e527fU, 0x9b05688cU, 0x1f83d9abU, 0x5be0cd19U,
};

constexpr std::uint32_t choose(
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t z) noexcept
{
    return (x & y) ^ (~x & z);
}

constexpr std::uint32_t majority(
    std::uint32_t x,
    std::uint32_t y,
    std::uint32_t z) noexcept
{
    return (x & y) ^ (x & z) ^ (y & z);
}

constexpr std::uint32_t big_sigma0(std::uint32_t x) noexcept
{
    return std::rotr(x, 2) ^ std::rotr(x, 13) ^ std::rotr(x, 22);
}

constexpr std::uint32_t big_sigma1(std::uint32_t x) noexcept
{
    return std::rotr(x, 6) ^ std::rotr(x, 11) ^ std::rotr(x, 25);
}

constexpr std::uint32_t small_sigma0(std::uint32_t x) noexcept
{
    return std::rotr(x, 7) ^ std::rotr(x, 18) ^ (x >> 3U);
}

constexpr std::uint32_t small_sigma1(std::uint32_t x) noexcept
{
    return std::rotr(x, 17) ^ std::rotr(x, 19) ^ (x >> 10U);
}

std::uint32_t load_be32(const Byte* p) noexcept
{
    return (static_cast<std::uint32_t>(p[0]) << 24U) |
           (static_cast<std::uint32_t>(p[1]) << 16U) |
           (static_cast<std::uint32_t>(p[2]) << 8U) |
           static_cast<std::uint32_t>(p[3]);
}

void store_be32(std::uint32_t value, Byte* out) noexcept
{
    out[0] = static_cast<Byte>(value >> 24U);
    out[1] = static_cast<Byte>(value >> 16U);
    out[2] = static_cast<Byte>(value >> 8U);
    out[3] = static_cast<Byte>(value);
}

} // namespace

Hash256 sha256(std::span<const Byte> data)
{
    std::vector<Byte> padded;
    padded.reserve(data.size() + 72U);
    padded.insert(padded.end(), data.begin(), data.end());
    padded.push_back(0x80U);

    while ((padded.size() % 64U) != 56U) {
        padded.push_back(0U);
    }

    const auto bit_length = static_cast<std::uint64_t>(data.size()) * 8U;
    for (int shift = 56; shift >= 0; shift -= 8) {
        padded.push_back(static_cast<Byte>(bit_length >> static_cast<unsigned>(shift)));
    }

    auto state = kInitialState;

    for (std::size_t offset = 0; offset < padded.size(); offset += 64U) {
        std::array<std::uint32_t, 64> schedule{};

        for (std::size_t i = 0; i < 16U; ++i) {
            schedule[i] = load_be32(padded.data() + offset + (i * 4U));
        }

        for (std::size_t i = 16U; i < 64U; ++i) {
            schedule[i] =
                small_sigma1(schedule[i - 2U]) +
                schedule[i - 7U] +
                small_sigma0(schedule[i - 15U]) +
                schedule[i - 16U];
        }

        auto a = state[0];
        auto b = state[1];
        auto c = state[2];
        auto d = state[3];
        auto e = state[4];
        auto f = state[5];
        auto g = state[6];
        auto h = state[7];

        for (std::size_t i = 0; i < 64U; ++i) {
            const auto t1 = h + big_sigma1(e) + choose(e, f, g) +
                            kRoundConstants[i] + schedule[i];
            const auto t2 = big_sigma0(a) + majority(a, b, c);

            h = g;
            g = f;
            f = e;
            e = d + t1;
            d = c;
            c = b;
            b = a;
            a = t1 + t2;
        }

        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    Hash256 digest{};
    for (std::size_t i = 0; i < state.size(); ++i) {
        store_be32(state[i], digest.data() + (i * 4U));
    }
    return digest;
}

Hash256 double_sha256(std::span<const Byte> data)
{
    const auto first = sha256(data);
    return sha256(first);
}

} // namespace quintum::crypto
