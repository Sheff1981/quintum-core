#pragma once

#include "core/serialize.hpp"
#include "core/types.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quintum {

using Amount = std::uint64_t;

struct OutPoint {
    Hash256 txid{};
    std::uint32_t index{0xffffffffU};

    [[nodiscard]] bool is_null() const noexcept;
    bool operator==(const OutPoint&) const = default;
};

struct TxInput {
    OutPoint previous_output{};
    Bytes unlocking_script{};
    std::uint32_t sequence{0xffffffffU};
};

struct TxOutput {
    Amount value{0};
    Bytes locking_script{};
};

struct Transaction {
    std::uint32_t version{1U};
    std::vector<TxInput> inputs{};
    std::vector<TxOutput> outputs{};
    std::uint32_t lock_time{0U};

    [[nodiscard]] bool is_coinbase() const noexcept;
};

enum class TxStructureError {
    none,
    no_inputs,
    no_outputs,
    duplicate_input,
    output_sum_overflow,
};

[[nodiscard]] std::optional<std::size_t> serialized_transaction_size(
    const Transaction& tx
) noexcept;
[[nodiscard]] Bytes serialize_transaction(const Transaction& tx);
[[nodiscard]] Hash256 transaction_id(const Transaction& tx);
[[nodiscard]] TxStructureError validate_transaction_structure(const Transaction& tx);

} // namespace quintum
