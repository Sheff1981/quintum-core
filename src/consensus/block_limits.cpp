#include "consensus/block_limits.hpp"

#include <cstdint>

namespace quintum::consensus {

BlockResourceError validate_block_resources(
    const Block& block,
    const ResourceLimits& limits) noexcept
{
    if (block.transactions.size() >
        static_cast<std::size_t>(
            limits.max_block_transactions)) {
        return BlockResourceError::too_many_transactions;
    }

    const auto block_size =
        serialized_block_size(block);

    if (!block_size) {
        return BlockResourceError::serialized_size_overflow;
    }

    if (*block_size >
        static_cast<std::size_t>(
            limits.max_block_serialized_bytes)) {
        return BlockResourceError::block_too_large;
    }

    for (std::size_t tx_index = 0U;
         tx_index < block.transactions.size();
         ++tx_index) {
        const auto& tx = block.transactions[tx_index];

        for (const auto& input : tx.inputs) {
            if (input.unlocking_script.size() >
                static_cast<std::size_t>(
                    limits.max_script_bytes)) {
                return BlockResourceError::script_too_large;
            }
        }

        for (const auto& output : tx.outputs) {
            if (output.locking_script.size() >
                static_cast<std::size_t>(
                    limits.max_script_bytes)) {
                return BlockResourceError::script_too_large;
            }
        }

        if (tx_index == 0U &&
            tx.is_coinbase() &&
            !tx.inputs.empty() &&
            tx.inputs.front().unlocking_script.size() >
                static_cast<std::size_t>(
                    limits.max_coinbase_script_bytes)) {
            return BlockResourceError::coinbase_script_too_large;
        }
    }

    return BlockResourceError::none;
}

} // namespace quintum::consensus
