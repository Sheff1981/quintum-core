#include "chain/chainstate.hpp"

#include "consensus/block_limits.hpp"
#include "consensus/difficulty.hpp"
#include "consensus/genesis.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/randomx_seed.hpp"
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
    const consensus::MonetaryParams& monetary_params,
    const consensus::ResourceLimits& limits,
    const std::optional<Hash256>& randomx_seed_key,
    bool verify_pow)
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

    if (verify_pow) {
        const auto pow_error =
            pow_params.pow_algorithm ==
                    consensus::PowAlgorithm::randomx_v2
                ? randomx_seed_key
                      ? consensus::check_randomx_proof_of_work(
                            block.header,
                            pow_params,
                            *randomx_seed_key
                        )
                      : consensus::PowCheckError::hashing_failed
                : consensus::check_proof_of_work(
                      block.header,
                      pow_params
                  );

        if (pow_error !=
            consensus::PowCheckError::none) {
            out.result.error =
                ChainConnectError::invalid_proof_of_work;
            return out;
        }
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
            candidate.apply_transaction(
                block.transactions[i],
                height,
                monetary_params
            );

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
            total_fees,
            monetary_params)) {
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

bool Chainstate::has_block_body(
    const Hash256& hash) const noexcept
{
    const auto it = block_index_.find(hash);
    return it != block_index_.end() &&
           it->second.block.has_value();
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

std::optional<Hash256> Chainstate::active_hash(
    std::uint32_t height) const
{
    const auto index = static_cast<std::size_t>(height);
    if (index >= chain_.size() ||
        chain_[index].height != height) {
        return std::nullopt;
    }
    return chain_[index].hash;
}

std::optional<BlockHeader> Chainstate::active_header(
    std::uint32_t height) const
{
    const auto index = static_cast<std::size_t>(height);
    if (index >= chain_.size() ||
        chain_[index].height != height) {
        return std::nullopt;
    }
    return chain_[index].header;
}

std::optional<std::uint32_t> Chainstate::active_height(
    const Hash256& hash) const
{
    const auto it = std::find_if(
        chain_.begin(),
        chain_.end(),
        [&](const ChainEntry& entry) {
            return entry.hash == hash;
        }
    );

    return it == chain_.end()
        ? std::nullopt
        : std::optional<std::uint32_t>{it->height};
}

const Block* Chainstate::block(
    const Hash256& hash) const noexcept
{
    const auto it = block_index_.find(hash);
    return it == block_index_.end() ||
            !it->second.block
        ? nullptr
        : &*it->second.block;
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

const BlockIndexEntry* Chainstate::find_index_entry(
    const Hash256& hash,
    const HeaderIndexOverlay* overlay) const noexcept
{
    if (overlay != nullptr) {
        const auto staged = overlay->find(hash);
        if (staged != overlay->end()) {
            return &staged->second;
        }
    }

    const auto persisted = block_index_.find(hash);
    return persisted == block_index_.end()
        ? nullptr
        : &persisted->second;
}

std::optional<Hash256>
Chainstate::randomx_seed_key_for(
    const BlockIndexEntry* parent,
    std::uint32_t candidate_height,
    const HeaderIndexOverlay* overlay) const
{
    if (candidate_height == 0U) {
        const Hash256 bootstrap_hash{};
        return consensus::randomx_seed_key(
            0U,
            bootstrap_hash
        );
    }

    if (parent == nullptr) {
        return std::nullopt;
    }

    const std::uint64_t seed_height =
        consensus::randomx_seed_height(
            candidate_height
        );

    if (seed_height >
        static_cast<std::uint64_t>(
            parent->height)) {
        return std::nullopt;
    }

    const BlockIndexEntry* cursor = parent;

    while (static_cast<std::uint64_t>(
               cursor->height) >
           seed_height) {
        cursor =
            find_index_entry(
                cursor->parent,
                overlay
            );

        if (cursor == nullptr) {
            return std::nullopt;
        }
    }

    if (static_cast<std::uint64_t>(
            cursor->height) !=
        seed_height) {
        return std::nullopt;
    }

    return consensus::randomx_seed_key(
        seed_height,
        cursor->hash
    );
}

std::optional<Hash256>
Chainstate::next_randomx_seed_key() const
{
    if (chain_.empty()) {
        return randomx_seed_key_for(
            nullptr,
            0U
        );
    }

    if (chain_.back().height ==
        std::numeric_limits<std::uint32_t>::max()) {
        return std::nullopt;
    }

    const auto it =
        block_index_.find(
            chain_.back().hash
        );

    if (it == block_index_.end()) {
        return std::nullopt;
    }

    return randomx_seed_key_for(
        &it->second,
        it->second.height + 1U
    );
}

bool Chainstate::has_failed_ancestor(
    const Hash256& hash,
    const HeaderIndexOverlay* overlay) const
{
    const BlockIndexEntry* entry =
        find_index_entry(hash, overlay);

    while (entry != nullptr) {
        if (entry->failed) {
            return true;
        }

        if (entry->height == 0U) {
            break;
        }

        entry =
            find_index_entry(
                entry->parent,
                overlay
            );
    }

    return false;
}

std::optional<std::uint64_t> Chainstate::median_time_past(
    const BlockIndexEntry* parent,
    const HeaderIndexOverlay* overlay) const
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
            cursor->header.timestamp);

        if (cursor->height == 0U) {
            break;
        }

        cursor =
            find_index_entry(
                cursor->parent,
                overlay
            );

        if (cursor == nullptr) {
            return std::nullopt;
        }
    }

    return consensus::median_timestamp(timestamps);
}

