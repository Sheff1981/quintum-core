#pragma once

#include "consensus/chainparams.hpp"
#include "primitives/block.hpp"

namespace quintum::consensus {

enum class BlockResourceError {
    none,
    serialized_size_overflow,
    block_too_large,
    too_many_transactions,
    script_too_large,
    coinbase_script_too_large,
};

[[nodiscard]] BlockResourceError validate_block_resources(
    const Block& block,
    const ResourceLimits& limits
) noexcept;

} // namespace quintum::consensus
