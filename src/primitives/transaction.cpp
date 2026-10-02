#include "primitives/transaction.hpp"

#include "crypto/sha256.hpp"

#include <algorithm>
#include <limits>

namespace quintum {
namespace {

bool checked_add(
    std::size_t& total,
    std::size_t value) noexcept
{
    if (value >
        std::numeric_limits<std::size_t>::max() - total) {
        return false;
    }
    total += value;
    return true;
}

void append_bytes(Bytes& out, const Bytes& bytes)
{
    append_compact_size(out, static_cast<std::uint64_t>(bytes.size()));
    out.insert(out.end(), bytes.begin(), bytes.end());
}

} // namespace

bool OutPoint::is_null() const noexcept
{
    return index == 0xffffffffU &&
           std::all_of(txid.begin(), txid.end(), [](Byte byte) { return byte == 0U; });
}

bool Transaction::is_coinbase() const noexcept
{
    return inputs.size() == 1U && inputs.front().previous_output.is_null();
}

std::optional<std::size_t> serialized_transaction_size(
    const Transaction& tx) noexcept
{
    std::size_t total = sizeof(tx.version);

    if (!checked_add(
            total,
            compact_size_serialized_size(
                static_cast<std::uint64_t>(tx.inputs.size())))) {
        return std::nullopt;
    }

    for (const auto& input : tx.inputs) {
        const std::size_t fixed =
            input.previous_output.txid.size() +
            sizeof(input.previous_output.index) +
            sizeof(input.sequence);

        if (!checked_add(total, fixed) ||
            !checked_add(
                total,
                compact_size_serialized_size(
                    static_cast<std::uint64_t>(
                        input.unlocking_script.size()))) ||
            !checked_add(
                total,
                input.unlocking_script.size())) {
            return std::nullopt;
        }
    }

    if (!checked_add(
            total,
            compact_size_serialized_size(
                static_cast<std::uint64_t>(tx.outputs.size())))) {
        return std::nullopt;
    }

    for (const auto& output : tx.outputs) {
        if (!checked_add(total, sizeof(output.value)) ||
            !checked_add(
                total,
                compact_size_serialized_size(
                    static_cast<std::uint64_t>(
                        output.locking_script.size()))) ||
            !checked_add(
                total,
                output.locking_script.size())) {
            return std::nullopt;
        }
    }

    if (!checked_add(total, sizeof(tx.lock_time))) {
        return std::nullopt;
    }

    return total;
}

Bytes serialize_transaction(const Transaction& tx)
{
    Bytes out;
    append_little_endian(out, tx.version);

    append_compact_size(out, static_cast<std::uint64_t>(tx.inputs.size()));
    for (const auto& input : tx.inputs) {
        out.insert(
            out.end(),
            input.previous_output.txid.begin(),
            input.previous_output.txid.end()
        );
        append_little_endian(out, input.previous_output.index);
        append_bytes(out, input.unlocking_script);
        append_little_endian(out, input.sequence);
    }

    append_compact_size(out, static_cast<std::uint64_t>(tx.outputs.size()));
    for (const auto& output : tx.outputs) {
        append_little_endian(out, output.value);
        append_bytes(out, output.locking_script);
    }

    append_little_endian(out, tx.lock_time);
    return out;
}

Hash256 transaction_id(const Transaction& tx)
{
    const auto bytes = serialize_transaction(tx);
    return crypto::double_sha256(bytes);
}

TxStructureError validate_transaction_structure(const Transaction& tx)
{
    if (tx.inputs.empty()) {
        return TxStructureError::no_inputs;
    }
    if (tx.outputs.empty()) {
        return TxStructureError::no_outputs;
    }

    for (std::size_t i = 0; i < tx.inputs.size(); ++i) {
        for (std::size_t j = i + 1U; j < tx.inputs.size(); ++j) {
            if (tx.inputs[i].previous_output == tx.inputs[j].previous_output) {
                return TxStructureError::duplicate_input;
            }
        }
    }

    Amount total{0};
    for (const auto& output : tx.outputs) {
        if (output.value > std::numeric_limits<Amount>::max() - total) {
            return TxStructureError::output_sum_overflow;
        }
        total += output.value;
    }

    return TxStructureError::none;
}

} // namespace quintum
