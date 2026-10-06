#pragma once

#include "chain/storage.hpp"
#include "consensus/pow.hpp"
#include "mining/block_template.hpp"
#include "node/mempool.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace quintum {

enum class NodeStartError {
    none,
    storage_failed,
    invalid_genesis,
    genesis_connect_failed,
};

struct NodeStartResult {
    NodeStartError error{NodeStartError::none};
    StorageError storage_error{StorageError::none};
    ChainConnectError chain_error{ChainConnectError::none};
    bool created_genesis{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == NodeStartError::none;
    }
};

enum class NodeTransactionError {
    none,
    not_started,
    mempool_rejected,
};

struct NodeTransactionResult {
    NodeTransactionError error{NodeTransactionError::none};
    MempoolAcceptResult mempool{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == NodeTransactionError::none;
    }
};

enum class NodeSubmitError {
    none,
    not_started,
    chain_rejected,
    storage_failed,
};

struct NodeSubmitResult {
    NodeSubmitError error{NodeSubmitError::none};
    PersistentConnectResult connect{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == NodeSubmitError::none;
    }
};

enum class NodeMineError {
    none,
    not_started,
    template_failed,
    proof_of_work_exhausted,
    proof_of_work_invalid,
    chain_rejected,
    storage_failed,
};

struct NodeMineResult {
    NodeMineError error{NodeMineError::none};
    mining::BlockTemplateError template_error{
        mining::BlockTemplateError::none
    };
    consensus::MiningResult mining{};
    PersistentConnectResult connect{};
    Block block{};
    std::uint32_t height{0U};
    Amount total_fees{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == NodeMineError::none;
    }
};

class NodeRuntime {
public:
    NodeRuntime(
        const consensus::ChainParams& params,
        std::filesystem::path directory,
        PrunePolicy prune_policy = {}
    );

    [[nodiscard]] NodeStartResult start();
    [[nodiscard]] NodeStartResult start_at(
        std::uint64_t adjusted_time
    );

    [[nodiscard]] bool started() const noexcept;
    [[nodiscard]] const Chainstate& chain() const noexcept;
    [[nodiscard]] const ChainstateStore& store() const noexcept;
    [[nodiscard]] const Mempool& mempool() const noexcept;

    [[nodiscard]] NodeTransactionResult submit_transaction(
        const Transaction& transaction
    );

    [[nodiscard]] NodeSubmitResult restore_block_body(
        const Block& block
    );

    [[nodiscard]] NodeSubmitResult submit_block(
        const Block& block
    );
    [[nodiscard]] NodeSubmitResult submit_block_at(
        const Block& block,
        std::uint64_t adjusted_time
    );

    [[nodiscard]] NodeMineResult mine_mempool_block(
        const Bytes& payout_script,
        std::uint64_t max_attempts
    );

    [[nodiscard]] NodeMineResult mine_mempool_block_at(
        const Bytes& payout_script,
        std::uint64_t adjusted_time,
        std::uint64_t max_attempts,
        std::uint64_t start_nonce = 0U
    );

    [[nodiscard]] NodeMineResult
    mine_mempool_block_parallel_at(
        const Bytes& payout_script,
        std::uint64_t adjusted_time,
        std::uint64_t max_attempts,
        std::size_t worker_count,
        bool full_memory,
        std::uint64_t start_nonce = 0U
    );

    [[nodiscard]] NodeMineResult mine_block(
        const Bytes& payout_script,
        std::uint64_t max_attempts,
        std::span<const Transaction> transactions = {}
    );

    [[nodiscard]] NodeMineResult mine_block_at(
        const Bytes& payout_script,
        std::uint64_t adjusted_time,
        std::uint64_t max_attempts,
        std::span<const Transaction> transactions = {},
        std::uint64_t start_nonce = 0U
    );

    [[nodiscard]] NodeMineResult
    mine_block_parallel_at(
        const Bytes& payout_script,
        std::uint64_t adjusted_time,
        std::uint64_t max_attempts,
        std::size_t worker_count,
        bool full_memory,
        std::span<const Transaction> transactions = {},
        std::uint64_t start_nonce = 0U
    );

private:
    consensus::ChainParams params_{};
    PersistentChainstate persistent_;
    Mempool mempool_{};
    bool started_{false};
};

} // namespace quintum
