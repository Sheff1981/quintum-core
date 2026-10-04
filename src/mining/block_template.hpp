#pragma once

#include "chain/chainstate.hpp"
#include "consensus/block_limits.hpp"
#include "primitives/block.hpp"

#include <cstddef>
#include <cstdint>
#include <span>

namespace quintum::mining {

enum class BlockTemplateError {
    none,
    empty_chain,
    height_overflow,
    invalid_payout_script,
    timestamp_overflow,
    timestamp_too_far_future,
    difficulty_unavailable,
    candidate_is_coinbase,
    transaction_failed,
    fee_sum_overflow,
    reward_overflow,
    founder_payout_unavailable,
    resource_limits_exceeded,
};

struct BlockTemplate {
    Block block{};
    std::uint32_t height{0U};
    Amount total_fees{0U};
};

struct BlockTemplateResult {
    BlockTemplateError error{BlockTemplateError::none};
    BlockTemplate value{};
    UtxoApplyError transaction_error{UtxoApplyError::none};
    consensus::BlockResourceError resource_error{
        consensus::BlockResourceError::none
    };
    std::size_t transaction_index{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == BlockTemplateError::none;
    }
};

[[nodiscard]] BlockTemplateResult create_block_template(
    const Chainstate& chain,
    const Bytes& payout_script,
    std::uint64_t timestamp,
    std::span<const Transaction> transactions = {}
);

} // namespace quintum::mining
