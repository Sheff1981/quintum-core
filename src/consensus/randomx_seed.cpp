#include "consensus/randomx_seed.hpp"

#include "core/serialize.hpp"
#include "crypto/sha256.hpp"

namespace quintum::consensus {

std::uint64_t randomx_seed_height(
    std::uint64_t candidate_height) noexcept
{
    constexpr std::uint64_t first_change =
        kRandomXSeedEpochBlocks +
        kRandomXSeedEpochLag;

    if (candidate_height <= first_change) {
        return 0U;
    }

    static_assert(
        (kRandomXSeedEpochBlocks &
         (kRandomXSeedEpochBlocks - 1U)) == 0U,
        "RandomX seed epoch must be a power of two"
    );

    return (
        candidate_height -
        kRandomXSeedEpochLag -
        1U
    ) &
        ~(kRandomXSeedEpochBlocks - 1U);
}

Hash256 randomx_seed_key(
    std::uint64_t seed_height,
    const Hash256& seed_block_hash)
{
    Bytes preimage;
    preimage.reserve(
        kRandomXSeedDomain.size() +
        sizeof(seed_height) +
        seed_block_hash.size()
    );

    for (const unsigned char ch :
         kRandomXSeedDomain) {
        preimage.push_back(
            static_cast<Byte>(ch)
        );
    }

    append_little_endian(
        preimage,
        seed_height
    );

    preimage.insert(
        preimage.end(),
        seed_block_hash.begin(),
        seed_block_hash.end()
    );

    return crypto::double_sha256(preimage);
}

} // namespace quintum::consensus
