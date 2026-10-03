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

constexpr std::array<Byte, 8> kMetadataMagicV1{
    'Q', 'W', 'M', 'E', 'T', 'A', '0', '1'
};
constexpr std::array<Byte, 8> kMetadataMagicV2{
    'Q', 'W', 'M', 'E', 'T', 'A', '0', '2'
};
constexpr std::uint32_t kMetadataVersionV1{1U};
constexpr std::uint32_t kMetadataVersionV2{2U};
constexpr std::size_t kChecksumSize{32U};
constexpr std::size_t kMetadataV2HeaderSize{
    kMetadataMagicV2.size() +
    sizeof(std::uint32_t) +
    4U +
    Hash256{}.size() +
    WalletNonce{}.size()
};
constexpr std::uintmax_t kMaxMetadataFileSize{
    8U * 1024U * 1024U
};
constexpr std::size_t kMaxMetadataEntries{10'000U};
constexpr std::size_t kMaxLabelBytes{128U};
constexpr std::size_t kMaxAddressBytes{90U};

Hash256 metadata_wallet_id(
    const crypto::PublicKey& public_key)
{
    Bytes bytes;
    constexpr std::string_view domain{
        "QUINTUM-WALLET-METADATA-V1"
    };

    append_compact_size(
        bytes,
        static_cast<std::uint64_t>(
            domain.size()
        )
    );

    for (const unsigned char ch : domain) {
        bytes.push_back(
            static_cast<Byte>(ch)
        );
    }

    bytes.insert(
        bytes.end(),
        public_key.begin(),
        public_key.end()
    );

    return crypto::double_sha256(bytes);
}

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

struct MetadataPlaintextGuard {
    Bytes* bytes{nullptr};

    ~MetadataPlaintextGuard()
    {
        if (bytes != nullptr) {
            crypto::secure_erase(*bytes);
        }
    }
};

void append_metadata_payload(
    Bytes& bytes,
    const std::map<std::string, std::string>& address_labels,
    const std::map<Hash256, std::string>& transaction_labels)
{
    append_compact_size(
        bytes,
        static_cast<std::uint64_t>(
            address_labels.size()
        )
    );

    for (const auto& [address, label] :
         address_labels) {
        append_string(bytes, address);
        append_string(bytes, label);
    }

    append_compact_size(
        bytes,
        static_cast<std::uint64_t>(
            transaction_labels.size()
        )
    );

    for (const auto& [txid, label] :
         transaction_labels) {
        bytes.insert(
            bytes.end(),
            txid.begin(),
            txid.end()
        );
        append_string(bytes, label);
    }
}

bool parse_metadata_payload(
    std::span<const Byte> payload,
    const consensus::ChainParams& params,
    std::map<std::string, std::string>& addresses,
    std::map<Hash256, std::string>& transactions)
{
    std::size_t offset{0U};

    const auto address_count =
        read_compact_size(
            payload,
            offset
        );

    if (!address_count ||
        *address_count >
            kMaxMetadataEntries) {
        return false;
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
            return false;
        }

        const auto decoded =
            decode_address(
                params.network,
                *address
            );

        if (!decoded.ok()) {
            return false;
        }

        const std::string canonical =
            encode_address(
                params.network,
                decoded.public_key
            );

        if (!loaded_addresses
                 .emplace(
                     canonical,
                     std::move(*label)
                 )
                 .second) {
            return false;
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
        return false;
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
            return false;
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
            return false;
        }
    }

    if (offset != payload.size()) {
        return false;
    }

    addresses =
        std::move(loaded_addresses);
    transactions =
        std::move(loaded_transactions);
    return true;
}

} // namespace

