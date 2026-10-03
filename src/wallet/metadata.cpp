#include "wallet/wallet.hpp"

#include "core/serialize.hpp"
#include "crypto/sha256.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <limits>
#include <span>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <io.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace quintum::wallet {
namespace {

constexpr std::array<Byte, 8> kMetadataMagic{
    'Q', 'W', 'M', 'E', 'T', 'A', '0', '1'
};
constexpr std::uint32_t kMetadataVersion{1U};
constexpr std::size_t kChecksumSize{32U};
constexpr std::uintmax_t kMaxMetadataFileSize{
    8U * 1024U * 1024U
};
constexpr std::size_t kMaxMetadataEntries{10'000U};
constexpr std::size_t kMaxLabelBytes{128U};
constexpr std::size_t kMaxAddressBytes{90U};

bool valid_label(std::string_view label) noexcept
{
    if (label.empty() ||
        label.size() > kMaxLabelBytes) {
        return false;
    }

    for (const unsigned char ch : label) {
        if (ch < 0x20U || ch == 0x7fU) {
            return false;
        }
    }

    return true;
}

void append_string(
    Bytes& out,
    std::string_view value)
{
    append_compact_size(
        out,
        static_cast<std::uint64_t>(
            value.size()
        )
    );

    for (const unsigned char ch : value) {
        out.push_back(
            static_cast<Byte>(ch)
        );
    }
}

std::optional<std::string> read_string(
    std::span<const Byte> bytes,
    std::size_t& offset,
    std::size_t maximum)
{
    const auto size =
        read_compact_size(
            bytes,
            offset
        );

    if (!size ||
        *size > maximum ||
        *size >
            static_cast<std::uint64_t>(
                bytes.size() - std::min(
                    offset,
                    bytes.size()
                ))) {
        return std::nullopt;
    }

    if (offset > bytes.size() ||
        *size >
            static_cast<std::uint64_t>(
                bytes.size() - offset)) {
        return std::nullopt;
    }

    std::string out;
    out.reserve(
        static_cast<std::size_t>(*size)
    );

    for (std::uint64_t i = 0U;
         i < *size;
         ++i) {
        out.push_back(
            static_cast<char>(
                bytes[offset++]
            )
        );
    }

    return out;
}

bool flush_file(std::FILE* file) noexcept
{
    if (std::fflush(file) != 0) {
        return false;
    }

#ifdef _WIN32
    return _commit(_fileno(file)) == 0;
#else
    return ::fsync(fileno(file)) == 0;
#endif
}

bool replace_file(
    const std::filesystem::path& source,
    const std::filesystem::path& destination)
{
#ifdef _WIN32
    return MoveFileExW(
               source.c_str(),
               destination.c_str(),
               MOVEFILE_REPLACE_EXISTING |
                   MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (::rename(
            source.c_str(),
            destination.c_str()) != 0) {
        return false;
    }

    auto directory =
        destination.parent_path();

    if (directory.empty()) {
        directory = ".";
    }

    const int fd = ::open(
        directory.c_str(),
        O_RDONLY | O_DIRECTORY
    );

    if (fd >= 0) {
        (void)::fsync(fd);
        (void)::close(fd);
    }

    return true;
#endif
}

WalletMetadataError write_atomic(
    const std::filesystem::path& destination,
    std::span<const Byte> bytes)
{
    std::error_code ec;

    const auto parent =
        destination.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(
            parent,
            ec
        );

        if (ec) {
            return WalletMetadataError::io_error;
        }
    }

    auto temporary = destination;
    temporary += ".tmp";

#ifdef _WIN32
    std::FILE* file{nullptr};

    if (_wfopen_s(
            &file,
            temporary.c_str(),
            L"wb") != 0) {
        file = nullptr;
    }
#else
    std::FILE* file =
        std::fopen(
            temporary.c_str(),
            "wb"
        );
#endif

    if (file == nullptr) {
        return WalletMetadataError::io_error;
    }

#ifndef _WIN32
    if (::chmod(
            temporary.c_str(),
            S_IRUSR | S_IWUSR) != 0) {
        (void)std::fclose(file);
        std::filesystem::remove(
            temporary,
            ec
        );
        return WalletMetadataError::io_error;
    }
#endif

    const std::size_t written =
        bytes.empty()
            ? 0U
            : std::fwrite(
                  bytes.data(),
                  1U,
                  bytes.size(),
                  file
              );

    const bool durable =
        written == bytes.size() &&
        flush_file(file);

    const bool closed =
        std::fclose(file) == 0;

    if (!durable ||
        !closed ||
        !replace_file(
            temporary,
            destination)) {
        std::filesystem::remove(
            temporary,
            ec
        );
        return WalletMetadataError::io_error;
    }

    return WalletMetadataError::none;
}

std::optional<Bytes> read_file(
    const std::filesystem::path& path)
{
    std::error_code ec;

    if (!std::filesystem::exists(
            path,
            ec)) {
        return Bytes{};
    }

    if (ec ||
        !std::filesystem::is_regular_file(
            path,
            ec) ||
        ec) {
        return std::nullopt;
    }

    const auto size =
        std::filesystem::file_size(
            path,
            ec
        );

    if (ec ||
        size > kMaxMetadataFileSize ||
        size >
            static_cast<std::uintmax_t>(
                std::numeric_limits<
                    std::size_t>::max())) {
        return std::nullopt;
    }

    std::ifstream input(
        path,
        std::ios::binary
    );

    if (!input) {
        return std::nullopt;
    }

    Bytes bytes(
        static_cast<std::size_t>(size)
    );

    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(
                bytes.data()),
            static_cast<std::streamsize>(
                bytes.size())
        );
    }

