#include "chain/chainstate.hpp"

#include "consensus/pow.hpp"

#include <algorithm>
#include <limits>
#include <utility>

namespace quintum {
namespace {

bool is_zero_hash(const Hash256& hash) noexcept
{
    return std::all_of(
        hash.begin(),
        hash.end(),
        [](Byte byte) { return byte == 0U; }
    );
}

} // namespace

bool Chainstate::empty() const noexcept
{
    return chain_.empty();
}

std::size_t Chainstate::size() const noexcept
{
    return chain_.size();
}

std::optional<std::uint32_t> Chainstate::height() const noexcept
{
    if (chain_.empty()) {
        return std::nullopt;
    }
    return chain_.back().height;
}

std::optional<Hash256> Chainstate::tip_hash() const
{
    if (chain_.empty()) {
        return std::nullopt;
    }
    return chain_.back().hash;
}

Hash256 Chainstate::cumulative_work() const noexcept
{
    if (chain_.empty()) {
        return {};
    }
    return chain_.back().chain_work;
}

const UtxoSet& Chainstate::utxos() const noexcept
{
    return utxos_;
}

ChainConnectResult Chainstate::connect_block(const Block& block)
{
    ChainConnectResult result;

    if (validate_block_structure(block) != BlockStructureError::none) {
        result.error = ChainConnectError::invalid_block_structure;
        return result;
    }

    if (chain_.empty()) {
        if (!is_zero_hash(block.header.previous_block)) {
            result.error = ChainConnectError::bad_previous_block;
            return result;
        }
    } else if (block.header.previous_block != chain_.back().hash) {
        result.error = ChainConnectError::bad_previous_block;
        return result;
    }

    if (consensus::check_proof_of_work(block.header) !=
        consensus::PowCheckError::none) {
        result.error = ChainConnectError::invalid_proof_of_work;
        return result;
    }

    const auto compact = consensus::decode_compact_target(block.header.bits);
    const auto block_work = consensus::work_for_target(compact.target);

    Hash256 new_chain_work = cumulative_work();
    if (!consensus::add_chain_work(new_chain_work, block_work)) {
        result.error = ChainConnectError::chain_work_overflow;
        return result;
    }

    const std::uint32_t next_height =
        chain_.empty() ? 0U : chain_.back().height + 1U;

    // Work on a copy. A failed block can never partially alter live chainstate.
    UtxoSet candidate = utxos_;
    BlockUndo block_undo;
    block_undo.transactions.reserve(block.transactions.size());

    Amount total_fees{0};

    for (std::size_t i = 0; i < block.transactions.size(); ++i) {
        auto tx_result =
            candidate.apply_transaction(block.transactions[i], next_height);

        if (!tx_result.ok()) {
            result.error = ChainConnectError::transaction_failed;
            result.transaction_error = tx_result.error;
            result.transaction_index = i;
            return result;
        }

        if (tx_result.fee >
            std::numeric_limits<Amount>::max() - total_fees) {
            result.error = ChainConnectError::fee_sum_overflow;
            result.transaction_index = i;
            return result;
        }

        total_fees += tx_result.fee;
        block_undo.transactions.push_back(std::move(tx_result.undo));
    }

    const auto hash = block_hash(block.header);

    utxos_ = std::move(candidate);
    chain_.push_back(ChainEntry{
        .hash = hash,
        .header = block.header,
        .height = next_height,
        .chain_work = new_chain_work,
        .undo = std::move(block_undo),
    });

    result.total_fees = total_fees;
    return result;
}

ChainDisconnectError Chainstate::disconnect_tip()
{
    if (chain_.empty()) {
        return ChainDisconnectError::empty_chain;
    }

    UtxoSet candidate = utxos_;
    const auto& undo = chain_.back().undo.transactions;

    for (auto it = undo.rbegin(); it != undo.rend(); ++it) {
        if (!candidate.undo_transaction(*it)) {
            return ChainDisconnectError::undo_failed;
        }
    }

    utxos_ = std::move(candidate);
    chain_.pop_back();
    return ChainDisconnectError::none;
}

} // namespace quintum
