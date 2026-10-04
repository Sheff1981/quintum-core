#pragma once

#include "primitives/transaction.hpp"
#include "consensus/monetary.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <utility>
#include <vector>

namespace quintum {

class ChainstateStore;

struct Coin {
    TxOutput output{};
    std::uint32_t height{0};
    bool coinbase{false};
};

struct OutPointLess {
    bool operator()(const OutPoint& lhs, const OutPoint& rhs) const noexcept
    {
        if (lhs.txid < rhs.txid) {
            return true;
        }
        if (rhs.txid < lhs.txid) {
            return false;
        }
        return lhs.index < rhs.index;
    }
};

struct UtxoUndo {
    std::vector<std::pair<OutPoint, Coin>> spent{};
    std::vector<OutPoint> created{};
};

enum class UtxoApplyError {
    none,
    invalid_structure,
    missing_input,
    input_sum_overflow,
    money_out_of_range,
    premature_coinbase_spend,
    invalid_authorization,
    insufficient_input_value,
    output_collision,
};

struct UtxoApplyResult {
    UtxoApplyError error{UtxoApplyError::none};
    Amount fee{0};
    UtxoUndo undo{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == UtxoApplyError::none;
    }
};

class UtxoSet {
    friend class ChainstateStore;

public:
    [[nodiscard]] bool contains(const OutPoint& outpoint) const;
    [[nodiscard]] std::optional<Coin> get(const OutPoint& outpoint) const;
    [[nodiscard]] std::size_t size() const noexcept;

    [[nodiscard]] UtxoApplyResult apply_transaction(
        const Transaction& tx,
        std::uint32_t height
    );

    [[nodiscard]] UtxoApplyResult apply_transaction(
        const Transaction& tx,
        std::uint32_t height,
        const consensus::MonetaryParams& monetary
    );

    [[nodiscard]] bool undo_transaction(const UtxoUndo& undo);

private:
    std::map<OutPoint, Coin, OutPointLess> coins_;
};

} // namespace quintum
