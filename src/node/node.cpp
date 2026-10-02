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

    return out;
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
    std::span<const Transaction> transactions)
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

    out.mining =
        consensus::mine_header(
            out.block.header,
            max_attempts
        );

    if (!out.mining.found()) {
        out.error = NodeMineError::proof_of_work_exhausted;
        return out;
    }

    if (consensus::check_proof_of_work(
            out.block.header,
            params_.pow) !=
        consensus::PowCheckError::none) {
        out.error = NodeMineError::proof_of_work_invalid;
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

    return out;
}

} // namespace quintum
