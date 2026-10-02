#pragma once

#include "core/types.hpp"
#include "primitives/block.hpp"

#include <cstdint>

namespace quintum::consensus {

struct CompactTarget {
    Hash256 target{};
    bool negative{false};
    bool overflow{false};

    [[nodiscard]] bool is_zero() const noexcept;
    [[nodiscard]] bool valid() const noexcept;
};

enum class PowCheckError {
    none,
    invalid_target,
    hash_above_target,
};

enum class MineStatus {
    found,
    exhausted,
    invalid_target,
};

struct MiningResult {
    MineStatus status{MineStatus::exhausted};
    std::uint64_t nonce{0};
    Hash256 hash{};
    std::uint64_t attempts{0};

    [[nodiscard]] bool found() const noexcept
    {
        return status == MineStatus::found;
    }
};

[[nodiscard]] CompactTarget decode_compact_target(std::uint32_t bits);
[[nodiscard]] std::uint32_t encode_compact_target(const Hash256& target);

[[nodiscard]] bool hash_meets_target(
    const Hash256& hash,
    const Hash256& target
) noexcept;

[[nodiscard]] PowCheckError check_proof_of_work(
    const BlockHeader& header
);

[[nodiscard]] MiningResult mine_header(
    BlockHeader& header,
    std::uint64_t max_attempts
);

} // namespace quintum::consensus
