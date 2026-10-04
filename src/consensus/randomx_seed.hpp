#pragma once

#include "core/types.hpp"

#include <cstdint>
#include <string_view>

namespace quintum::consensus {

inline constexpr std::uint64_t kRandomXSeedEpochBlocks =
    2'048U;
inline constexpr std::uint64_t kRandomXSeedEpochLag =
    64U;
inline constexpr std::string_view kRandomXSeedDomain =
    "QUINTUM-RX-SEED-V1";

[[nodiscard]] std::uint64_t randomx_seed_height(
    std::uint64_t candidate_height
) noexcept;

// Consensus key derivation:
// double-SHA-256(
//   ASCII("QUINTUM-RX-SEED-V1") ||
//   uint64_le(seed_height) ||
//   seed_block_hash[32]
// )
[[nodiscard]] Hash256 randomx_seed_key(
    std::uint64_t seed_height,
    const Hash256& seed_block_hash
);

} // namespace quintum::consensus