WalletMetadataError Wallet::load_metadata()
{
    address_labels_.clear();
    transaction_labels_.clear();
    metadata_wallet_id_ = Hash256{};
    metadata_wallet_id_valid_ = false;

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
            kMetadataMagicV1.size() +
                sizeof(std::uint32_t)) {
        return WalletMetadataError::corrupt;
    }

    std::array<Byte, 8> magic{};
    std::copy_n(
        bytes->begin(),
        static_cast<std::ptrdiff_t>(
            magic.size()),
        magic.begin()
    );

    const auto wallet_owns_id =
        [&](const Hash256& wallet_id) {
            return std::any_of(
                keys_.begin(),
                keys_.end(),
                [&](const KeyRecord& key) {
                    return metadata_wallet_id(
                               key.public_key) ==
                           wallet_id;
                }
            );
        };

    if (magic == kMetadataMagicV2) {
        if (!encrypted_ ||
            bytes->size() <
                kMetadataV2HeaderSize +
                    WalletTag{}.size()) {
            return WalletMetadataError::corrupt;
        }

        std::size_t offset{
            kMetadataMagicV2.size()
        };

        const auto version =
            read_little_endian<std::uint32_t>(
                *bytes,
                offset
            );

        if (!version ||
            *version != kMetadataVersionV2) {
            return WalletMetadataError::corrupt;
        }

        std::array<Byte, 4> message_start{};

        if (!read_exact(
                *bytes,
                offset,
                message_start)) {
            return WalletMetadataError::corrupt;
        }

        if (message_start !=
            params_.message_start) {
            return WalletMetadataError::wrong_network;
        }

        Hash256 stored_wallet_id{};

        if (!read_exact(
                *bytes,
                offset,
                stored_wallet_id)) {
            return WalletMetadataError::corrupt;
        }

        if (keys_.empty() ||
            !wallet_owns_id(
                stored_wallet_id)) {
            return WalletMetadataError::wrong_wallet;
        }

        WalletNonce nonce{};

        if (!read_exact(
                *bytes,
                offset,
                nonce) ||
            offset != kMetadataV2HeaderSize) {
            return WalletMetadataError::corrupt;
        }

        const std::size_t tag_offset =
            bytes->size() -
            WalletTag{}.size();

        if (tag_offset < offset) {
            return WalletMetadataError::corrupt;
        }

        WalletTag tag{};
        std::copy_n(
            bytes->begin() +
                static_cast<std::ptrdiff_t>(
                    tag_offset),
            static_cast<std::ptrdiff_t>(
                tag.size()),
            tag.begin()
        );

        const std::span<const Byte>
            associated_data{
                bytes->data(),
                kMetadataV2HeaderSize
            };

        const std::span<const Byte>
            ciphertext{
                bytes->data() + offset,
                tag_offset - offset
            };

        Bytes plaintext;
        MetadataPlaintextGuard guard{
            &plaintext
        };

        if (!decrypt_wallet_payload(
                ciphertext,
                associated_data,
                encryption_key_,
                nonce,
                tag,
                plaintext)) {
            return WalletMetadataError::corrupt;
        }

        if (!parse_metadata_payload(
                plaintext,
                params_,
                address_labels_,
                transaction_labels_)) {
            return WalletMetadataError::corrupt;
        }

        metadata_wallet_id_ =
            stored_wallet_id;
        metadata_wallet_id_valid_ = true;
        return WalletMetadataError::none;
    }

    if (magic != kMetadataMagicV1 ||
        bytes->size() <
            kMetadataMagicV1.size() +
                sizeof(std::uint32_t) +
                params_.message_start.size() +
                Hash256{}.size() +
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

    Hash256 stored_checksum{};
    std::copy_n(
        bytes->begin() +
            static_cast<std::ptrdiff_t>(
                payload_size),
        static_cast<std::ptrdiff_t>(
            stored_checksum.size()),
        stored_checksum.begin()
    );

    if (expected != stored_checksum) {
        return WalletMetadataError::corrupt;
    }

    const std::span<const Byte> legacy{
        bytes->data(),
        payload_size
    };

    std::size_t offset{
        kMetadataMagicV1.size()
    };

    const auto version =
        read_little_endian<std::uint32_t>(
            legacy,
            offset
        );

    if (!version ||
        *version != kMetadataVersionV1) {
        return WalletMetadataError::corrupt;
    }

    std::array<Byte, 4> message_start{};

    if (!read_exact(
            legacy,
            offset,
            message_start)) {
        return WalletMetadataError::corrupt;
    }

    if (message_start !=
        params_.message_start) {
        return WalletMetadataError::wrong_network;
    }

    Hash256 stored_wallet_id{};

    if (!read_exact(
            legacy,
            offset,
            stored_wallet_id)) {
        return WalletMetadataError::corrupt;
    }

    if (keys_.empty() ||
        !wallet_owns_id(
            stored_wallet_id)) {
        return WalletMetadataError::wrong_wallet;
    }

    if (!parse_metadata_payload(
            legacy.subspan(offset),
            params_,
            address_labels_,
            transaction_labels_)) {
        return WalletMetadataError::corrupt;
    }

    metadata_wallet_id_ =
        stored_wallet_id;
    metadata_wallet_id_valid_ = true;

    // Encrypted wallets migrate legacy plaintext metadata immediately.
    if (encrypted_) {
        const auto migrated =
            save_metadata();

        if (migrated !=
            WalletMetadataError::none) {
            return migrated;
        }
    }

    return WalletMetadataError::none;
}

