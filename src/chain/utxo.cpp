#include "chain/utxo.hpp"

#include <limits>

namespace quintum {

bool UtxoSet::contains(const OutPoint& outpoint) const
{
    return coins_.contains(outpoint);
}

std::optional<Coin> UtxoSet::get(const OutPoint& outpoint) const
{
    const auto it = coins_.find(outpoint);
    if (it == coins_.end()) {
        return std::nullopt;
    }
    return it->second;
}

std::size_t UtxoSet::size() const noexcept
{
    return coins_.size();
}

UtxoApplyResult UtxoSet::apply_transaction(
    const Transaction& tx,
    std::uint32_t height)
{
    UtxoApplyResult result;

    if (validate_transaction_structure(tx) != TxStructureError::none) {
        result.error = UtxoApplyError::invalid_structure;
        return result;
    }

    const bool is_coinbase = tx.is_coinbase();
    Amount input_total{0};

    if (!is_coinbase) {
        result.undo.spent.reserve(tx.inputs.size());

        for (const auto& input : tx.inputs) {
            const auto it = coins_.find(input.previous_output);
            if (it == coins_.end()) {
                result.error = UtxoApplyError::missing_input;
                return result;
            }

            if (it->second.output.value >
                std::numeric_limits<Amount>::max() - input_total) {
                result.error = UtxoApplyError::input_sum_overflow;
                return result;
            }

            input_total += it->second.output.value;
            result.undo.spent.emplace_back(input.previous_output, it->second);
        }
    }

    Amount output_total{0};
    for (const auto& output : tx.outputs) {
        // validate_transaction_structure() already guarantees this cannot overflow.
        output_total += output.value;
    }

    if (!is_coinbase && input_total < output_total) {
        result.error = UtxoApplyError::insufficient_input_value;
        return result;
    }

    const auto txid = transaction_id(tx);
    result.undo.created.reserve(tx.outputs.size());

    for (std::size_t i = 0; i < tx.outputs.size(); ++i) {
        const OutPoint outpoint{
            .txid = txid,
            .index = static_cast<std::uint32_t>(i),
        };

        if (coins_.contains(outpoint)) {
            result.error = UtxoApplyError::output_collision;
            return result;
        }

        result.undo.created.push_back(outpoint);
    }

    // Mutation starts only after every validation above succeeded.
    if (!is_coinbase) {
        for (const auto& input : tx.inputs) {
            coins_.erase(input.previous_output);
        }
        result.fee = input_total - output_total;
    }

    for (std::size_t i = 0; i < tx.outputs.size(); ++i) {
        coins_.emplace(
            result.undo.created[i],
            Coin{
                .output = tx.outputs[i],
                .height = height,
                .coinbase = is_coinbase,
            }
        );
    }

    return result;
}

bool UtxoSet::undo_transaction(const UtxoUndo& undo)
{
    for (const auto& outpoint : undo.created) {
        if (!coins_.contains(outpoint)) {
            return false;
        }
    }

    for (const auto& [outpoint, coin] : undo.spent) {
        (void)coin;
        if (coins_.contains(outpoint)) {
            return false;
        }
    }

    for (const auto& outpoint : undo.created) {
        coins_.erase(outpoint);
    }

    for (const auto& [outpoint, coin] : undo.spent) {
        coins_.emplace(outpoint, coin);
    }

    return true;
}

} // namespace quintum
