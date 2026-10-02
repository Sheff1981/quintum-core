#pragma once

#include "consensus/chainparams.hpp"
#include "core/types.hpp"

#include <cstdint>

namespace quintum::consensus {

enum class DifficultyError {
    none,
    invalid_previous_target,
    invalid_pow_limit,
    arithmetic_failure,
};

struct DifficultyResult {
    DifficultyError error{DifficultyError::none};
    std::uint32_t bits{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == DifficultyError::none;
    }
};

[[nodiscard]] DifficultyResult calculate_retarget_bits(
    std::uint32_t previous_bits,
    std::uint64_t first_block_timestamp,
    std::uint64_t last_block_timestamp,
    const PowParams& params
);

[[nodiscard]] Hash256 scale_target_clamped(
    const Hash256& target,
    std::uint64_t numerator,
    std::uint64_t denominator,
    const Hash256& limit
);

[[nodiscard]] bool target_within_pow_limit(
    std::uint32_t bits,
    const PowParams& params
);

} // namespace quintum::consensus