WalletMetadataError Wallet::save_metadata()
{
    if (address_labels_.size() >
            kMaxMetadataEntries ||
        transaction_labels_.size() >
            kMaxMetadataEntries) {
        return WalletMetadataError::
            too_many_entries;
    }

    if (keys_.empty()) {
        return WalletMetadataError::corrupt;
    }

    if (!metadata_wallet_id_valid_) {
        const auto deterministic_anchor =
            std::find_if(
                keys_.begin(),
                keys_.end(),
                [](const KeyRecord& key) {
                    return key.deterministic &&
                           !key.internal &&
                           key.hd_index == 0U;
                }
            );

        const auto& anchor =
            deterministic_anchor !=
                    keys_.end()
                ? *deterministic_anchor
                : keys_.front();

        metadata_wallet_id_ =
            metadata_wallet_id(
                anchor.public_key
            );
        metadata_wallet_id_valid_ = true;
    }

    Bytes payload;
    payload.reserve(
        address_labels_.size() * 128U +
        transaction_labels_.size() * 160U +
        16U
    );
    MetadataPlaintextGuard payload_guard{
        &payload
    };

    append_metadata_payload(
        payload,
        address_labels_,
        transaction_labels_
    );

    Bytes bytes;

    if (encrypted_) {
        bytes.reserve(
            kMetadataV2HeaderSize +
            payload.size() +
            WalletTag{}.size()
        );

        bytes.insert(
            bytes.end(),
            kMetadataMagicV2.begin(),
            kMetadataMagicV2.end()
        );

        append_little_endian(
            bytes,
            kMetadataVersionV2
        );

        bytes.insert(
            bytes.end(),
            params_.message_start.begin(),
            params_.message_start.end()
        );

        bytes.insert(
            bytes.end(),
            metadata_wallet_id_.begin(),
            metadata_wallet_id_.end()
        );

        WalletNonce nonce{};

        if (!crypto::secure_random_bytes(
                nonce)) {
            return WalletMetadataError::io_error;
        }

        bytes.insert(
            bytes.end(),
            nonce.begin(),
            nonce.end()
        );

        const std::span<const Byte>
            associated_data{
                bytes.data(),
                bytes.size()
            };

        Bytes ciphertext;
        WalletTag tag{};

        if (!encrypt_wallet_payload(
                payload,
                associated_data,
                encryption_key_,
                nonce,
                ciphertext,
                tag)) {
            return WalletMetadataError::io_error;
        }

        bytes.insert(
            bytes.end(),
            ciphertext.begin(),
            ciphertext.end()
        );
        bytes.insert(
            bytes.end(),
            tag.begin(),
            tag.end()
        );
    } else {
        bytes.reserve(
            kMetadataMagicV1.size() +
            sizeof(std::uint32_t) +
            params_.message_start.size() +
            metadata_wallet_id_.size() +
            payload.size() +
            kChecksumSize
        );

        bytes.insert(
            bytes.end(),
            kMetadataMagicV1.begin(),
            kMetadataMagicV1.end()
        );

        append_little_endian(
            bytes,
            kMetadataVersionV1
        );

        bytes.insert(
            bytes.end(),
            params_.message_start.begin(),
            params_.message_start.end()
        );

        bytes.insert(
            bytes.end(),
            metadata_wallet_id_.begin(),
            metadata_wallet_id_.end()
        );

        bytes.insert(
            bytes.end(),
            payload.begin(),
            payload.end()
        );

        const Hash256 checksum =
            crypto::double_sha256(bytes);

        bytes.insert(
            bytes.end(),
            checksum.begin(),
            checksum.end()
        );
    }

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

    const std::string key =
        encode_address(
            params_.network,
            decoded.public_key
        );

    if (label.empty()) {
        const auto found =
            address_labels_.find(key);

        if (found ==
            address_labels_.end()) {
            return WalletMetadataError::none;
        }

        const std::string old =
            found->second;

        address_labels_.erase(found);

        const auto saved =
            save_metadata();

        if (saved !=
            WalletMetadataError::none) {
            address_labels_[key] = old;
        }

        return saved;
    }

    if (!valid_label(label)) {
        return WalletMetadataError::
            invalid_label;
    }

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
        const auto found =
            transaction_labels_.find(txid);

        if (found ==
            transaction_labels_.end()) {
            return WalletMetadataError::none;
        }

        const std::string old =
            found->second;

        transaction_labels_.erase(found);

        const auto saved =
            save_metadata();

        if (saved !=
            WalletMetadataError::none) {
            transaction_labels_[txid] =
                old;
        }

        return saved;
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
