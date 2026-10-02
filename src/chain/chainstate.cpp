#include "chain/chainstate.hpp"

#include "consensus/block_limits.hpp"
#include "consensus/difficulty.hpp"
#include "consensus/genesis.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/time.hpp"

#include <algorithm>
#include <chrono>
#include <limits>
#include <utility>
#include <vector>

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

bool greater_work(const Hash256& lhs, const Hash256& rhs) noexcept
{
    return std::lexicographical_compare(
        rhs.begin(), rhs.end(),
        lhs.begin(), lhs.end()
    );
}

struct ApplyBlockResult {
    ChainConnectResult result{};
    ChainEntry entry{};
};

ApplyBlockResult apply_block_to_view(
    const Block& block,
    UtxoSet& utxos,
    std::uint32_t height,
    const Hash256& parent_work,
    const consensus::PowParams& pow_params,
    const consensus::ResourceLimits& limits)
{
    ApplyBlockResult out;

    if (validate_block_structure(block) != BlockStructureError::none) {
        out.result.error = ChainConnectError::invalid_block_structure;
        return out;
    }

    const auto resource_error =
        consensus::validate_block_resources(block, limits);

    if (resource_error != consensus::BlockResourceError::none) {
        out.result.error =
            ChainConnectError::resource_limits_exceeded;
        out.result.resource_error = resource_error;
        return out;
    }

    if (consensus::check_proof_of_work(
            block.header,
            pow_params) !=
        consensus::PowCheckError::none) {
        out.result.error = ChainConnectError::invalid_proof_of_work;
        return out;
    }

    const auto compact = consensus::decode_compact_target(block.header.bits);
    const auto block_work = consensus::work_for_target(compact.target);

    Hash256 chain_work = parent_work;
    if (!consensus::add_chain_work(chain_work, block_work)) {
        out.result.error = ChainConnectError::chain_work_overflow;
        return out;
    }

    UtxoSet candidate = utxos;
    BlockUndo block_undo;
    block_undo.transactions.reserve(block.transactions.size());

    Amount total_fees{0};

    for (std::size_t i = 0; i < block.transactions.size(); ++i) {
        auto tx_result =
            candidate.apply_transaction(block.transactions[i], height);

        if (!tx_result.ok()) {
            out.result.error = ChainConnectError::transaction_failed;
            out.result.transaction_error = tx_result.error;
            out.result.transaction_index = i;
            return out;
        }

        if (tx_result.fee >
            std::numeric_limits<Amount>::max() - total_fees) {
            out.result.error = ChainConnectError::fee_sum_overflow;
            out.result.transaction_index = i;
            return out;
        }

        total_fees += tx_result.fee;
        block_undo.transactions.push_back(std::move(tx_result.undo));
    }

    if (!consensus::coinbase_reward_is_valid(
            block.transactions.front(),
            height,
            total_fees)) {
        out.result.error = ChainConnectError::invalid_coinbase_reward;
        return out;
    }

    utxos = std::move(candidate);

    out.entry = ChainEntry{
        .hash = block_hash(block.header),
        .header = block.header,
        .height = height,
        .chain_work = chain_work,
        .undo = std::move(block_undo),
    };

    out.result.total_fees = total_fees;
    return out;
}

} // namespace

Chainstate::Chainstate(
    const consensus::ChainParams& params)
    : params_(params)
{
}

const consensus::ChainParams& Chainstate::params() const noexcept
{
    return params_;
}

bool Chainstate::empty() const noexcept
{
    return chain_.empty();
}

std::size_t Chainstate::size() const noexcept
{
    return chain_.size();
}