    if (!input) {
        return std::nullopt;
    }

    return bytes;
}

bool read_exact(
    std::span<const Byte> bytes,
    std::size_t& offset,
    std::span<Byte> out) noexcept
{
    if (offset > bytes.size() ||
        out.size() >
            bytes.size() - offset) {
        return false;
    }

    std::copy_n(
        bytes.begin() +
            static_cast<std::ptrdiff_t>(
                offset),
        static_cast<std::ptrdiff_t>(
            out.size()),
        out.begin()
    );

    offset += out.size();
    return true;
}

} // namespace

WalletMetadataError Wallet::load_metadata()
{
    address_labels_.clear();
    transaction_labels_.clear();

    std::error_code exists_ec;
    const bool exists =
        std::filesystem::exists(
            metadata_path_,
            exists_ec
        );

    if (exists_ec) {
        return WalletMetadataError::io_error;
    }

    if (!exists) {
        return WalletMetadataError::none;
    }

    auto bytes =
        read_file(metadata_path_);

    if (!bytes ||
        bytes->size() <
            kMetadataMagic.size() +
                sizeof(std::uint32_t) +
                params_.message_start.size() +
                2U +
                kChecksumSize) {
        return WalletMetadataError::corrupt;
    }

    const std::size_t payload_size =
        bytes->size() - kChecksumSize;

    const Hash256 expected =
        crypto::double_sha256(
            std::span<const Byte>{
                bytes->data(),
                payload_size
            }
        );

    Hash256 stored{};
    std::copy_n(
        bytes->begin() +
            static_cast<std::ptrdiff_t>(
                payload_size),
        static_cast<std::ptrdiff_t>(
            stored.size()),
        stored.begin()
    );

    if (expected != stored) {
        return WalletMetadataError::corrupt;
    }

    const std::span<const Byte> payload{
        bytes->data(),
        payload_size
    };

    std::size_t offset{0U};

    std::array<Byte, 8> magic{};
    if (!read_exact(
            payload,
            offset,
            magic) ||
        magic != kMetadataMagic) {
        return WalletMetadataError::corrupt;
    }

    const auto version =
        read_little_endian<std::uint32_t>(
            payload,
            offset
        );

    if (!version ||
        *version != kMetadataVersion) {
        return WalletMetadataError::corrupt;
    }

    std::array<Byte, 4> message_start{};
    if (!read_exact(
            payload,
            offset,
            message_start)) {
        return WalletMetadataError::corrupt;
    }

    if (message_start !=
        params_.message_start) {
        return WalletMetadataError::wrong_network;
    }

    const auto address_count =
        read_compact_size(
            payload,
            offset
        );

    if (!address_count ||
        *address_count >
            kMaxMetadataEntries) {
        return WalletMetadataError::corrupt;
    }

    std::map<std::string, std::string>
        loaded_addresses;

    for (std::uint64_t i = 0U;
         i < *address_count;
         ++i) {
        auto address =
            read_string(
                payload,
                offset,
                kMaxAddressBytes
            );

        auto label =
            read_string(
                payload,
                offset,
                kMaxLabelBytes
            );

        if (!address ||
            !label ||
            !valid_label(*label)) {
            return WalletMetadataError::corrupt;
        }

        const auto decoded =
            decode_address(
                params_.network,
                *address
            );

        if (!decoded.ok()) {
            return WalletMetadataError::corrupt;
        }

        if (!loaded_addresses
                 .emplace(
                     std::move(*address),
                     std::move(*label)
                 )
                 .second) {
            return WalletMetadataError::corrupt;
        }
    }

    const auto transaction_count =
        read_compact_size(
            payload,
            offset
        );

    if (!transaction_count ||
        *transaction_count >
            kMaxMetadataEntries) {
        return WalletMetadataError::corrupt;
    }

    std::map<Hash256, std::string>
        loaded_transactions;

    for (std::uint64_t i = 0U;
         i < *transaction_count;
         ++i) {
        Hash256 txid{};

        if (!read_exact(
                payload,
                offset,
                txid)) {
            return WalletMetadataError::corrupt;
        }

        auto label =
            read_string(
                payload,
                offset,
                kMaxLabelBytes
            );

        if (!label ||
            !valid_label(*label) ||
            !loaded_transactions
                 .emplace(
                     txid,
                     std::move(*label)
                 )
                 .second) {
            return WalletMetadataError::corrupt;
        }
    }

    if (offset != payload.size()) {
        return WalletMetadataError::corrupt;
    }

    address_labels_ =
        std::move(loaded_addresses);
    transaction_labels_ =
        std::move(loaded_transactions);

    return WalletMetadataError::none;
}

