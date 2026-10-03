#pragma once

#include "chain/chainstate.hpp"
#include "consensus/chainparams.hpp"

#include <cstdint>
#include <filesystem>

namespace quintum {

enum class StorageError {
    none,
    not_found,
    io_error,
    truncated,
    checksum_mismatch,
    bad_format,
    unsupported_version,
    wrong_network,
    state_mismatch,
    consensus_replay_failed,
};

class ChainstateStore {
public:
    ChainstateStore(
        std::filesystem::path directory,
        const consensus::ChainParams& params
    );

    ChainstateStore(
        std::filesystem::path directory,
        std::filesystem::path blocks_directory,
        std::filesystem::path state_directory,
        const consensus::ChainParams& params
    );

    [[nodiscard]] const std::filesystem::path& directory() const noexcept;
    [[nodiscard]] std::filesystem::path blocks_path() const;
    [[nodiscard]] std::filesystem::path state_path() const;

    // Commits the accepted block log first, then atomically replaces the
    // chainstate snapshot. A crash can therefore leave only an uncommitted
    // tail in blocks.dat; load() ignores that tail.
    [[nodiscard]] StorageError commit(const Chainstate& chain) const;

    // Reconstructs and revalidates state from disk. The destination is changed
    // only after the complete snapshot and block log have been verified.
    [[nodiscard]] StorageError load(Chainstate& chain) const;

private:
    std::filesystem::path directory_{};
    std::filesystem::path blocks_directory_{};
    std::filesystem::path state_directory_{};
    consensus::ChainParams params_{};
};

struct PersistentConnectResult {
    ChainConnectResult chain{};
    StorageError storage_error{StorageError::none};

    [[nodiscard]] bool ok() const noexcept
    {
        return chain.ok() &&
               storage_error == StorageError::none;
    }
};

struct PersistentDisconnectResult {
    ChainDisconnectError chain{ChainDisconnectError::none};
    StorageError storage_error{StorageError::none};

    [[nodiscard]] bool ok() const noexcept
    {
        return chain == ChainDisconnectError::none &&
               storage_error == StorageError::none;
    }
};

class PersistentChainstate {
public:
    PersistentChainstate(
        const consensus::ChainParams& params,
        std::filesystem::path directory
    );

    PersistentChainstate(
        const consensus::ChainParams& params,
        std::filesystem::path directory,
        std::filesystem::path blocks_directory,
        std::filesystem::path state_directory
    );

    [[nodiscard]] StorageError load();

    [[nodiscard]] const Chainstate& chain() const noexcept;
    [[nodiscard]] const ChainstateStore& store() const noexcept;

    [[nodiscard]] PersistentConnectResult connect_block(
        const Block& block
    );
    [[nodiscard]] PersistentConnectResult connect_block(
        const Block& block,
        std::uint64_t adjusted_time
    );

    [[nodiscard]] PersistentDisconnectResult disconnect_tip();

private:
    Chainstate chain_;
    ChainstateStore store_;
};

} // namespace quintum
