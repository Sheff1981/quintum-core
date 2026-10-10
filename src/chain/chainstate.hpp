#pragma once

#include "chain/utxo.hpp"
#include "consensus/block_limits.hpp"
#include "consensus/chainparams.hpp"
#include "core/types.hpp"
#include "primitives/block.hpp"

#include <cstddef>
#include <functional>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <vector>

namespace quintum {

class ChainstateStore;

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
    std::optional<Block> block{};
    BlockHeader header{};
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
    wrong_genesis,
    unknown_parent,
    duplicate_block,
    invalid_ancestor,
    invalid_proof_of_work,
    unexpected_difficulty,
    timestamp_too_old,
    timestamp_too_far_future,
    resource_limits_exceeded,
    chain_work_overflow,
    height_overflow,
    transaction_failed,
    fee_sum_overflow,
    invalid_coinbase_reward,
    reorg_undo_failed,
    block_body_unavailable,
    validation_cancelled,
};

struct ChainConnectResult {
    ChainConnectError error{ChainConnectError::none};
    UtxoApplyError transaction_error{UtxoApplyError::none};
    consensus::BlockResourceError resource_error{
        consensus::BlockResourceError::none
    };
    std::size_t transaction_index{0};
    Amount total_fees{0};
    bool activated{false};
    bool reorganized{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == ChainConnectError::none;
    }
};

struct HeaderValidationResult {
    ChainConnectError error{ChainConnectError::none};
    std::size_t header_index{0U};
    Hash256 chain_work{};

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
    friend class ChainstateStore;

public:
    explicit Chainstate(const consensus::ChainParams& params);

    [[nodiscard]] const consensus::ChainParams& params() const noexcept;
    [[nodiscard]] bool empty() const noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::size_t block_index_size() const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> height() const noexcept;
    [[nodiscard]] std::optional<Hash256> tip_hash() const;
    [[nodiscard]] std::optional<std::uint64_t> tip_timestamp() const noexcept
    {
        if (chain_.empty() ||
            chain_.back().header.timestamp ==
                std::numeric_limits<std::uint64_t>::max()) {
            return std::nullopt;
        }
        return chain_.back().header.timestamp;
    }
    [[nodiscard]] Hash256 cumulative_work() const noexcept;
    [[nodiscard]] const UtxoSet& utxos() const noexcept;
    [[nodiscard]] bool has_block(const Hash256& hash) const;
    [[nodiscard]] bool has_block_body(const Hash256& hash) const noexcept;
    [[nodiscard]] bool is_on_active_chain(const Hash256& hash) const;
    [[nodiscard]] std::optional<Hash256> active_hash(
        std::uint32_t height
    ) const;
    [[nodiscard]] std::optional<BlockHeader> active_header(
        std::uint32_t height
    ) const;
    [[nodiscard]] std::optional<std::uint32_t> active_height(
        const Hash256& hash
    ) const;
    [[nodiscard]] const Block* block(
        const Hash256& hash
    ) const noexcept;
    [[nodiscard]] std::optional<std::uint32_t> next_work_required(
        std::uint64_t candidate_timestamp
    ) const;
    [[nodiscard]] std::optional<Hash256>
    next_randomx_seed_key() const;

    // Validates a contiguous headers-first batch against the same consensus
    // difficulty, time and PoW rules used by connect_block(), without
    // mutating chainstate or requiring block bodies.
    [[nodiscard]] HeaderValidationResult validate_headers(
        std::span<const BlockHeader> headers,
        std::uint64_t adjusted_time,
        const std::function<bool(std::size_t, bool)>& progress = {}
    ) const;

    // Copies only validated header metadata, never UTXOs, bodies or undo.
    [[nodiscard]] Chainstate header_validation_snapshot() const;

    // Restores the body of an already-known block whose metadata survived
    // pruning. This does not change the active chain by itself.
    [[nodiscard]] ChainConnectResult restore_block_body(
        const Block& block
    );

    // Accepts active-tip extensions and side-branch blocks. A side branch is
    // activated only when its cumulative valid work becomes strictly greater.
    [[nodiscard]] ChainConnectResult connect_block(const Block& block);
    [[nodiscard]] ChainConnectResult connect_block(
        const Block& block,
        std::uint64_t adjusted_time
    );

    [[nodiscard]] ChainDisconnectError disconnect_tip();

private:
    using HeaderIndexOverlay =
        std::map<Hash256, BlockIndexEntry>;

    [[nodiscard]] const BlockIndexEntry* find_index_entry(
        const Hash256& hash,
        const HeaderIndexOverlay* overlay = nullptr
    ) const noexcept;
    [[nodiscard]] bool has_failed_ancestor(
        const Hash256& hash,
        const HeaderIndexOverlay* overlay = nullptr
    ) const;
    [[nodiscard]] std::optional<std::uint32_t> expected_bits(
        const Block& block,
        const BlockIndexEntry* parent,
        const HeaderIndexOverlay* overlay = nullptr
    ) const;
    [[nodiscard]] std::optional<std::uint64_t> median_time_past(
        const BlockIndexEntry* parent,
        const HeaderIndexOverlay* overlay = nullptr
    ) const;
    [[nodiscard]] std::optional<Hash256>
    randomx_seed_key_for(
        const BlockIndexEntry* parent,
        std::uint32_t candidate_height,
        const HeaderIndexOverlay* overlay = nullptr
    ) const;
    [[nodiscard]] HeaderValidationResult
    validate_header_candidate(
        const BlockHeader& header,
        const BlockIndexEntry* parent,
        std::uint32_t height,
        std::uint64_t adjusted_time,
        const HeaderIndexOverlay* overlay = nullptr,
        bool verify_pow = true
    ) const;

    // Disk snapshots are only written after normal consensus validation.
    // Startup replay can therefore re-check structure, difficulty, monetary
    // rules, transactions, UTXO/undo and chainwork without repeating the
    // expensive RandomX hash for every already-accepted block.
    [[nodiscard]] ChainConnectResult connect_validated_snapshot_block(
        const Block& block,
        std::uint64_t adjusted_time
    );

    [[nodiscard]] ChainConnectResult connect_block_impl(
        const Block& block,
        std::uint64_t adjusted_time,
        bool verify_pow
    );

    consensus::ChainParams params_{};
    UtxoSet utxos_{};
    std::vector<ChainEntry> chain_{};
    std::map<Hash256, BlockIndexEntry> block_index_{};
    std::vector<Hash256> acceptance_order_{};
};

} // namespace quintum