WalletMetadataError Wallet::save_metadata() const
{
    if (address_labels_.size() >
            kMaxMetadataEntries ||
        transaction_labels_.size() >
            kMaxMetadataEntries) {
        return WalletMetadataError::
            too_many_entries;
    }

    Bytes bytes;
    bytes.reserve(
        64U +
        address_labels_.size() * 128U +
        transaction_labels_.size() * 160U
    );

    bytes.insert(
        bytes.end(),
        kMetadataMagic.begin(),
        kMetadataMagic.end()
    );

    append_little_endian(
        bytes,
        kMetadataVersion
    );

    bytes.insert(
        bytes.end(),
        params_.message_start.begin(),
        params_.message_start.end()
    );

    append_compact_size(
        bytes,
        static_cast<std::uint64_t>(
            address_labels_.size()
        )
    );

    for (const auto& [address, label] :
         address_labels_) {
        append_string(bytes, address);
        append_string(bytes, label);
    }

    append_compact_size(
        bytes,
        static_cast<std::uint64_t>(
            transaction_labels_.size()
        )
    );

    for (const auto& [txid, label] :
         transaction_labels_) {
        bytes.insert(
            bytes.end(),
            txid.begin(),
            txid.end()
        );
        append_string(bytes, label);
    }

    const Hash256 checksum =
        crypto::double_sha256(bytes);

    bytes.insert(
        bytes.end(),
        checksum.begin(),
        checksum.end()
    );

    if (bytes.size() >
        kMaxMetadataFileSize) {
        return WalletMetadataError::
            too_many_entries;
    }

    return write_atomic(
        metadata_path_,
        bytes
    );
}

WalletMetadataError Wallet::set_address_label(
    std::string_view address,
    std::string_view label)
{
    if (!started_) {
        return WalletMetadataError::io_error;
    }

    const auto decoded =
        decode_address(
            params_.network,
            address
        );

    if (!decoded.ok()) {
        return decoded.error ==
                AddressError::wrong_network
            ? WalletMetadataError::
                  wrong_network_address
            : WalletMetadataError::
                  invalid_address;
    }

    if (label.empty()) {
        address_labels_.erase(
            std::string{address}
        );
        return save_metadata();
    }

    if (!valid_label(label)) {
        return WalletMetadataError::
            invalid_label;
    }

    const std::string key{address};

    const auto previous =
        address_labels_.find(key);

    std::optional<std::string> old;

    if (previous !=
        address_labels_.end()) {
        old = previous->second;
    }

    address_labels_[key] =
        std::string{label};

    const auto saved =
        save_metadata();

    if (saved !=
        WalletMetadataError::none) {
        if (old) {
            address_labels_[key] =
                std::move(*old);
        } else {
            address_labels_.erase(key);
        }
    }

    return saved;
}

WalletMetadataError Wallet::set_transaction_label(
    const Hash256& txid,
    std::string_view label)
{
    if (!started_) {
        return WalletMetadataError::io_error;
    }

    if (label.empty()) {
        transaction_labels_.erase(txid);
        return save_metadata();
    }

    if (!valid_label(label)) {
        return WalletMetadataError::
            invalid_label;
    }

    const auto previous =
        transaction_labels_.find(txid);

    std::optional<std::string> old;

    if (previous !=
        transaction_labels_.end()) {
        old = previous->second;
    }

    transaction_labels_[txid] =
        std::string{label};

    const auto saved =
        save_metadata();

    if (saved !=
        WalletMetadataError::none) {
        if (old) {
            transaction_labels_[txid] =
                std::move(*old);
        } else {
            transaction_labels_.erase(txid);
        }
    }

    return saved;
}

std::vector<WalletAddressBookEntry>
Wallet::address_book() const
{
    std::vector<WalletAddressBookEntry> out;
    out.reserve(address_labels_.size());

    for (const auto& [address, label] :
         address_labels_) {
        out.push_back(
            WalletAddressBookEntry{
                .address = address,
                .label = label,
            }
        );
    }

    return out;
}

std::optional<std::string>
Wallet::transaction_label(
    const Hash256& txid) const
{
    const auto found =
        transaction_labels_.find(txid);

    if (found ==
        transaction_labels_.end()) {
        return std::nullopt;
    }

    return found->second;
}

} // namespace quintum::wallet