std::optional<std::uint32_t> Chainstate::expected_bits(
    const Block& block,
    const BlockIndexEntry* parent,
    const HeaderIndexOverlay* overlay) const
{
    const auto& pow = params_.pow;

    if (parent == nullptr) {
        return params_.genesis.enforce
            ? params_.genesis.bits
            : pow.pow_limit_bits;
    }

    if (pow.no_retargeting) {
        return parent->header.bits;
    }

    if (pow.target_spacing_seconds == 0U) {
        return std::nullopt;
    }

    if (pow.difficulty_algorithm ==
        consensus::DifficultyAlgorithm::asert) {
        if (pow.asert_half_life_seconds == 0U ||
            pow.target_spacing_seconds >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()) ||
            pow.asert_half_life_seconds >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()) ||
            parent->height <
                pow.asert_anchor_height) {
            return std::nullopt;
        }

        if (pow.allow_min_difficulty_blocks) {
            const std::uint64_t delay =
                pow.target_spacing_seconds >
                        std::numeric_limits<std::uint64_t>::max() / 2U
                    ? std::numeric_limits<std::uint64_t>::max()
                    : pow.target_spacing_seconds * 2U;

            const std::uint64_t threshold =
                parent->header.timestamp >
                        std::numeric_limits<std::uint64_t>::max() - delay
                    ? std::numeric_limits<std::uint64_t>::max()
                    : parent->header.timestamp + delay;

            if (block.header.timestamp > threshold) {
                return pow.pow_limit_bits;
            }
        }

        const BlockIndexEntry* anchor = parent;

        while (anchor->height >
               pow.asert_anchor_height) {
            anchor =
                find_index_entry(
                    anchor->parent,
                    overlay
                );

            if (anchor == nullptr) {
                return std::nullopt;
            }
        }

        if (anchor->height !=
            pow.asert_anchor_height) {
            return std::nullopt;
        }

        std::uint64_t anchor_parent_time{0U};

        if (anchor->height == 0U) {
            if (anchor->header.timestamp <
                pow.target_spacing_seconds) {
                return std::nullopt;
            }

            anchor_parent_time =
                anchor->header.timestamp -
                pow.target_spacing_seconds;
        } else {
            const auto* anchor_parent =
                find_index_entry(
                    anchor->parent,
                    overlay
                );

            if (anchor_parent == nullptr) {
                return std::nullopt;
            }

            anchor_parent_time =
                anchor_parent->header.timestamp;
        }

        if (parent->header.timestamp >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max()) ||
            anchor_parent_time >
                static_cast<std::uint64_t>(
                    std::numeric_limits<std::int64_t>::max())) {
            return std::nullopt;
        }

        const std::int64_t time_diff =
            static_cast<std::int64_t>(
                parent->header.timestamp
            ) -
            static_cast<std::int64_t>(
                anchor_parent_time
            );

        const std::int64_t height_diff =
            static_cast<std::int64_t>(
                parent->height -
                anchor->height
            );

        const auto asert =
            consensus::calculate_asert_bits(
                anchor->header.bits,
                static_cast<std::int64_t>(
                    pow.target_spacing_seconds
                ),
                time_diff,
                height_diff,
                pow.pow_limit_bits,
                static_cast<std::int64_t>(
                    pow.asert_half_life_seconds
                )
            );

        return asert.ok()
            ? std::optional<std::uint32_t>{asert.bits}
            : std::nullopt;
    }

    if (pow.retarget_interval == 0U) {
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
                parent->header.timestamp >
                        std::numeric_limits<std::uint64_t>::max() - delay
                    ? std::numeric_limits<std::uint64_t>::max()
                    : parent->header.timestamp + delay;

            if (block.header.timestamp > threshold) {
                return pow.pow_limit_bits;
            }

            const BlockIndexEntry* cursor = parent;

            while ((cursor->height % pow.retarget_interval) != 0U &&
                   cursor->header.bits ==
                       pow.pow_limit_bits) {
                cursor =
                    find_index_entry(
                        cursor->parent,
                        overlay
                    );

                if (cursor == nullptr) {
                    return std::nullopt;
                }
            }

            return cursor->header.bits;
        }

        return parent->header.bits;
    }

    const BlockIndexEntry* first = parent;

    for (std::uint32_t step = 1U;
         step < pow.retarget_interval;
         ++step) {
        first =
            find_index_entry(
                first->parent,
                overlay
            );

        if (first == nullptr) {
            return std::nullopt;
        }
    }

    const auto retarget =
        consensus::calculate_retarget_bits(
            parent->header.bits,
            first->header.timestamp,
            parent->header.timestamp,
            pow
        );

    if (!retarget.ok()) {
        return std::nullopt;
    }

    return retarget.bits;
}