std::size_t Chainstate::block_index_size() const noexcept
{
    return block_index_.size();
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

bool Chainstate::has_block(const Hash256& hash) const
{
    return block_index_.contains(hash);
}

bool Chainstate::is_on_active_chain(const Hash256& hash) const
{
    return std::any_of(
        chain_.begin(),
        chain_.end(),
        [&hash](const ChainEntry& entry) {
            return entry.hash == hash;
        }
    );
}

std::optional<std::uint32_t> Chainstate::next_work_required(
    std::uint64_t candidate_timestamp) const
{
    Block candidate;
    candidate.header.timestamp = candidate_timestamp;

    if (chain_.empty()) {
        return expected_bits(candidate, nullptr);
    }

    const auto it =
        block_index_.find(chain_.back().hash);

    if (it == block_index_.end()) {
        return std::nullopt;
    }

    return expected_bits(candidate, &it->second);
}

bool Chainstate::has_failed_ancestor(const Hash256& hash) const
{
    auto it = block_index_.find(hash);

    while (it != block_index_.end()) {
        if (it->second.failed) {
            return true;
        }

        if (it->second.height == 0U) {
            break;
        }

        it = block_index_.find(it->second.parent);
    }

    return false;
}

std::optional<std::uint64_t> Chainstate::median_time_past(
    const BlockIndexEntry* parent) const
{
    if (parent == nullptr ||
        params_.time.median_time_span == 0U) {
        return std::nullopt;
    }

    std::vector<std::uint64_t> timestamps;
    timestamps.reserve(
        static_cast<std::size_t>(
            params_.time.median_time_span));

    const BlockIndexEntry* cursor = parent;

    for (std::uint32_t count = 0U;
         count < params_.time.median_time_span;
         ++count) {
        timestamps.push_back(
            cursor->block.header.timestamp);

        if (cursor->height == 0U) {
            break;
        }

        const auto it =
            block_index_.find(cursor->parent);

        if (it == block_index_.end()) {
            return std::nullopt;
        }

        cursor = &it->second;
    }

    return consensus::median_timestamp(timestamps);
}

std::optional<std::uint32_t> Chainstate::expected_bits(
    const Block& block,
    const BlockIndexEntry* parent) const
{
    const auto& pow = params_.pow;

    if (parent == nullptr) {
        return params_.genesis.enforce
            ? params_.genesis.bits
            : pow.pow_limit_bits;
    }

    if (pow.no_retargeting) {
        return parent->block.header.bits;
    }

    if (pow.retarget_interval == 0U ||
        pow.target_spacing_seconds == 0U) {
        return std::nullopt;
    }

    if (parent->height ==
        std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }

    const std::uint32_t next_height =
        parent->height + 1U;

    if ((next_height % pow.retarget_interval) != 0U) {
        if (pow.allow_min_difficulty_blocks) {
            const std::uint64_t delay =
                pow.target_spacing_seconds >
                        std::numeric_limits<std::uint64_t>::max() / 2U
                    ? std::numeric_limits<std::uint64_t>::max()
                    : pow.target_spacing_seconds * 2U;

            const std::uint64_t threshold =
                parent->block.header.timestamp >
                        std::numeric_limits<std::uint64_t>::max() - delay
                    ? std::numeric_limits<std::uint64_t>::max()
                    : parent->block.header.timestamp + delay;

            if (block.header.timestamp > threshold) {
                return pow.pow_limit_bits;
            }

            const BlockIndexEntry* cursor = parent;

            while ((cursor->height % pow.retarget_interval) != 0U &&
                   cursor->block.header.bits ==
                       pow.pow_limit_bits) {
                const auto it =
                    block_index_.find(cursor->parent);

                if (it == block_index_.end()) {
                    return std::nullopt;
                }

                cursor = &it->second;
            }

            return cursor->block.header.bits;
        }

        return parent->block.header.bits;
    }

    const BlockIndexEntry* first = parent;

    for (std::uint32_t step = 1U;
         step < pow.retarget_interval;
         ++step) {
        const auto it =
            block_index_.find(first->parent);

        if (it == block_index_.end()) {
            return std::nullopt;
        }

        first = &it->second;
    }

    const auto retarget =
        consensus::calculate_retarget_bits(
            parent->block.header.bits,
            first->block.header.timestamp,
            parent->block.header.timestamp,
            pow
        );

    if (!retarget.ok()) {
        return std::nullopt;
    }

    return retarget.bits;
}

ChainConnectResult Chainstate::connect_block(
    const Block& block)
{
    const auto now =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now()
                .time_since_epoch())
            .count();

    const auto adjusted_time =
        now < 0 ? 0U :
        static_cast<std::uint64_t>(now);

    return connect_block(block, adjusted_time);
}

