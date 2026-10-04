#include "mining/block_template.hpp"

#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "consensus/time.hpp"
#include "crypto/secp256k1.hpp"
#include "core/serialize.hpp"

#include <algorithm>
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

    const auto payout_public_key =
        consensus::parse_p2pk_locking_script(payout_script);

    if (!payout_public_key ||
        !crypto::is_valid_public_key(*payout_public_key)) {
        out.error = BlockTemplateError::invalid_payout_script;
        return out;
    }

    const auto tip_timestamp = chain.tip_timestamp();
    if (!tip_timestamp) {
        out.error = BlockTemplateError::timestamp_overflow;
        return out;
    }

    const std::uint64_t candidate_timestamp =
        std::max(timestamp, *tip_timestamp + 1U);

    if (!consensus::timestamp_not_too_far_future(
            candidate_timestamp,
            timestamp,
            chain.params().time.max_future_seconds)) {
        out.error = BlockTemplateError::timestamp_too_far_future;
        return out;
    }

    const std::uint32_t height = *current_height + 1U;
    const auto bits =
        chain.next_work_required(candidate_timestamp);

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

        auto applied = view.apply_transaction(
            tx,
            height,
            chain.params().monetary
        );
        if (!applied.ok()) {
            out.error = BlockTemplateError::transaction_failed;
            out.transaction_error = applied.error;
            out.transaction_index = i;
            return out;
        }

        if (applied.fee >
            chain.params().monetary.max_money -
                total_fees) {
            out.error = BlockTemplateError::fee_sum_overflow;
            out.transaction_index = i;
            return out;
        }

        total_fees += applied.fee;
    }

    const auto& monetary =
        chain.params().monetary;

    const Amount subsidy =
        consensus::block_subsidy(
            height,
            monetary
        );

    if (!consensus::money_range(
            subsidy,
            monetary) ||
        total_fees >
            monetary.max_money - subsidy) {
        out.error =
            BlockTemplateError::reward_overflow;
        return out;
    }

    const Amount miner_subsidy =
        consensus::miner_subsidy(
            height,
            monetary
        );

    if (total_fees >
        monetary.max_money -
            miner_subsidy) {
        out.error =
            BlockTemplateError::reward_overflow;
        return out;
    }

    Transaction coinbase;
    TxInput coinbase_input;
    append_little_endian(
        coinbase_input.unlocking_script,
        height
    );
    coinbase.inputs.push_back(
        std::move(coinbase_input)
    );

    TxOutput reward;
    reward.value =
        miner_subsidy + total_fees;
    reward.locking_script =
        payout_script;
    coinbase.outputs.push_back(
        std::move(reward)
    );

    const Amount founder_subsidy =
        consensus::founder_subsidy(
            height,
            monetary
        );

    if (founder_subsidy > 0U) {
        if (!monetary.founder_payout_enabled ||
            !crypto::is_valid_public_key(
                monetary.founder_public_key)) {
            out.error =
                BlockTemplateError::
                    founder_payout_unavailable;
            return out;
        }

        TxOutput founder;
        founder.value = founder_subsidy;
        founder.locking_script =
            consensus::make_p2pk_locking_script(
                monetary.founder_public_key
            );
        coinbase.outputs.push_back(
            std::move(founder)
        );
    }

    Block block;
    block.header.version = 1U;
    block.header.previous_block = *previous_hash;
    block.header.timestamp = candidate_timestamp;
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
