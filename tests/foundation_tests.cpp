#include "core/serialize.hpp"
#include "core/types.hpp"
#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <string>

namespace {

std::string to_hex(const quintum::Hash256& hash)
{
    static constexpr char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(hash.size() * 2U);

    for (const auto byte : hash) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }
    return out;
}

void test_hash_types()
{
    quintum::Hash256 hash{};
    static_assert(hash.size() == 32);
    assert(std::all_of(hash.begin(), hash.end(), [](quintum::Byte b) { return b == 0; }));
}

void test_little_endian()
{
    quintum::Bytes bytes;
    quintum::append_little_endian<std::uint32_t>(bytes, 0x12345678U);
    assert((bytes == quintum::Bytes{0x78U, 0x56U, 0x34U, 0x12U}));

    std::size_t offset = 0;
    const auto value = quintum::read_little_endian<std::uint32_t>(bytes, offset);
    assert(value && *value == 0x12345678U);
    assert(offset == bytes.size());
}

void test_compact_size()
{
    constexpr std::array<std::uint64_t, 6> values{
        252U, 253U, 65535U, 65536U, 0xffffffffULL, 0x100000000ULL
    };

    for (const auto expected : values) {
        quintum::Bytes encoded;
        quintum::append_compact_size(encoded, expected);

        std::size_t offset = 0;
        const auto decoded = quintum::read_compact_size(encoded, offset);
        assert(decoded && *decoded == expected);
        assert(offset == encoded.size());
    }

    // Reject a non-canonical representation of the value 1.
    const quintum::Bytes non_canonical{253U, 1U, 0U};
    std::size_t offset = 0;
    assert(!quintum::read_compact_size(non_canonical, offset));
}

void test_sha256()
{
    const std::array<quintum::Byte, 0> empty{};
    assert(
        to_hex(quintum::crypto::sha256(empty)) ==
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"
    );
    assert(
        to_hex(quintum::crypto::double_sha256(empty)) ==
        "5df6e0e2761359d30a8275058e299fcc0381534545f55cf43e41983f5d4c9456"
    );

    constexpr std::array<quintum::Byte, 3> abc{
        static_cast<quintum::Byte>('a'),
        static_cast<quintum::Byte>('b'),
        static_cast<quintum::Byte>('c')
    };
    assert(
        to_hex(quintum::crypto::sha256(abc)) ==
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"
    );
    assert(
        to_hex(quintum::crypto::double_sha256(abc)) ==
        "4f8b42c22dd3729b519ba6f68d2da7cc5b2d606d05daed5ad5128cc03e6c6358"
    );
}

} // namespace

int main()
{
    test_hash_types();
    test_little_endian();
    test_compact_size();
    test_sha256();
    return 0;
}
