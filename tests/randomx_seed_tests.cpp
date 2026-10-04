#include "consensus/randomx_seed.hpp"

#include <cassert>
#include <string>

namespace {

std::string hex(const quintum::Hash256& hash)
{
    static constexpr char digits[] =
        "0123456789abcdef";

    std::string out;
    out.reserve(hash.size() * 2U);

    for (const auto byte : hash) {
        out.push_back(
            digits[(byte >> 4U) & 0x0fU]
        );
        out.push_back(
            digits[byte & 0x0fU]
        );
    }

    return out;
}

void test_seed_height_boundaries()
{
    using namespace quintum::consensus;

    static_assert(
        kRandomXSeedEpochBlocks == 2'048U
    );
    static_assert(
        kRandomXSeedEpochLag == 64U
    );

    assert(randomx_seed_height(0U) == 0U);
    assert(randomx_seed_height(1U) == 0U);
    assert(randomx_seed_height(2'048U) == 0U);
    assert(randomx_seed_height(2'112U) == 0U);
    assert(randomx_seed_height(2'113U) == 2'048U);
    assert(randomx_seed_height(4'160U) == 2'048U);
    assert(randomx_seed_height(4'161U) == 4'096U);
}

void test_seed_key_vector()
{
    quintum::Hash256 seed_block_hash{};

    for (std::size_t i = 0U;
         i < seed_block_hash.size();
         ++i) {
        seed_block_hash[i] =
            static_cast<quintum::Byte>(i);
    }

    const auto key =
        quintum::consensus::randomx_seed_key(
            2'048U,
            seed_block_hash
        );

    assert(
        hex(key) ==
        "67a2b8bd572c7b8d9982a9a15df773d6213627cf2a217646989dc16f73bd4d09"
    );
}

void test_height_changes_key()
{
    quintum::Hash256 seed_block_hash{};
    seed_block_hash.back() = 0x42U;

    const auto a =
        quintum::consensus::randomx_seed_key(
            0U,
            seed_block_hash
        );
    const auto b =
        quintum::consensus::randomx_seed_key(
            2'048U,
            seed_block_hash
        );

    assert(a != b);
}

} // namespace

int main()
{
    test_seed_height_boundaries();
    test_seed_key_vector();
    test_height_changes_key();
    return 0;
}