HeaderValidationResult Chainstate::validate_header_candidate(
    const BlockHeader& header,
    const BlockIndexEntry* parent,
    std::uint32_t height,
    std::uint64_t adjusted_time,
    const HeaderIndexOverlay* overlay,
    bool verify_pow) const
{
    HeaderValidationResult out;
    out.chain_work =
        parent == nullptr
            ? Hash256{}
            : parent->chain_work;

    if (!consensus::timestamp_not_too_far_future(
            header.timestamp,
            adjusted_time,
            params_.time.max_future_seconds)) {
        out.error =
            ChainConnectError::
                timestamp_too_far_future;
        return out;
    }

    if (parent != nullptr) {
        const auto mtp =
            median_time_past(
                parent,
                overlay
            );

        if (!mtp ||
            header.timestamp <= *mtp) {
            out.error =
                ChainConnectError::
                    timestamp_too_old;
            return out;
        }
    }

    Block candidate;
    candidate.header = header;

    const auto required_bits =
        expected_bits(
            candidate,
            parent,
            overlay
        );

    if (!required_bits ||
        header.bits != *required_bits) {
        out.error =
            ChainConnectError::
                unexpected_difficulty;
        return out;
    }

    if (verify_pow) {
        std::optional<Hash256> randomx_seed;

        if (params_.pow.pow_algorithm ==
            consensus::PowAlgorithm::
                randomx_v2) {
            randomx_seed =
                randomx_seed_key_for(
                    parent,
                    height,
                    overlay
                );

            if (!randomx_seed) {
                out.error =
                    ChainConnectError::
                        invalid_ancestor;
                return out;
            }
        }

        const auto pow_error =
            params_.pow.pow_algorithm ==
                    consensus::PowAlgorithm::
                        randomx_v2
                ? consensus::
                      check_randomx_proof_of_work(
                          header,
                          params_.pow,
                          *randomx_seed
                      )
                : consensus::
                      check_proof_of_work(
                          header,
                          params_.pow
                      );

        if (pow_error !=
            consensus::PowCheckError::none) {
            out.error =
                ChainConnectError::
                    invalid_proof_of_work;
            return out;
        }
    }

    const auto compact =
        consensus::decode_compact_target(
            header.bits
        );
    const auto block_work =
        consensus::work_for_target(
            compact.target
        );

    if (!consensus::add_chain_work(
            out.chain_work,
            block_work)) {
        out.error =
            ChainConnectError::
                chain_work_overflow;
        return out;
    }

    return out;
}