ChainConnectResult Chainstate::connect_block(
    const Block& block,
    std::uint64_t adjusted_time)
{
    ChainConnectResult result;

    if (validate_block_structure(block) != BlockStructureError::none) {
        result.error = ChainConnectError::invalid_block_structure;
        return result;
    }

    const auto resource_error =
        consensus::validate_block_resources(
            block,
            params_.limits);

    if (resource_error != consensus::BlockResourceError::none) {
        result.error =
            ChainConnectError::resource_limits_exceeded;
        result.resource_error = resource_error;
        return result;
    }

    if (!consensus::timestamp_not_too_far_future(
            block.header.timestamp,
            adjusted_time,
            params_.time.max_future_seconds)) {
        result.error =
            ChainConnectError::timestamp_too_far_future;
        return result;
    }

    const auto hash = block_hash(block.header);

    if (block_index_.contains(hash)) {
        result.error = ChainConnectError::duplicate_block;
        return result;
    }

    std::uint32_t new_height{0U};
    Hash256 parent_work{};
    const BlockIndexEntry* parent_entry{nullptr};

    if (block_index_.empty()) {
        if (params_.genesis.enforce) {
            if (!consensus::verify_genesis(params_) ||
                hash != params_.genesis.hash) {
                result.error =
                    ChainConnectError::wrong_genesis;
                return result;
            }
        } else if (!is_zero_hash(
                       block.header.previous_block)) {
            result.error =
                ChainConnectError::bad_previous_block;
            return result;
        }
    } else {
        const auto parent_it =
            block_index_.find(block.header.previous_block);

        if (parent_it == block_index_.end()) {
            result.error = ChainConnectError::unknown_parent;
            return result;
        }

        if (has_failed_ancestor(parent_it->first)) {
            result.error = ChainConnectError::invalid_ancestor;
            return result;
        }

        if (parent_it->second.height ==
            std::numeric_limits<std::uint32_t>::max()) {
            result.error = ChainConnectError::height_overflow;
            return result;
        }

        new_height = parent_it->second.height + 1U;
        parent_work = parent_it->second.chain_work;
        parent_entry = &parent_it->second;
    }

    if (parent_entry != nullptr) {
        const auto mtp =
            median_time_past(parent_entry);

        if (!mtp ||
            block.header.timestamp <= *mtp) {
            result.error =
                ChainConnectError::timestamp_too_old;
            return result;
        }
    }

    const auto required_bits =
        expected_bits(block, parent_entry);

    if (!required_bits ||
        block.header.bits != *required_bits) {
        result.error = ChainConnectError::unexpected_difficulty;
        return result;
    }

    if (consensus::check_proof_of_work(
            block.header,
            params_.pow) !=
        consensus::PowCheckError::none) {
        result.error = ChainConnectError::invalid_proof_of_work;
        return result;
    }

    const auto compact = consensus::decode_compact_target(block.header.bits);
    const auto block_work = consensus::work_for_target(compact.target);

    Hash256 new_chain_work = parent_work;
    if (!consensus::add_chain_work(new_chain_work, block_work)) {
        result.error = ChainConnectError::chain_work_overflow;
        return result;
    }

    block_index_.emplace(
        hash,
        BlockIndexEntry{
            .block = block,
            .hash = hash,
            .parent = block.header.previous_block,
            .height = new_height,
            .chain_work = new_chain_work,
            .failed = false,
        }
    );

    const bool should_activate =
        chain_.empty() ||
        greater_work(new_chain_work, cumulative_work());

    if (!should_activate) {
        return result;
    }

    // Find the common ancestor between the candidate branch and active chain.
    Hash256 candidate_cursor = hash;
    std::uint32_t candidate_height = new_height;

    std::size_t active_size = chain_.size();

    while (active_size > 0U &&
           chain_[active_size - 1U].height > candidate_height) {
        --active_size;
    }

    while (candidate_height >
           (active_size == 0U ? 0U : chain_[active_size - 1U].height)) {
        const auto it = block_index_.find(candidate_cursor);
        if (it == block_index_.end() || it->second.height == 0U) {
            break;
        }
        candidate_cursor = it->second.parent;
        --candidate_height;
    }

    while (active_size > 0U &&
           chain_[active_size - 1U].hash != candidate_cursor) {
        --active_size;

        const auto it = block_index_.find(candidate_cursor);
        if (it == block_index_.end() || it->second.height == 0U) {
            candidate_cursor = {};
            break;
        }

        candidate_cursor = it->second.parent;
    }

    const std::size_t fork_size = active_size;
    const Hash256 fork_hash =
        fork_size == 0U ? Hash256{} : chain_[fork_size - 1U].hash;

    // Build candidate path from fork child to new tip.
    std::vector<Hash256> forward_path;
    Hash256 walk = hash;

    while (walk != fork_hash) {
        const auto it = block_index_.find(walk);
        if (it == block_index_.end()) {
            block_index_[hash].failed = true;
            result.error = ChainConnectError::invalid_ancestor;
            return result;
        }

        forward_path.push_back(walk);

        if (it->second.height == 0U) {
            walk = it->second.parent;
            break;
        }

        walk = it->second.parent;
    }

    if (walk != fork_hash) {
        block_index_[hash].failed = true;
        result.error = ChainConnectError::invalid_ancestor;
        return result;
    }

    std::reverse(forward_path.begin(), forward_path.end());

    // Stage the whole reorg. The live active chain is untouched until success.
    UtxoSet staged_utxos = utxos_;
    std::vector<ChainEntry> staged_chain = chain_;

    while (staged_chain.size() > fork_size) {
        const auto& undo = staged_chain.back().undo.transactions;

        for (auto it = undo.rbegin(); it != undo.rend(); ++it) {
            if (!staged_utxos.undo_transaction(*it)) {
                result.error = ChainConnectError::reorg_undo_failed;
                return result;
            }
        }

        staged_chain.pop_back();
    }

    Amount activated_fees{0U};

    for (const auto& block_hash_value : forward_path) {
        auto index_it = block_index_.find(block_hash_value);
        if (index_it == block_index_.end()) {
            result.error = ChainConnectError::invalid_ancestor;
            return result;
        }

        if (index_it->second.failed) {
            result.error = ChainConnectError::invalid_ancestor;
            return result;
        }

        const Hash256 staged_parent_work =
            staged_chain.empty()
                ? Hash256{}
                : staged_chain.back().chain_work;

        const std::uint32_t staged_height =
            staged_chain.empty()
                ? 0U
                : staged_chain.back().height + 1U;

        auto applied = apply_block_to_view(
            index_it->second.block,
            staged_utxos,
            staged_height,
            staged_parent_work,
            params_.pow,
            params_.limits
        );

        if (!applied.result.ok()) {
            index_it->second.failed = true;
            result = applied.result;
            return result;
        }

        if (applied.result.total_fees >
            std::numeric_limits<Amount>::max() - activated_fees) {
            index_it->second.failed = true;
            result.error = ChainConnectError::fee_sum_overflow;
            return result;
        }

        activated_fees += applied.result.total_fees;
        staged_chain.push_back(std::move(applied.entry));
    }

    const bool was_reorg =
        !chain_.empty() &&
        fork_size < chain_.size();

    utxos_ = std::move(staged_utxos);
    chain_ = std::move(staged_chain);

    result.total_fees = activated_fees;
    result.activated = true;
    result.reorganized = was_reorg;
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
