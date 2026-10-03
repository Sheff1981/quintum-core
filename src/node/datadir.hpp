#pragma once

#include <filesystem>

namespace quintum {

enum class DataDirectoryError {
    none,
    io_error,
    conflict,
};

class DataDirectoryLayout {
public:
    explicit DataDirectoryLayout(
        std::filesystem::path root
    );

    [[nodiscard]] const std::filesystem::path&
    root() const noexcept;

    [[nodiscard]] std::filesystem::path
    blocks_directory() const;
    [[nodiscard]] std::filesystem::path
    chainstate_directory() const;
    [[nodiscard]] std::filesystem::path
    indexes_directory() const;
    [[nodiscard]] std::filesystem::path
    wallets_directory() const;
    [[nodiscard]] std::filesystem::path
    wallet_directory() const;

    [[nodiscard]] std::filesystem::path
    blocks_file() const;
    [[nodiscard]] std::filesystem::path
    chainstate_file() const;
    [[nodiscard]] std::filesystem::path
    wallet_file() const;
    [[nodiscard]] std::filesystem::path
    wallet_state_file() const;
    [[nodiscard]] std::filesystem::path
    wallet_metadata_file() const;
    [[nodiscard]] std::filesystem::path
    peers_file() const;

    // Creates the version-1 directory layout and moves known legacy flat
    // files into it using same-filesystem rename. Existing destination files
    // are never overwritten. A conflict fails closed and leaves both files
    // untouched.
    //
    // Wallet migration is skipped for network-only nodes so a public seed
    // never touches private wallet material that may exist in the datadir.
    [[nodiscard]] DataDirectoryError prepare(
        bool wallet_enabled = true
    ) const;

private:
    std::filesystem::path root_{};
};

} // namespace quintum