HeaderValidationResult Chainstate::validate_headers(
    std::span<const BlockHeader> headers,
    std::uint64_t adjusted_time) const
{
    HeaderValidationResult out;
    out.chain_work = cumulative_work();

    HeaderIndexOverlay overlay;

    for (std::size_t index = 0U;
         index < headers.size();
         ++index) {
        out.header_index = index;

        const auto& header = headers[index];
        const Hash256 hash =
            block_hash(header);

        // Metadata that is already in the validated block index (including a
        // pruned block body) does not need expensive PoW re-validation.
        if (const auto* known =
                find_index_entry(hash, nullptr)) {
            if (known->failed ||
                has_failed_ancestor(
                    hash,
                    nullptr)) {
                out.error =
                    ChainConnectError::
                        invalid_ancestor;
                return out;
            }

            if (known->height > 0U &&
                find_index_entry(
                    header.previous_block,
                    &overlay) == nullptr) {
                out.error =
                    ChainConnectError::
                        unknown_parent;
                return out;
            }

            out.chain_work =
                known->chain_work;
            continue;
        }

        const auto* parent =
            find_index_entry(
                header.previous_block,
                &overlay
            );

        if (parent == nullptr) {
            out.error =
                ChainConnectError::unknown_parent;
            return out;
        }

        if (parent->failed ||
            has_failed_ancestor(
                parent->hash,
                &overlay)) {
            out.error =
                ChainConnectError::
                    invalid_ancestor;
            return out;
        }

        if (parent->height ==
            std::numeric_limits<
                std::uint32_t>::max()) {
            out.error =
                ChainConnectError::
                    height_overflow;
            return out;
        }

        const std::uint32_t height =
            parent->height + 1U;

        const auto checked =
            validate_header_candidate(
                header,
                parent,
                height,
                adjusted_time,
                &overlay
            );

        if (!checked.ok()) {
            out.error = checked.error;
            return out;
        }

        const Hash256 chain_work =
            checked.chain_work;

        overlay.emplace(
            hash,
            BlockIndexEntry{
                .block = std::nullopt,
                .header = header,
                .hash = hash,
                .parent =
                    header.previous_block,
                .height = height,
                .chain_work = chain_work,
                .failed = false,
            }
        );

        out.chain_work = chain_work;
    }

    out.header_index = headers.size();
    return out;
}

ChainConnectResult Chainstate::restore_block_body(
    const Block& block)
{
    ChainConnectResult result;

    if (validate_block_structure(block) !=
        BlockStructureError::none) {
        result.error =
            ChainConnectError::invalid_block_structure;
        return result;
    }

    const auto resource_error =
        consensus::validate_block_resources(
            block,
            params_.limits);

    if (resource_error !=
        consensus::BlockResourceError::none) {
        result.error =
            ChainConnectError::resource_limits_exceeded;
        result.resource_error = resource_error;
        return result;
    }

    const auto hash = block_hash(block.header);
    auto it = block_index_.find(hash);

    if (it == block_index_.end()) {
        result.error = ChainConnectError::unknown_parent;
        return result;
    }

    if (it->second.failed ||
        has_failed_ancestor(hash)) {
        result.error = ChainConnectError::invalid_ancestor;
        return result;
    }

    if (it->second.block) {
        result.error = ChainConnectError::duplicate_block;
        return result;
    }

    it->second.block = block;
    return result;
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
    return connect_block_impl(
        block,
        adjusted_time,
        true
    );
}

ChainConnectResult Chainstate::connect_validated_snapshot_block(
    const Block& block,
    std::uint64_t adjusted_time)
{
    return connect_block_impl(
        block,
        adjusted_time,
        false
    );
}

ChainConnectResult Chainstate::connect_block_impl(
    const Block& block,
    std::uint64_t adjusted_time,
    bool verify_pow)
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
        parent_entry = &parent_it->second;
    }

    const auto header_check =
        validate_header_candidate(
            block.header,
            parent_entry,
            new_height,
            adjusted_time,
            nullptr,
            verify_pow
        );

    if (!header_check.ok()) {
        result.error =
            header_check.error;
        return result;
    }

    const Hash256 new_chain_work =
        header_check.chain_work;

    block_index_.emplace(
        hash,
        BlockIndexEntry{
            .block = block,
            .header = block.header,
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
        acceptance_order_.push_back(hash);
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

        const BlockIndexEntry* staged_parent{
            nullptr
        };

        if (staged_height > 0U) {
            const auto parent_it =
                block_index_.find(
                    index_it->second.parent
                );

            if (parent_it ==
                block_index_.end()) {
                index_it->second.failed = true;
                result.error =
                    ChainConnectError::invalid_ancestor;
                return result;
            }

            staged_parent =
                &parent_it->second;
        }

        std::optional<Hash256>
            staged_randomx_seed;

        if (verify_pow &&
            params_.pow.pow_algorithm ==
                consensus::PowAlgorithm::randomx_v2) {
            staged_randomx_seed =
                randomx_seed_key_for(
                    staged_parent,
                    staged_height
                );

            if (!staged_randomx_seed) {
                index_it->second.failed = true;
                result.error =
                    ChainConnectError::invalid_ancestor;
                return result;
            }
        }

        if (!index_it->second.block) {
            result.error =
                ChainConnectError::block_body_unavailable;
            return result;
        }

        auto applied = apply_block_to_view(
            *index_it->second.block,
            staged_utxos,
            staged_height,
            staged_parent_work,
            params_.pow,
            params_.monetary,
            params_.limits,
            staged_randomx_seed,
            verify_pow
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
    acceptance_order_.push_back(hash);
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
