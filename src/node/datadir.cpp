#include "node/datadir.hpp"

#include <array>
#include <cerrno>
#include <system_error>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>
#endif

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

std::filesystem::path
DataDirectoryLayout::lock_file() const
{
    return root_ / ".lock";
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
        const auto error =
            migrate_file(
                root_ / "blocks.dat",
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

DataDirectoryLock::~DataDirectoryLock()
{
    release();
}

DataDirectoryLock::DataDirectoryLock(
    DataDirectoryLock&& other) noexcept
{
#ifdef _WIN32
    handle_ = other.handle_;
    other.handle_ = nullptr;
#else
    fd_ = other.fd_;
    other.fd_ = -1;
#endif
}

DataDirectoryLock& DataDirectoryLock::operator=(
    DataDirectoryLock&& other) noexcept
{
    if (this == &other) {
        return *this;
    }

    release();

#ifdef _WIN32
    handle_ = other.handle_;
    other.handle_ = nullptr;
#else
    fd_ = other.fd_;
    other.fd_ = -1;
#endif

    return *this;
}

DataDirectoryLockError DataDirectoryLock::acquire(
    const std::filesystem::path& root)
{
    if (locked()) {
        return DataDirectoryLockError::already_locked;
    }

    const auto root_error =
        ensure_directory(root);

    if (root_error != DataDirectoryError::none) {
        return DataDirectoryLockError::io_error;
    }

    const auto path =
        root / ".lock";

#ifdef _WIN32
    HANDLE handle =
        CreateFileW(
            path.c_str(),
            GENERIC_READ | GENERIC_WRITE,
            0,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

    if (handle == INVALID_HANDLE_VALUE) {
        const DWORD error =
            GetLastError();

        return error == ERROR_SHARING_VIOLATION ||
                       error == ERROR_LOCK_VIOLATION
            ? DataDirectoryLockError::already_locked
            : DataDirectoryLockError::io_error;
    }

    handle_ = handle;
#else
    const int descriptor =
        ::open(
            path.c_str(),
            O_RDWR | O_CREAT,
            static_cast<mode_t>(0600)
        );

    if (descriptor < 0) {
        return DataDirectoryLockError::io_error;
    }

    if (::flock(
            descriptor,
            LOCK_EX | LOCK_NB) != 0) {
        const int error = errno;
        (void)::close(descriptor);

        return error == EWOULDBLOCK ||
                       error == EAGAIN
            ? DataDirectoryLockError::already_locked
            : DataDirectoryLockError::io_error;
    }

    fd_ = descriptor;
#endif

    return DataDirectoryLockError::none;
}

void DataDirectoryLock::release() noexcept
{
#ifdef _WIN32
    if (handle_ != nullptr) {
        (void)CloseHandle(
            static_cast<HANDLE>(handle_)
        );
        handle_ = nullptr;
    }
#else
    if (fd_ >= 0) {
        (void)::flock(fd_, LOCK_UN);
        (void)::close(fd_);
        fd_ = -1;
    }
#endif
}

bool DataDirectoryLock::locked() const noexcept
{
#ifdef _WIN32
    return handle_ != nullptr;
#else
    return fd_ >= 0;
#endif
}

} // namespace quintum
