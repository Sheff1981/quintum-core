#pragma once

#include "chain/utxo.hpp"
#include "core/types.hpp"
#include "primitives/block.hpp"

#include <cstddef>
#include <cstdint>
#include <map>
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

struct BlockIndexEntry {
    Block block{};
    Hash256 hash{};
    Hash256 parent{};
    std::uint32_t height{0};
    Hash256 chain_work{};
    bool failed{false};
};

enum class ChainConnectError {
    none,
    invalid_block_structure,
    bad_previous_block,
    unknown_parent,
    duplicate_block,
    invalid_ancestor,
    invalid_proof_of_work,
    chain_work_overflow,
    height_overflow,
    transaction_failed,
    fee_sum_overflow,
    invalid_coinbase_reward,
    reorg_undo_failed,
};

struct ChainConnectResult {
    ChainConnectError error{ChainConnectError::none};
    UtxoApplyError transaction_error{UtxoApplyError::none};
    std::size_t transaction_index{0};
    Amount total_fees{0};
    bool activated{false};
    bool reorganized{false};

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
    [[nodiscard]] std::size_t block_index_size() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> height() const noexcept;
    [[nodiscard]] std::optional<Hash256> tip_hash() const;
    [[nodiscard]] Hash256 cumulative_work() const noexcept;
    [[nodiscard]] const UtxoSet& utxos() const noexcept;
    [[nodiscard]] bool has_block(const Hash256& hash) const;
    [[nodiscard]] bool is_on_active_chain(const Hash256& hash) const;

    // Accepts active-tip extensions and side-branch blocks. A side branch is
    // activated only when its cumulative valid work becomes strictly greater.
    [[nodiscard]] ChainConnectResult connect_block(const Block& block);

    [[nodiscard]] ChainDisconnectError disconnect_tip();

private:
    [[nodiscard]] bool has_failed_ancestor(const Hash256& hash) const;

    UtxoSet utxos_{};
    std::vector<ChainEntry> chain_{};
    std::map<Hash256, BlockIndexEntry> block_index_{};
};

} // namespace quintum
