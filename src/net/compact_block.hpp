#pragma once

#include "consensus/chainparams.hpp"
#include "node/mempool.hpp"
#include "primitives/block.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace quintum::net {

inline constexpr std::uint32_t
    kInventoryCompactBlock = 4U;
inline constexpr std::size_t
    kCompactShortIdBytes = 6U;
inline constexpr std::size_t
    kMaxCompactPrefilledTransactions = 16U;

struct CompactPrefilledTransaction {
    std::uint32_t index{0U};
    Transaction transaction{};
};

struct CompactBlock {
    BlockHeader header{};
    std::uint64_t nonce{0U};
    std::vector<std::uint64_t> short_ids{};
    std::vector<CompactPrefilledTransaction>
        prefilled{};
};

struct BlockTransactionsRequest {
    Hash256 block_hash{};
    std::vector<std::uint32_t> indexes{};
};

struct BlockTransactions {
    Hash256 block_hash{};
    std::vector<std::pair<
        std::uint32_t,
        Transaction>> transactions{};
};

enum class CompactBlockError {
    none,
    malformed,
    merkle_mismatch,
    response_mismatch,
};

struct CompactReconstructionResult {
    CompactBlockError error{
        CompactBlockError::none
    };
    std::optional<Block> block{};
    std::vector<std::uint32_t>
        missing_indexes{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == CompactBlockError::none;
    }

    [[nodiscard]] bool complete() const noexcept
    {
        return ok() &&
               block.has_value() &&
               missing_indexes.empty();
    }
};

[[nodiscard]] std::uint64_t
compact_short_id(
    const BlockHeader& header,
    std::uint64_t nonce,
    const Hash256& txid
) noexcept;

[[nodiscard]] Bytes serialize_compact_block(
    const Block& block,
    std::uint64_t nonce
);

[[nodiscard]] std::optional<CompactBlock>
parse_compact_block(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits
);

[[nodiscard]] CompactReconstructionResult
reconstruct_compact_block(
    const CompactBlock& compact,
    const Mempool& mempool
);

[[nodiscard]] Bytes serialize_getblocktxn(
    const BlockTransactionsRequest& request
);

[[nodiscard]] std::optional<
    BlockTransactionsRequest>
parse_getblocktxn(
    std::span<const Byte> payload,
    std::uint32_t max_transactions
);

[[nodiscard]] Bytes serialize_blocktxn(
    const BlockTransactions& response
);

[[nodiscard]] std::optional<BlockTransactions>
parse_blocktxn(
    std::span<const Byte> payload,
    const consensus::ResourceLimits& limits
);

[[nodiscard]] CompactReconstructionResult
complete_compact_block(
    const CompactBlock& compact,
    const Mempool& mempool,
    const BlockTransactions& response
);

} // namespace quintum::net
