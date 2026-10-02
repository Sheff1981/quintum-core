#include "primitives/block.hpp"

#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <vector>

namespace quintum {
namespace {

Hash256 hash_pair(const Hash256& left, const Hash256& right)
{
    std::array<Byte, 64> bytes{};
    std::copy(left.begin(), left.end(), bytes.begin());
    std::copy(right.begin(), right.end(), bytes.begin() + 32);
    return crypto::double_sha256(bytes);
}

} // namespace

Bytes serialize_block_header(const BlockHeader& header)
{
    Bytes out;
    out.reserve(88U);

    append_little_endian(out, header.version);
    out.insert(out.end(), header.previous_block.begin(), header.previous_block.end());
    out.insert(out.end(), header.merkle_root.begin(), header.merkle_root.end());
    append_little_endian(out, header.timestamp);
    append_little_endian(out, header.bits);
    append_little_endian(out, header.nonce);

    return out;
}

Hash256 block_hash(const BlockHeader& header)
{
    const auto bytes = serialize_block_header(header);
    return crypto::double_sha256(bytes);
}

MerkleResult compute_merkle_root(std::span<const Transaction> transactions)
{
    MerkleResult result;
    if (transactions.empty()) {
        return result;
    }

    std::vector<Hash256> level;
    level.reserve(transactions.size());

    for (const auto& tx : transactions) {
        level.push_back(transaction_id(tx));
    }

    while (level.size() > 1U) {
        std::vector<Hash256> next;
        next.reserve((level.size() + 1U) / 2U);

        for (std::size_t i = 0; i < level.size(); i += 2U) {
            const std::size_t right_index = (i + 1U < level.size()) ? i + 1U : i;

            if (i + 1U < level.size() && level[i] == level[right_index]) {
                result.mutated = true;
            }

            next.push_back(hash_pair(level[i], level[right_index]));
        }

        level = std::move(next);
    }

    result.root = level.front();
    return result;
}

void update_merkle_root(Block& block)
{
    block.header.merkle_root = compute_merkle_root(block.transactions).root;
}

BlockStructureError validate_block_structure(const Block& block)
{
    if (block.transactions.empty()) {
        return BlockStructureError::no_transactions;
    }

    if (!block.transactions.front().is_coinbase()) {
        return BlockStructureError::first_transaction_not_coinbase;
    }

    for (std::size_t i = 1; i < block.transactions.size(); ++i) {
        if (block.transactions[i].is_coinbase()) {
            return BlockStructureError::multiple_coinbase_transactions;
        }
    }

    const auto merkle = compute_merkle_root(block.transactions);
    if (merkle.mutated) {
        return BlockStructureError::mutated_merkle_tree;
    }

    if (merkle.root != block.header.merkle_root) {
        return BlockStructureError::merkle_root_mismatch;
    }

    return BlockStructureError::none;
}

} // namespace quintum
