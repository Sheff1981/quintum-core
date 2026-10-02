#pragma once

#include "chain/utxo.hpp"
#include "core/types.hpp"
#include "primitives/block.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace quintum {

struct BlockUndo {
    std::vector<UtxoUndo> transactions{};
};

struct ChainEntry {
    Hash256 hash{};
    BlockHeader header{};
    std::uint32_t height{0};
    Hash256 chain_work{};
    BlockUndo undo{};
};

enum class ChainConnectError {
    none,
    invalid_block_structure,
    bad_previous_block,
    invalid_proof_of_work,
    chain_work_overflow,
    transaction_failed,
    fee_sum_overflow,
};

struct ChainConnectResult {
    ChainConnectError error{ChainConnectError::none};
    UtxoApplyError transaction_error{UtxoApplyError::none};
    std::size_t transaction_index{0};
    Amount total_fees{0};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == ChainConnectError::none;
    }
};

enum class ChainDisconnectError {
    none,
    empty_chain,
    undo_failed,
};

class Chainstate {
public:
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> height() const noexcept;
    [[nodiscard]] std::optional<Hash256> tip_hash() const;
    [[nodiscard]] Hash256 cumulative_work() const noexcept;
    [[nodiscard]] const UtxoSet& utxos() const noexcept;

    [[nodiscard]] ChainConnectResult connect_block(const Block& block);
    [[nodiscard]] ChainDisconnectError disconnect_tip();

private:
    UtxoSet utxos_{};
    std::vector<ChainEntry> chain_{};
};

} // namespace quintum
