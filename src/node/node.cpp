#include "node/node.hpp"

#include "consensus/genesis.hpp"

#include <chrono>
#include <utility>

namespace quintum {
namespace {

std::uint64_t unix_time_now() noexcept
{
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();

    return seconds < 0
        ? 0U
        : static_cast<std::uint64_t>(seconds);
}

} // namespace

NodeRuntime::NodeRuntime(
    const consensus::ChainParams& params,
    std::filesystem::path directory)
    : params_(params),
      persistent_(params, std::move(directory))
{
}

NodeStartResult NodeRuntime::start()
{
    return start_at(unix_time_now());
}

NodeStartResult NodeRuntime::start_at(
    std::uint64_t adjusted_time)
{
    if (started_) {
        return {};
    }

    const auto load_error = persistent_.load();

    if (load_error == StorageError::none) {
        started_ = true;
        return {};
    }

    if (load_error != StorageError::not_found) {
        return NodeStartResult{
            .error = NodeStartError::storage_failed,
            .storage_error = load_error,
        };
    }

    if (!consensus::verify_genesis(params_)) {
        return NodeStartResult{
            .error = NodeStartError::invalid_genesis,
        };
    }

    const auto genesis = consensus::create_genesis_block(params_);
    const auto connected =
        persistent_.connect_block(genesis, adjusted_time);

    if (!connected.chain.ok()) {
        return NodeStartResult{
            .error = NodeStartError::genesis_connect_failed,
            .chain_error = connected.chain.error,
        };
    }

    if (connected.storage_error != StorageError::none) {
        return NodeStartResult{
            .error = NodeStartError::storage_failed,
            .storage_error = connected.storage_error,
        };
    }

    started_ = true;
    return NodeStartResult{
        .created_genesis = true,
    };
}

bool NodeRuntime::started() const noexcept
{
    return started_;
}

const Chainstate& NodeRuntime::chain() const noexcept
{
    return persistent_.chain();
}

const ChainstateStore& NodeRuntime::store() const noexcept
{
    return persistent_.store();
}

const Mempool& NodeRuntime::mempool() const noexcept
{
    return mempool_;
}

NodeTransactionResult NodeRuntime::submit_transaction(
    const Transaction& transaction)
{
    NodeTransactionResult out;

    if (!started_) {
        out.error = NodeTransactionError::not_started;
        return out;
    }

    out.mempool =
        mempool_.accept(
            persistent_.chain(),
            transaction
        );

    if (!out.mempool.ok()) {
        out.error =
            NodeTransactionError::mempool_rejected;
    }

    return out;
}

NodeSubmitResult NodeRuntime::submit_block(
    const Block& block)
{
    return submit_block_at(
        block,
        unix_time_now()
    );
}

NodeSubmitResult NodeRuntime::submit_block_at(
    const Block& block,
    std::uint64_t adjusted_time)
{
    NodeSubmitResult out;

    if (!started_) {
        out.error = NodeSubmitError::not_started;
        return out;
    }

    out.connect =
        persistent_.connect_block(
            block,
            adjusted_time
        );

    if (!out.connect.chain.ok()) {
        out.error = NodeSubmitError::chain_rejected;
        return out;
    }

    if (out.connect.storage_error != StorageError::none) {
        out.error = NodeSubmitError::storage_failed;
        return out;
    }

    mempool_.reconcile(persistent_.chain());
    return out;
}

NodeMineResult NodeRuntime::mine_mempool_block(
    const Bytes& payout_script,
    std::uint64_t max_attempts)
{
    return mine_mempool_block_at(
        payout_script,
        unix_time_now(),
        max_attempts
    );
}

NodeMineResult NodeRuntime::mine_mempool_block_at(
    const Bytes& payout_script,
    std::uint64_t adjusted_time,
    std::uint64_t max_attempts,
    std::uint64_t start_nonce)
{
    if (!started_) {
        NodeMineResult out;
        out.error = NodeMineError::not_started;
        return out;
    }

    mempool_.reconcile(persistent_.chain());

    const std::size_t block_transaction_limit =
        params_.limits.max_block_transactions > 0U
            ? static_cast<std::size_t>(
                  params_.limits.max_block_transactions - 1U)
            : 0U;

    auto transactions =
        mempool_.transactions(
            block_transaction_limit
        );

    std::size_t low{0U};
    std::size_t high{transactions.size()};
    std::size_t best{0U};

    while (low <= high) {
        const std::size_t mid =
            low + (high - low) / 2U;

        const auto candidate =
            mining::create_block_template(
                persistent_.chain(),
                payout_script,
                adjusted_time,
                std::span<const Transaction>(
                    transactions.data(),
                    mid
                )
            );

        if (candidate.ok()) {
            best = mid;
            low = mid + 1U;
            continue;
        }

        if (candidate.error ==
            mining::BlockTemplateError::
                resource_limits_exceeded) {
            if (mid == 0U) {
                break;
            }
            high = mid - 1U;
            continue;
        }

        NodeMineResult out;
        out.error = NodeMineError::template_failed;
        out.template_error = candidate.error;
        return out;
    }

    return mine_block_at(
        payout_script,
        adjusted_time,
        max_attempts,
        std::span<const Transaction>(
            transactions.data(),
            best
        ),
        start_nonce
    );
}

NodeMineResult
NodeRuntime::mine_mempool_block_parallel_at(
    const Bytes& payout_script,
    std::uint64_t adjusted_time,
    std::uint64_t max_attempts,
    std::size_t worker_count,
    bool full_memory,
    std::uint64_t start_nonce)
{
    if (!started_) {
        NodeMineResult out;
        out.error =
            NodeMineError::not_started;
        return out;
    }

    mempool_.reconcile(
        persistent_.chain()
    );

    const std::size_t block_transaction_limit =
        params_.limits.max_block_transactions > 0U
            ? static_cast<std::size_t>(
                  params_.limits.
                      max_block_transactions -
                  1U)
            : 0U;

    auto transactions =
        mempool_.transactions(
            block_transaction_limit
        );

    std::size_t low{0U};
    std::size_t high{
        transactions.size()
    };
    std::size_t best{0U};

    while (low <= high) {
        const std::size_t mid =
            low + (high - low) / 2U;

        const auto candidate =
            mining::create_block_template(
                persistent_.chain(),
                payout_script,
                adjusted_time,
                std::span<const Transaction>(
                    transactions.data(),
                    mid
                )
            );

        if (candidate.ok()) {
            best = mid;
            low = mid + 1U;
            continue;
        }

        if (candidate.error ==
            mining::BlockTemplateError::
                resource_limits_exceeded) {
            if (mid == 0U) {
                break;
            }
            high = mid - 1U;
            continue;
        }

        NodeMineResult out;
        out.error =
            NodeMineError::template_failed;
        out.template_error =
            candidate.error;
        return out;
    }

    return mine_block_parallel_at(
        payout_script,
        adjusted_time,
        max_attempts,
        worker_count,
        full_memory,
        std::span<const Transaction>(
            transactions.data(),
            best
        ),
        start_nonce
    );
}

NodeMineResult NodeRuntime::mine_block(
    const Bytes& payout_script,
    std::uint64_t max_attempts,
    std::span<const Transaction> transactions)
{
    return mine_block_at(
        payout_script,
        unix_time_now(),
        max_attempts,
        transactions
    );
}

NodeMineResult NodeRuntime::mine_block_at(
    const Bytes& payout_script,
    std::uint64_t adjusted_time,
    std::uint64_t max_attempts,
    std::span<const Transaction> transactions,
    std::uint64_t start_nonce)
{
    NodeMineResult out;

    if (!started_) {
        out.error = NodeMineError::not_started;
        return out;
    }

    auto block_template =
        mining::create_block_template(
            persistent_.chain(),
            payout_script,
            adjusted_time,
            transactions
        );

    if (!block_template.ok()) {
        out.error = NodeMineError::template_failed;
        out.template_error = block_template.error;
        return out;
    }

    out.block = std::move(block_template.value.block);
    out.height = block_template.value.height;
    out.total_fees = block_template.value.total_fees;
    out.block.header.nonce = start_nonce;

    std::optional<Hash256> randomx_seed;

    if (params_.pow.pow_algorithm ==
        consensus::PowAlgorithm::randomx_v2) {
        randomx_seed =
            persistent_.chain().
                next_randomx_seed_key();

        if (!randomx_seed) {
            out.error =
                NodeMineError::proof_of_work_invalid;
            return out;
        }

        out.mining =
            consensus::mine_randomx_header(
                out.block.header,
                *randomx_seed,
                max_attempts
            );
    } else {
        out.mining =
            consensus::mine_header(
                out.block.header,
                max_attempts
            );
    }

    if (!out.mining.found()) {
        out.error =
            out.mining.status ==
                    consensus::MineStatus::hashing_failed
                ? NodeMineError::proof_of_work_invalid
                : NodeMineError::proof_of_work_exhausted;
        return out;
    }

    const auto pow_error =
        params_.pow.pow_algorithm ==
                consensus::PowAlgorithm::randomx_v2
            ? consensus::check_randomx_proof_of_work(
                  out.block.header,
                  params_.pow,
                  *randomx_seed
              )
            : consensus::check_proof_of_work(
                  out.block.header,
                  params_.pow
              );

    if (pow_error !=
        consensus::PowCheckError::none) {
        out.error =
            NodeMineError::proof_of_work_invalid;
        return out;
    }

    out.connect =
        persistent_.connect_block(
            out.block,
            adjusted_time
        );

    if (!out.connect.chain.ok()) {
        out.error = NodeMineError::chain_rejected;
        return out;
    }

    if (out.connect.storage_error != StorageError::none) {
        out.error = NodeMineError::storage_failed;
        return out;
    }

    mempool_.reconcile(persistent_.chain());
    return out;
}

NodeMineResult NodeRuntime::mine_block_parallel_at(
    const Bytes& payout_script,
    std::uint64_t adjusted_time,
    std::uint64_t max_attempts,
    std::size_t worker_count,
    bool full_memory,
    std::span<const Transaction> transactions,
    std::uint64_t start_nonce)
{
    if (params_.pow.pow_algorithm !=
            consensus::PowAlgorithm::randomx_v2 ||
        worker_count <= 1U) {
        return mine_block_at(
            payout_script,
            adjusted_time,
            max_attempts,
            transactions,
            start_nonce
        );
    }

    NodeMineResult out;

    if (!started_) {
        out.error =
            NodeMineError::not_started;
        return out;
    }

    auto block_template =
        mining::create_block_template(
            persistent_.chain(),
            payout_script,
            adjusted_time,
            transactions
        );

    if (!block_template.ok()) {
        out.error =
            NodeMineError::template_failed;
        out.template_error =
            block_template.error;
        return out;
    }

    out.block =
        std::move(
            block_template.value.block);
    out.height =
        block_template.value.height;
    out.total_fees =
        block_template.value.total_fees;
    out.block.header.nonce =
        start_nonce;

    const auto randomx_seed =
        persistent_.chain().
            next_randomx_seed_key();

    if (!randomx_seed) {
        out.error =
            NodeMineError::
                proof_of_work_invalid;
        return out;
    }

    out.mining =
        consensus::
            mine_randomx_header_parallel(
                out.block.header,
                *randomx_seed,
                max_attempts,
                worker_count,
                full_memory
            );

    if (!out.mining.found()) {
        out.error =
            out.mining.status ==
                    consensus::MineStatus::
                        hashing_failed
                ? NodeMineError::
                      proof_of_work_invalid
                : NodeMineError::
                      proof_of_work_exhausted;
        return out;
    }

    if (consensus::
            check_randomx_proof_of_work(
                out.block.header,
                params_.pow,
                *randomx_seed
            ) !=
        consensus::PowCheckError::none) {
        out.error =
            NodeMineError::
                proof_of_work_invalid;
        return out;
    }

    out.connect =
        persistent_.connect_block(
            out.block,
            adjusted_time
        );

    if (!out.connect.chain.ok()) {
        out.error =
            NodeMineError::chain_rejected;
        return out;
    }

    if (out.connect.storage_error !=
        StorageError::none) {
        out.error =
            NodeMineError::storage_failed;
        return out;
    }

    mempool_.reconcile(
        persistent_.chain()
    );
    return out;
}

} // namespace quintum
