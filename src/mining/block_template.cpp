#include "mining/block_template.hpp"

#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "core/serialize.hpp"

#include <limits>
#include <utility>

namespace quintum::mining {

BlockTemplateResult create_block_template(
    const Chainstate& chain,
    const Bytes& payout_script,
    std::uint64_t timestamp,
    std::span<const Transaction> transactions)
{
    BlockTemplateResult out;

    if (chain.empty()) {
        out.error = BlockTemplateError::empty_chain;
        return out;
    }

    const auto current_height = chain.height();
    const auto previous_hash = chain.tip_hash();

    if (!current_height || !previous_hash ||
        *current_height == std::numeric_limits<std::uint32_t>::max()) {
        out.error = BlockTemplateError::height_overflow;
        return out;
    }

    if (!consensus::parse_p2pk_locking_script(payout_script)) {
        out.error = BlockTemplateError::invalid_payout_script;
        return out;
    }

    const std::uint32_t height = *current_height + 1U;
    const auto bits = chain.next_work_required(timestamp);

    if (!bits) {
        out.error = BlockTemplateError::difficulty_unavailable;
        return out;
    }

    UtxoSet view = chain.utxos();
    Amount total_fees{0U};

    for (std::size_t i = 0U; i < transactions.size(); ++i) {
        const auto& tx = transactions[i];

        if (tx.is_coinbase()) {
            out.error = BlockTemplateError::candidate_is_coinbase;
            out.transaction_index = i;
            return out;
        }

        auto applied = view.apply_transaction(tx, height);
        if (!applied.ok()) {
            out.error = BlockTemplateError::transaction_failed;
            out.transaction_error = applied.error;
            out.transaction_index = i;
            return out;
        }

        if (applied.fee > consensus::kMaxMoney - total_fees) {
            out.error = BlockTemplateError::fee_sum_overflow;
            out.transaction_index = i;
            return out;
        }

        total_fees += applied.fee;
    }

    const Amount subsidy = consensus::block_subsidy(height);
    if (total_fees > consensus::kMaxMoney - subsidy) {
        out.error = BlockTemplateError::reward_overflow;
        return out;
    }

    Transaction coinbase;
    TxInput coinbase_input;
    append_little_endian(coinbase_input.unlocking_script, height);
    coinbase.inputs.push_back(std::move(coinbase_input));

    TxOutput reward;
    reward.value = subsidy + total_fees;
    reward.locking_script = payout_script;
    coinbase.outputs.push_back(std::move(reward));

    Block block;
    block.header.version = 1U;
    block.header.previous_block = *previous_hash;
    block.header.timestamp = timestamp;
    block.header.bits = *bits;
    block.header.nonce = 0U;

    block.transactions.reserve(1U + transactions.size());
    block.transactions.push_back(std::move(coinbase));
    block.transactions.insert(
        block.transactions.end(),
        transactions.begin(),
        transactions.end()
    );

    update_merkle_root(block);

    const auto resource_error =
        consensus::validate_block_resources(block, chain.params().limits);

    if (resource_error != consensus::BlockResourceError::none) {
        out.error = BlockTemplateError::resource_limits_exceeded;
        out.resource_error = resource_error;
        return out;
    }

    if (validate_block_structure(block) != BlockStructureError::none) {
        out.error = BlockTemplateError::resource_limits_exceeded;
        return out;
    }

    out.value = BlockTemplate{
        .block = std::move(block),
        .height = height,
        .total_fees = total_fees,
    };
    return out;
}

} // namespace quintum::mining
