#pragma once

#include "core/serialize.hpp"
#include "core/types.hpp"
#include "primitives/transaction.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace quintum {

struct BlockHeader {
    std::uint32_t version{1U};
    Hash256 previous_block{};
    Hash256 merkle_root{};
    std::uint64_t timestamp{0U};
    std::uint32_t bits{0U};
    std::uint64_t nonce{0U};
};

struct Block {
    BlockHeader header{};
    std::vector<Transaction> transactions{};
};

struct MerkleResult {
    Hash256 root{};
    bool mutated{false};
};

enum class BlockStructureError {
    none,
    no_transactions,
    first_transaction_not_coinbase,
    multiple_coinbase_transactions,
    merkle_root_mismatch,
    mutated_merkle_tree,
};

[[nodiscard]] std::optional<std::size_t> serialized_block_size(
    const Block& block
) noexcept;
[[nodiscard]] Bytes serialize_block_header(const BlockHeader& header);
[[nodiscard]] Hash256 block_hash(const BlockHeader& header);
[[nodiscard]] MerkleResult compute_merkle_root(std::span<const Transaction> transactions);
void update_merkle_root(Block& block);
[[nodiscard]] BlockStructureError validate_block_structure(const Block& block);

} // namespace quintum
