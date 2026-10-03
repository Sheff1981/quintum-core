#include "node/datadir.hpp"

#include <array>
#include <system_error>
#include <utility>

namespace quintum {
namespace {

DataDirectoryError ensure_directory(
    const std::filesystem::path& path)
{
    std::error_code ec;
    const bool exists =
        std::filesystem::exists(path, ec);

    if (ec) {
        return DataDirectoryError::io_error;
    }

    if (exists) {
        const bool is_directory =
            std::filesystem::is_directory(path, ec);

        if (ec) {
            return DataDirectoryError::io_error;
        }

        return is_directory
            ? DataDirectoryError::none
            : DataDirectoryError::conflict;
    }

    if (!std::filesystem::create_directories(
            path,
            ec) &&
        ec) {
        return DataDirectoryError::io_error;
    }

    return ec
        ? DataDirectoryError::io_error
        : DataDirectoryError::none;
}

DataDirectoryError migrate_file(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
    std::error_code ec;
    const bool source_exists =
        std::filesystem::exists(source, ec);

    if (ec) {
        return DataDirectoryError::io_error;
    }

    if (!source_exists) {
        return DataDirectoryError::none;
    }

    const bool source_is_file =
        std::filesystem::is_regular_file(source, ec);

    if (ec) {
        return DataDirectoryError::io_error;
    }

    if (!source_is_file) {
        return DataDirectoryError::conflict;
    }

    const bool destination_exists =
        std::filesystem::exists(destination, ec);

    if (ec) {
        return DataDirectoryError::io_error;
    }

    if (destination_exists) {
        // Never guess which copy is authoritative.
        return DataDirectoryError::conflict;
    }

    const auto parent =
        destination.parent_path();

    const auto directory_error =
        ensure_directory(parent);

    if (directory_error !=
        DataDirectoryError::none) {
        return directory_error;
    }

    std::filesystem::rename(
        source,
        destination,
        ec
    );

    return ec
        ? DataDirectoryError::io_error
        : DataDirectoryError::none;
}

DataDirectoryError migrate_files(
    const std::filesystem::path& root,
    const std::filesystem::path& destination_directory,
    const std::array<const char*, 3>& names)
{
    for (const char* name : names) {
        const auto error =
            migrate_file(
                root / name,
                destination_directory / name
            );

        if (error != DataDirectoryError::none) {
            return error;
        }
    }

    return DataDirectoryError::none;
}

} // namespace

DataDirectoryLayout::DataDirectoryLayout(
    std::filesystem::path root)
    : root_(std::move(root))
{
}

const std::filesystem::path&
DataDirectoryLayout::root() const noexcept
{
    return root_;
}

std::filesystem::path
DataDirectoryLayout::blocks_directory() const
{
    return root_ / "blocks";
}

std::filesystem::path
DataDirectoryLayout::chainstate_directory() const
{
    return root_ / "chainstate";
}

std::filesystem::path
DataDirectoryLayout::indexes_directory() const
{
    return root_ / "indexes";
}

std::filesystem::path
DataDirectoryLayout::wallets_directory() const
{
    return root_ / "wallets";
}

std::filesystem::path
DataDirectoryLayout::wallet_directory() const
{
    return wallets_directory() / "default";
}

std::filesystem::path
DataDirectoryLayout::blocks_file() const
{
    return blocks_directory() / "blocks.dat";
}

std::filesystem::path
DataDirectoryLayout::chainstate_file() const
{
    return chainstate_directory() / "chainstate.dat";
}

std::filesystem::path
DataDirectoryLayout::wallet_file() const
{
    return wallet_directory() / "wallet.dat";
}

std::filesystem::path
DataDirectoryLayout::wallet_state_file() const
{
    return wallet_directory() / "wallet_state.dat";
}

std::filesystem::path
DataDirectoryLayout::wallet_metadata_file() const
{
    return wallet_directory() / "wallet_meta.dat";
}

std::filesystem::path
DataDirectoryLayout::peers_file() const
{
    return root_ / "peers.dat";
}

DataDirectoryError DataDirectoryLayout::prepare(
    bool wallet_enabled) const
{
    const auto root_error =
        ensure_directory(root_);

    if (root_error != DataDirectoryError::none) {
        return root_error;
    }

    for (const auto& directory : {
             blocks_directory(),
             chainstate_directory(),
             indexes_directory()}) {
        const auto error =
            ensure_directory(directory);

        if (error != DataDirectoryError::none) {
            return error;
        }
    }

    {
        const std::array<const char*, 1> block_files{
            "blocks.dat"
        };

        const auto error =
            migrate_file(
                root_ / block_files.front(),
                blocks_file()
            );

        if (error != DataDirectoryError::none) {
            return error;
        }
    }

    {
        const std::array<const char*, 2> state_files{
            "chainstate.dat",
            "chainstate.dat.tmp"
        };

        for (const char* name : state_files) {
            const auto error =
                migrate_file(
                    root_ / name,
                    chainstate_directory() / name
                );

            if (error != DataDirectoryError::none) {
                return error;
            }
        }
    }

    if (!wallet_enabled) {
        return DataDirectoryError::none;
    }

    const auto wallet_directory_error =
        ensure_directory(wallet_directory());

    if (wallet_directory_error !=
        DataDirectoryError::none) {
        return wallet_directory_error;
    }

    return migrate_files(
        root_,
        wallet_directory(),
        std::array<const char*, 3>{
            "wallet.dat",
            "wallet_state.dat",
            "wallet_meta.dat"
        }
    );
}

} // namespace quintum
