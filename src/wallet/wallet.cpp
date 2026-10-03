#include "wallet/wallet.hpp"
#include "wallet/file_security.hpp"

#include "consensus/monetary.hpp"
#include "core/serialize.hpp"
#include "crypto/random.hpp"
#include "crypto/sha256.hpp"
#include "wallet/fee_policy.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <fstream>
#include <limits>
#include <system_error>
#include <utility>

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

constexpr std::array<Byte, 8> kWalletMagic{
    'Q', 'W', 'A', 'L', 'L', 'E', 'T', '1'
};
constexpr std::uint32_t kLegacyWalletVersion{1U};
constexpr std::uint32_t kEncryptedWalletVersion{2U};
constexpr std::size_t kChecksumSize{32U};
constexpr std::size_t kEncryptedTagSize{WalletTag{}.size()};
constexpr std::size_t kEncryptedKeyRecordSize{
    1U + 1U + sizeof(std::uint32_t) +
    crypto::PrivateKey{}.size()
};
constexpr std::uint32_t kMaxArgon2MemoryBlocks{262'144U};
constexpr std::uint32_t kMaxArgon2Passes{10U};
constexpr Byte kInternalFlag{0x01U};
constexpr Byte kUsedFlag{0x02U};
constexpr Byte kKnownKeyFlags{
    kInternalFlag | kUsedFlag
};
constexpr std::size_t kKeyRecordSize{
    1U + crypto::PrivateKey{}.size()
};
constexpr std::array<Byte, 8> kBackupBundleMagic{
    'Q', 'T', 'M', 'B', 'K', 'P', '0', '1'
};
constexpr std::uint32_t kBackupBundleVersion{1U};
constexpr std::uintmax_t kMaxBackupBundleSize{
    20U * 1024U * 1024U
};

enum class PathTargetRelation {
    distinct,
    same,
    io_error,
};

PathTargetRelation path_target_relation(
    const std::filesystem::path& lhs,
    const std::filesystem::path& rhs) noexcept
{
    std::error_code ec;
    const bool lhs_exists =
        std::filesystem::exists(lhs, ec);

    if (ec) {
        return PathTargetRelation::io_error;
    }

    const bool rhs_exists =
        std::filesystem::exists(rhs, ec);

    if (ec) {
        return PathTargetRelation::io_error;
    }

    if (lhs_exists && rhs_exists) {
        const bool equivalent =
            std::filesystem::equivalent(
                lhs,
                rhs,
                ec
            );

        if (ec) {
            return PathTargetRelation::io_error;
        }

        if (equivalent) {
            return PathTargetRelation::same;
        }
    }

    const auto canonical_lhs =
        std::filesystem::weakly_canonical(
            lhs,
            ec
        );

    if (ec) {
        return PathTargetRelation::io_error;
    }

    const auto canonical_rhs =
        std::filesystem::weakly_canonical(
            rhs,
            ec
        );

    if (ec) {
        return PathTargetRelation::io_error;
    }

    return canonical_lhs == canonical_rhs
        ? PathTargetRelation::same
        : PathTargetRelation::distinct;
}

WalletStoreError validate_backup_destination(
    const std::filesystem::path& destination,
    const std::filesystem::path& wallet_path,
    const std::filesystem::path& state_path,
    const std::filesystem::path& metadata_path) noexcept
{
    for (const auto* protected_path : {
             &wallet_path,
             &state_path,
             &metadata_path}) {
        const auto relation =
            path_target_relation(
                destination,
                *protected_path
            );

        if (relation ==
            PathTargetRelation::io_error) {
            return WalletStoreError::io_error;
        }

        if (relation ==
            PathTargetRelation::same) {
            return WalletStoreError::unsafe_destination;
        }
    }

    return WalletStoreError::none;
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

    // The temporary file is chmod(0600) before the rename.
    // rename() preserves that mode, so there is no fallible
    // post-commit permission change that could report failure
    // after the durable file has already replaced the old one.
    std::filesystem::path directory =
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

WalletStoreError write_atomic(
    const std::filesystem::path& destination,
    std::span<const Byte> bytes,
    bool overwrite)
{
    std::error_code ec;

    if (!overwrite &&
        std::filesystem::exists(
            destination,
            ec)) {
        return ec
            ? WalletStoreError::io_error
            : WalletStoreError::target_exists;
    }

    if (ec) {
        return WalletStoreError::io_error;
    }

    std::filesystem::path parent =
        destination.parent_path();

    if (!parent.empty()) {
        std::filesystem::create_directories(
            parent,
            ec
        );

        if (ec) {
            return WalletStoreError::io_error;
        }
    }

    std::filesystem::path temporary =
        destination;
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
        return WalletStoreError::io_error;
    }

    if (!restrict_file_permissions(temporary)) {
        (void)std::fclose(file);
        std::filesystem::remove(
            temporary,
            ec
        );
        return WalletStoreError::io_error;
    }

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
        return WalletStoreError::io_error;
    }

    return WalletStoreError::none;
}

std::optional<Bytes> read_file_limited(
    const std::filesystem::path& path,
    std::uintmax_t max_size)
{
    std::ifstream input(
        path,
        std::ios::binary
    );

    if (!input) {
        return std::nullopt;
    }

    input.seekg(0, std::ios::end);
    const auto end = input.tellg();

    if (end < 0) {
        return std::nullopt;
    }

    const auto size =
        static_cast<std::uintmax_t>(end);

    if (size > max_size ||
        size >
            static_cast<std::uintmax_t>(
                std::numeric_limits<
                    std::size_t>::max())) {
        return std::nullopt;
    }

    Bytes bytes(
        static_cast<std::size_t>(size)
    );

    input.seekg(0, std::ios::beg);

    if (!bytes.empty()) {
        input.read(
            reinterpret_cast<char*>(
                bytes.data()),
            static_cast<std::streamsize>(
                bytes.size())
        );
    }

    if (!input) {
        crypto::secure_erase(bytes);
        return std::nullopt;
    }

    return bytes;
}

std::optional<Bytes> read_file(
    const std::filesystem::path& path)
{
    constexpr std::uintmax_t max_size{
        8U * 1024U * 1024U
    };

    return read_file_limited(
        path,
        max_size
    );
}

bool add_amount(
    Amount& total,
    Amount value) noexcept
{
    if (!consensus::money_range(value) ||
        value >
            consensus::kMaxMoney - total) {
        return false;
    }

    total += value;
    return true;
}

struct SecretBytesGuard {
    Bytes* bytes{nullptr};

    ~SecretBytesGuard()
    {
        if (bytes != nullptr) {
            crypto::secure_erase(*bytes);
        }
    }
};

template <typename Array>
struct SecretArrayGuard {
    Array* value{nullptr};

    ~SecretArrayGuard()
    {
        if (value != nullptr) {
            crypto::secure_erase(*value);
        }
    }
};

bool mature_at_height(
    const Coin& coin,
    std::uint32_t spend_height) noexcept
{
    if (!coin.coinbase) {
        return true;
    }

    return spend_height >= coin.height &&
           spend_height - coin.height >=
               consensus::kCoinbaseMaturity;
}

} // namespace

Wallet::Wallet(
    const consensus::ChainParams& params,
    std::filesystem::path directory)
    : params_(params),
      directory_(std::move(directory)),
      path_(directory_ / "wallet.dat"),
      state_path_(directory_ / "wallet_state.dat"),
      metadata_path_(directory_ / "wallet_meta.dat")
{
}

Wallet::~Wallet()
{
    clear_keys();
    crypto::secure_erase(encryption_key_);
    crypto::secure_erase(encryption_salt_);

    if (recovery_seed_) {
        crypto::secure_erase(*recovery_seed_);
        recovery_seed_.reset();
    }
}

WalletStartResult Wallet::start(
    std::string_view passphrase)
{
    WalletStartResult out;

    if (started_) {
        const auto existing =
            addresses();

        if (!existing.empty()) {
            out.receive_address =
                existing.back();
        }

        return out;
    }

    out.store_error = load(passphrase);

    if (out.store_error ==
        WalletStoreError::none) {
        out.metadata_error =
            load_metadata();

        if (out.metadata_error !=
            WalletMetadataError::none) {
            clear_keys();
            crypto::secure_erase(
                encryption_key_);
            crypto::secure_erase(
                encryption_salt_);

            if (recovery_seed_) {
                crypto::secure_erase(
                    *recovery_seed_);
                recovery_seed_.reset();
            }

            encrypted_ = false;
            out.error =
                WalletStartError::
                    metadata_failed;
            return out;
        }

        started_ = true;
        load_index_state();

        const auto existing =
            addresses();

        if (!existing.empty()) {
            out.receive_address =
                existing.back();
        }

        return out;
    }

    if (out.store_error !=
        WalletStoreError::not_found) {
        out.error =
            WalletStartError::store_failed;
        return out;
    }

    if (!passphrase.empty()) {
        encrypted_ = true;

        if (!crypto::secure_random_bytes(
                encryption_salt_)) {
            encrypted_ = false;
            out.error =
                WalletStartError::key_generation_failed;
            return out;
        }

        RecoverySeed seed{};
        if (!crypto::secure_random_bytes(seed)) {
            encrypted_ = false;
            crypto::secure_erase(encryption_salt_);
            out.error =
                WalletStartError::key_generation_failed;
            return out;
        }

        recovery_seed_ = seed;
        crypto::secure_erase(seed);

        if (!derive_wallet_encryption_key(
                passphrase,
                encryption_salt_,
                argon2_memory_blocks_,
                argon2_passes_,
                encryption_key_)) {
            encrypted_ = false;
            crypto::secure_erase(encryption_salt_);
            crypto::secure_erase(encryption_key_);
            crypto::secure_erase(*recovery_seed_);
            recovery_seed_.reset();
            out.store_error =
                WalletStoreError::crypto_error;
            out.error =
                WalletStartError::store_failed;
            return out;
        }
    }

    std::vector<KeyRecord> initial;

    if (!generate_pool_records(
            initial,
            false,
            kWalletKeypoolSize + 1U) ||
        !generate_pool_records(
            initial,
            true,
            kWalletKeypoolSize)) {
        for (auto& record : initial) {
            crypto::secure_erase(
                record.private_key
            );
        }

        if (encrypted_) {
            crypto::secure_erase(encryption_key_);
            crypto::secure_erase(encryption_salt_);
            if (recovery_seed_) {
                crypto::secure_erase(*recovery_seed_);
                recovery_seed_.reset();
            }
            encrypted_ = false;
        }

        out.error =
            WalletStartError::
                key_generation_failed;
        return out;
    }

    initial.front().used = true;

    const crypto::PublicKey primary =
        initial.front().public_key;

    out.store_error =
        save_keys(initial);

    if (out.store_error !=
        WalletStoreError::none) {
        for (auto& record : initial) {
            crypto::secure_erase(
                record.private_key
            );
        }

        if (encrypted_) {
            crypto::secure_erase(encryption_key_);
            crypto::secure_erase(encryption_salt_);
            if (recovery_seed_) {
                crypto::secure_erase(*recovery_seed_);
                recovery_seed_.reset();
            }
            encrypted_ = false;
        }

        out.error =
            WalletStartError::store_failed;
        return out;
    }

    keys_ = std::move(initial);
    address_labels_.clear();
    transaction_labels_.clear();
    metadata_wallet_id_ = Hash256{};
    metadata_wallet_id_valid_ = false;
    started_ = true;
    reset_index_state();
    out.created = true;
    out.backup_recommended = true;
    out.receive_address =
        encode_address(
            params_.network,
            primary
        );

    return out;
}

bool Wallet::started() const noexcept
{
    return started_;
}

WalletKeyResult
Wallet::new_receive_address()
{
    if (!started_) {
        WalletKeyResult out;
        out.error =
            WalletKeyError::not_started;
        return out;
    }

    return reserve_key(false);
}

WalletStoreError Wallet::encrypt_wallet(
    std::string_view passphrase)
{
    if (!started_) {
        return WalletStoreError::io_error;
    }

    if (encrypted_) {
        return WalletStoreError::none;
    }

    if (passphrase.empty()) {
        return WalletStoreError::
            invalid_passphrase;
    }

    auto original_wallet =
        read_file(path_);

    if (!original_wallet) {
        return WalletStoreError::io_error;
    }

    SecretBytesGuard wallet_guard{
        &*original_wallet
    };

    std::error_code ec;
    const bool metadata_existed =
        std::filesystem::exists(
            metadata_path_,
            ec
        );

    if (ec) {
        return WalletStoreError::io_error;
    }

    Bytes original_metadata;

    if (metadata_existed) {
        auto loaded =
            read_file(metadata_path_);

        if (!loaded) {
            return WalletStoreError::io_error;
        }

        original_metadata =
            std::move(*loaded);
    }

    SecretBytesGuard metadata_guard{
        &original_metadata
    };

    WalletSalt salt{};
    RecoverySeed seed{};
    WalletEncryptionKey key{};

    SecretArrayGuard salt_guard{&salt};
    SecretArrayGuard seed_guard{&seed};
    SecretArrayGuard key_guard{&key};

    if (!crypto::secure_random_bytes(salt) ||
        !crypto::secure_random_bytes(seed)) {
        return WalletStoreError::crypto_error;
    }

    if (!derive_wallet_encryption_key(
            passphrase,
            salt,
            kWalletArgon2MemoryBlocks,
            kWalletArgon2Passes,
            key)) {
        return WalletStoreError::crypto_error;
    }

    encrypted_ = true;
    recovery_seed_ = seed;
    encryption_key_ = key;
    encryption_salt_ = salt;
    argon2_memory_blocks_ =
        kWalletArgon2MemoryBlocks;
    argon2_passes_ =
        kWalletArgon2Passes;

    const auto clear_encryption_state =
        [&]() noexcept {
            encrypted_ = false;
            crypto::secure_erase(
                encryption_key_);
            crypto::secure_erase(
                encryption_salt_);

            if (recovery_seed_) {
                crypto::secure_erase(
                    *recovery_seed_);
                recovery_seed_.reset();
            }
        };

    const auto result =
        save_keys(keys_);

    if (result != WalletStoreError::none) {
        clear_encryption_state();
        return result;
    }

    // Encrypt/migrate metadata in the same user operation. If it cannot
    // be committed, restore the original wallet and metadata so Encrypt
    // does not leave half-migrated privacy state.
    const auto metadata_result =
        save_metadata();

    if (metadata_result !=
        WalletMetadataError::none) {
        bool rollback_ok =
            write_atomic(
                path_,
                *original_wallet,
                true
            ) == WalletStoreError::none;

        if (metadata_existed) {
            rollback_ok =
                rollback_ok &&
                write_atomic(
                    metadata_path_,
                    original_metadata,
                    true
                ) == WalletStoreError::none;
        } else {
            std::error_code remove_ec;
            std::filesystem::remove(
                metadata_path_,
                remove_ec
            );

            if (remove_ec) {
                rollback_ok = false;
            }
        }

        if (rollback_ok) {
            clear_encryption_state();
        }

        return WalletStoreError::io_error;
    }

    return WalletStoreError::none;
}

WalletStoreError Wallet::recover_from_seed(
    const RecoverySeed& seed,
    std::string_view passphrase)
{
    if (started_) {
        return WalletStoreError::target_exists;
    }

    if (passphrase.empty()) {
        return WalletStoreError::
            invalid_passphrase;
    }

    const bool seed_is_nonzero =
        std::any_of(
            seed.begin(),
            seed.end(),
            [](Byte value) {
                return value != 0U;
            }
        );

    if (!seed_is_nonzero) {
        return WalletStoreError::crypto_error;
    }

    std::error_code ec;
    if (std::filesystem::exists(
            path_,
            ec)) {
        return ec
            ? WalletStoreError::io_error
            : WalletStoreError::target_exists;
    }

    if (ec) {
        return WalletStoreError::io_error;
    }

    WalletSalt salt{};
    WalletEncryptionKey key{};

    SecretArrayGuard salt_guard{&salt};
    SecretArrayGuard key_guard{&key};

    if (!crypto::secure_random_bytes(salt) ||
        !derive_wallet_encryption_key(
            passphrase,
            salt,
            kWalletArgon2MemoryBlocks,
            kWalletArgon2Passes,
            key)) {
        return WalletStoreError::crypto_error;
    }

    encrypted_ = true;
    recovery_seed_ = seed;
    encryption_key_ = key;
    encryption_salt_ = salt;
    argon2_memory_blocks_ =
        kWalletArgon2MemoryBlocks;
    argon2_passes_ =
        kWalletArgon2Passes;

    std::vector<KeyRecord> initial;

    if (!generate_pool_records(
            initial,
            false,
            kWalletKeypoolSize + 1U) ||
        !generate_pool_records(
            initial,
            true,
            kWalletKeypoolSize)) {
        encrypted_ = false;
        crypto::secure_erase(
            encryption_key_);
        crypto::secure_erase(
            encryption_salt_);
        crypto::secure_erase(
            *recovery_seed_);
        recovery_seed_.reset();
        return WalletStoreError::crypto_error;
    }

    initial.front().used = true;

    const auto result =
        save_keys(initial);

    if (result != WalletStoreError::none) {
        encrypted_ = false;
        crypto::secure_erase(
            encryption_key_);
        crypto::secure_erase(
            encryption_salt_);
        crypto::secure_erase(
            *recovery_seed_);
        recovery_seed_.reset();
        return result;
    }

    keys_ = std::move(initial);
    address_labels_.clear();
    transaction_labels_.clear();
    metadata_wallet_id_ = Hash256{};
    metadata_wallet_id_valid_ = false;
    started_ = true;
    reset_index_state();
    return WalletStoreError::none;
}

WalletRecoveryResult Wallet::recover_from_mnemonic(
    std::string_view mnemonic,
    std::string_view passphrase,
    const Chainstate& chain,
    const Mempool& mempool,
    std::uint32_t gap_limit)
{
    WalletRecoveryResult out;

    if (started_) {
        out.error =
            WalletRecoveryError::already_started;
        return out;
    }

    if (passphrase.empty()) {
        out.error =
            WalletRecoveryError::invalid_passphrase;
        out.store_error =
            WalletStoreError::invalid_passphrase;
        return out;
    }

    if (gap_limit == 0U ||
        gap_limit >
            static_cast<std::uint32_t>(
                kMaxWalletKeys)) {
        out.error =
            WalletRecoveryError::invalid_gap_limit;
        return out;
    }

    std::error_code ec;

    if (std::filesystem::exists(
            path_,
            ec)) {
        out.error =
            WalletRecoveryError::target_exists;
        out.store_error =
            ec
                ? WalletStoreError::io_error
                : WalletStoreError::target_exists;
        return out;
    }

    if (ec) {
        out.error =
            WalletRecoveryError::store_failed;
        out.store_error =
            WalletStoreError::io_error;
        return out;
    }

    const auto height =
        chain.height();

    if (!height ||
        *height ==
            std::numeric_limits<
                std::uint32_t>::max()) {
        out.error =
            WalletRecoveryError::chain_not_ready;
        return out;
    }

    auto decoded =
        decode_recovery_mnemonic(
            mnemonic
        );

    if (!decoded.ok()) {
        out.error =
            WalletRecoveryError::invalid_mnemonic;
        out.mnemonic_error =
            decoded.error;
        return out;
    }

    SecretArrayGuard seed_guard{
        &decoded.seed
    };

    std::vector<KeyRecord> recovered;
    recovered.reserve(
        static_cast<std::size_t>(
            gap_limit) * 2U + 1U
    );

    std::map<crypto::PublicKey, bool>
        all_public_keys;

    const auto scan_branch =
        [&](bool internal,
            std::size_t minimum_keys,
            std::size_t& key_count) -> bool {
            std::vector<KeyRecord> branch;
            std::optional<std::uint32_t>
                highest_seen;

            const std::size_t initial_count =
                std::max(
                    minimum_keys,
                    static_cast<std::size_t>(
                        gap_limit)
                );

            if (initial_count >
                kMaxWalletKeys -
                    recovered.size()) {
                out.error =
                    WalletRecoveryError::key_limit;
                return false;
            }

            std::size_t target_count =
                initial_count;

            while (branch.size() <
                   target_count) {
                const std::size_t start =
                    branch.size();

                if (target_count >
                    static_cast<std::size_t>(
                        std::numeric_limits<
                            std::uint32_t>::max()) ||
                    target_count >
                        kMaxWalletKeys -
                            recovered.size()) {
                    out.error =
                        WalletRecoveryError::key_limit;
                    return false;
                }

                std::map<
                    crypto::PublicKey,
                    std::size_t> new_keys;

                branch.reserve(target_count);

                for (std::size_t position = start;
                     position < target_count;
                     ++position) {
                    const auto index =
                        static_cast<
                            std::uint32_t>(
                                position
                            );

                    auto private_key =
                        derive_hd_private_key(
                            decoded.seed,
                            params_.network,
                            internal,
                            index
                        );

                    if (!private_key) {
                        out.error =
                            WalletRecoveryError::
                                derivation_failed;
                        return false;
                    }

                    const auto public_key =
                        crypto::derive_public_key(
                            *private_key
                        );

                    if (!public_key) {
                        crypto::secure_erase(
                            *private_key
                        );
                        out.error =
                            WalletRecoveryError::
                                derivation_failed;
                        return false;
                    }

                    if (!all_public_keys
                             .emplace(
                                 *public_key,
                                 true)
                             .second) {
                        crypto::secure_erase(
                            *private_key
                        );
                        out.error =
                            WalletRecoveryError::
                                derivation_failed;
                        return false;
                    }

                    branch.emplace_back(
                        *private_key,
                        *public_key,
                        internal,
                        false,
                        true,
                        index
                    );

                    crypto::secure_erase(
                        *private_key
                    );

                    new_keys.emplace(
                        *public_key,
                        branch.size() - 1U
                    );
                }

                for (std::uint64_t current = 0U;
                     current <=
                         static_cast<
                             std::uint64_t>(
                                 *height);
                     ++current) {
                    const auto active_hash =
                        chain.active_hash(
                            static_cast<
                                std::uint32_t>(
                                    current
                                )
                        );

                    if (!active_hash) {
                        out.error =
                            WalletRecoveryError::
                                chain_not_ready;
                        return false;
                    }

                    const Block* block =
                        chain.block(
                            *active_hash
                        );

                    if (block == nullptr) {
                        out.error =
                            WalletRecoveryError::
                                chain_not_ready;
                        return false;
                    }

                    for (const auto& tx :
                         block->transactions) {
                        for (const auto& output :
                             tx.outputs) {
                            const auto public_key =
                                consensus::
                                    parse_p2pk_locking_script(
                                        output.locking_script
                                    );

                            if (!public_key) {
                                continue;
                            }

                            const auto found =
                                new_keys.find(
                                    *public_key
                                );

                            if (found ==
                                new_keys.end()) {
                                continue;
                            }

                            auto& record =
                                branch[found->second];

                            record.used = true;

                            if (!highest_seen ||
                                record.hd_index >
                                    *highest_seen) {
                                highest_seen =
                                    record.hd_index;
                            }
                        }
                    }
                }

                if (!highest_seen) {
                    break;
                }

                const std::uint64_t required =
                    static_cast<std::uint64_t>(
                        *highest_seen) +
                    1U +
                    static_cast<std::uint64_t>(
                        gap_limit);

                if (required >
                    static_cast<std::uint64_t>(
                        kMaxWalletKeys -
                        recovered.size())) {
                    out.error =
                        WalletRecoveryError::key_limit;
                    return false;
                }

                if (required <= branch.size()) {
                    break;
                }

                target_count =
                    static_cast<std::size_t>(
                        required
                    );
            }

            if (!internal &&
                !branch.empty()) {
                branch.front().used = true;
            }

            key_count = branch.size();

            for (auto& record :
                 branch) {
                recovered.push_back(
                    std::move(record)
                );
            }

            return true;
        };

    const std::size_t receive_minimum =
        kWalletKeypoolSize + 1U;
    const std::size_t change_minimum =
        kWalletKeypoolSize;

    if (!scan_branch(
            false,
            receive_minimum,
            out.receive_keys) ||
        !scan_branch(
            true,
            change_minimum,
            out.change_keys)) {
        for (auto& record :
             recovered) {
            crypto::secure_erase(
                record.private_key
            );
        }
        return out;
    }

    // Mark keys referenced by the current mempool before the
    // pre-commit sync. This guarantees sync() does not need to persist
    // key metadata while wallet.dat is intentionally still absent.
    std::map<
        crypto::PublicKey,
        std::size_t> recovered_lookup;

    for (std::size_t i = 0U;
         i < recovered.size();
         ++i) {
        recovered_lookup.emplace(
            recovered[i].public_key,
            i
        );
    }

    for (const auto& entry :
         mempool.entries()) {
        for (const auto& output :
             entry.transaction.outputs) {
            const auto public_key =
                consensus::
                    parse_p2pk_locking_script(
                        output.locking_script
                    );

            if (!public_key) {
                continue;
            }

            const auto found =
                recovered_lookup.find(
                    *public_key
                );

            if (found !=
                recovered_lookup.end()) {
                recovered[found->second].used =
                    true;
            }
        }
    }

    WalletSalt salt{};
    WalletEncryptionKey key{};

    SecretArrayGuard salt_guard{&salt};
    SecretArrayGuard key_guard{&key};

    if (!crypto::secure_random_bytes(
            salt) ||
        !derive_wallet_encryption_key(
            passphrase,
            salt,
            kWalletArgon2MemoryBlocks,
            kWalletArgon2Passes,
            key)) {
        out.error =
            WalletRecoveryError::store_failed;
        out.store_error =
            WalletStoreError::crypto_error;
        return out;
    }

    encrypted_ = true;
    recovery_seed_ = decoded.seed;
    encryption_key_ = key;
    encryption_salt_ = salt;
    argon2_memory_blocks_ =
        kWalletArgon2MemoryBlocks;
    argon2_passes_ =
        kWalletArgon2Passes;

    keys_ = std::move(recovered);
    started_ = true;
    reset_index_state();

    const auto rollback_uncommitted =
        [&]() noexcept {
            started_ = false;
            reset_index_state();
            clear_keys();

            encrypted_ = false;
            crypto::secure_erase(
                encryption_key_
            );
            crypto::secure_erase(
                encryption_salt_
            );

            if (recovery_seed_) {
                crypto::secure_erase(
                    *recovery_seed_
                );
                recovery_seed_.reset();
            }

            std::error_code cleanup_ec;

            if (std::filesystem::
                    is_regular_file(
                        state_path_,
                        cleanup_ec) &&
                !cleanup_ec) {
                std::filesystem::remove(
                    state_path_,
                    cleanup_ec
                );
            }
        };

    // Build the complete derivable index first. wallet.dat is deliberately
    // not committed until this succeeds, so an interrupted/failed recovery
    // cannot look like a completed wallet restore.
    out.sync =
        sync(
            chain,
            mempool
        );

    if (!out.sync.ok()) {
        rollback_uncommitted();
        out.error =
            WalletRecoveryError::sync_failed;
        return out;
    }

    // sync() must not have persisted key metadata during pre-commit.
    // Treat any unexpected wallet file as a failed atomic recovery.
    ec.clear();

    if (std::filesystem::exists(
            path_,
            ec) ||
        ec) {
        if (!ec) {
            std::filesystem::remove(
                path_,
                ec
            );
        }

        rollback_uncommitted();
        out.error =
            WalletRecoveryError::store_failed;
        out.store_error =
            WalletStoreError::io_error;
        return out;
    }

    address_labels_.clear();
    transaction_labels_.clear();
    metadata_wallet_id_ = Hash256{};
    metadata_wallet_id_valid_ = false;

    out.store_error =
        save_keys(keys_);

    if (out.store_error !=
        WalletStoreError::none) {
        rollback_uncommitted();
        out.error =
            WalletRecoveryError::store_failed;
        return out;
    }

    // Metadata is not seed-recoverable. Replace any orphaned metadata
    // from a previous wallet only after the recovered wallet keys have
    // been committed, binding an empty metadata store to this wallet.
    const auto metadata_saved =
        save_metadata();

    if (metadata_saved !=
        WalletMetadataError::none) {
        std::error_code remove_ec;
        std::filesystem::remove(
            path_,
            remove_ec
        );

        rollback_uncommitted();
        out.error =
            WalletRecoveryError::store_failed;
        out.store_error =
            WalletStoreError::io_error;
        return out;
    }

    return out;
}

bool Wallet::encrypted() const noexcept
{
    return encrypted_;
}

bool Wallet::verify_passphrase(
    std::string_view passphrase) const
{
    if (!started_ ||
        !encrypted_ ||
        passphrase.empty()) {
        return false;
    }

    WalletEncryptionKey candidate{};
    SecretArrayGuard candidate_guard{
        &candidate
    };

    if (!derive_wallet_encryption_key(
            passphrase,
            encryption_salt_,
            argon2_memory_blocks_,
            argon2_passes_,
            candidate)) {
        return false;
    }

    Byte difference{0U};

    for (std::size_t i = 0U;
         i < candidate.size();
         ++i) {
        difference =
            static_cast<Byte>(
                difference |
                static_cast<Byte>(
                    candidate[i] ^
                    encryption_key_[i]
                )
            );
    }

    return difference == 0U;
}

std::optional<RecoverySeed>
Wallet::recovery_seed() const noexcept
{
    if (!started_ ||
        !encrypted_ ||
        !recovery_seed_) {
        return std::nullopt;
    }

    const bool seed_complete =
        std::all_of(
            keys_.begin(),
            keys_.end(),
            [](const KeyRecord& key) {
                return key.deterministic;
            }
        );

    if (!seed_complete) {
        return std::nullopt;
    }

    return recovery_seed_;
}

std::optional<std::string>
Wallet::recovery_mnemonic() const
{
    auto seed =
        recovery_seed();

    if (!seed) {
        return std::nullopt;
    }

    SecretArrayGuard seed_guard{
        &*seed
    };

    auto encoded =
        encode_recovery_mnemonic(
            *seed
        );

    if (!encoded.ok()) {
        return std::nullopt;
    }

    return encoded.words;
}

WalletKeyResult Wallet::import_private_key(
    const crypto::PrivateKey& private_key)
{
    if (!started_) {
        WalletKeyResult out;
        out.error =
            WalletKeyError::not_started;
        return out;
    }

    auto result = append_key(
        private_key,
        false,
        true
    );

    if (result.ok()) {
        reset_index_state();
    }

    return result;
}

WalletStoreError Wallet::backup(
    const std::filesystem::path& destination,
    bool overwrite) const
{
    if (!started_) {
        return WalletStoreError::io_error;
    }

    const auto destination_error =
        validate_backup_destination(
            destination,
            path_,
            state_path_,
            metadata_path_
        );

    if (destination_error !=
        WalletStoreError::none) {
        return destination_error;
    }

    auto bytes =
        read_file(path_);

    if (!bytes) {
        return WalletStoreError::io_error;
    }

    SecretBytesGuard guard{&*bytes};

    return write_atomic(
        destination,
        *bytes,
        overwrite
    );
}

WalletStoreError Wallet::backup_bundle(
    const std::filesystem::path& destination,
    bool overwrite) const
{
    if (!started_) {
        return WalletStoreError::io_error;
    }

    const auto destination_error =
        validate_backup_destination(
            destination,
            path_,
            state_path_,
            metadata_path_
        );

    if (destination_error !=
        WalletStoreError::none) {
        return destination_error;
    }

    auto wallet_bytes =
        read_file(path_);

    if (!wallet_bytes) {
        return WalletStoreError::io_error;
    }

    SecretBytesGuard wallet_guard{
        &*wallet_bytes
    };

    Bytes metadata_bytes;
    std::error_code ec;

    const bool metadata_exists =
        std::filesystem::exists(
            metadata_path_,
            ec
        );

    if (ec) {
        return WalletStoreError::io_error;
    }

    if (metadata_exists) {
        auto loaded =
            read_file(metadata_path_);

        if (!loaded) {
            return WalletStoreError::io_error;
        }

        metadata_bytes =
            std::move(*loaded);
    }

    SecretBytesGuard metadata_guard{
        &metadata_bytes
    };

    Bytes bundle;
    bundle.reserve(
        64U +
        wallet_bytes->size() +
        metadata_bytes.size()
    );

    bundle.insert(
        bundle.end(),
        kBackupBundleMagic.begin(),
        kBackupBundleMagic.end()
    );

    append_little_endian(
        bundle,
        kBackupBundleVersion
    );

    bundle.insert(
        bundle.end(),
        params_.message_start.begin(),
        params_.message_start.end()
    );

    append_compact_size(
        bundle,
        static_cast<std::uint64_t>(
            wallet_bytes->size()
        )
    );

    append_compact_size(
        bundle,
        static_cast<std::uint64_t>(
            metadata_bytes.size()
        )
    );

    bundle.insert(
        bundle.end(),
        wallet_bytes->begin(),
        wallet_bytes->end()
    );

    bundle.insert(
        bundle.end(),
        metadata_bytes.begin(),
        metadata_bytes.end()
    );

    const Hash256 checksum =
        crypto::double_sha256(bundle);

    bundle.insert(
        bundle.end(),
        checksum.begin(),
        checksum.end()
    );

    SecretBytesGuard bundle_guard{
        &bundle
    };

    if (bundle.size() >
        kMaxBackupBundleSize) {
        return WalletStoreError::io_error;
    }

    return write_atomic(
        destination,
        bundle,
        overwrite
    );
}

WalletStoreError Wallet::restore_bundle(
    const std::filesystem::path& source)
{
    if (started_) {
        return WalletStoreError::target_exists;
    }

    std::error_code ec;

    if (std::filesystem::exists(
            path_,
            ec)) {
        return ec
            ? WalletStoreError::io_error
            : WalletStoreError::target_exists;
    }

    if (ec) {
        return WalletStoreError::io_error;
    }

    auto bundle =
        read_file_limited(
            source,
            kMaxBackupBundleSize
        );

    if (!bundle ||
        bundle->size() <
            kBackupBundleMagic.size() +
                sizeof(std::uint32_t) +
                params_.message_start.size() +
                2U +
                kChecksumSize) {
        return WalletStoreError::corrupt;
    }

    SecretBytesGuard bundle_guard{
        &*bundle
    };

    const std::size_t payload_size =
        bundle->size() - kChecksumSize;

    const Hash256 expected =
        crypto::double_sha256(
            std::span<const Byte>{
                bundle->data(),
                payload_size
            }
        );

    Hash256 stored{};
    std::copy_n(
        bundle->begin() +
            static_cast<std::ptrdiff_t>(
                payload_size),
        static_cast<std::ptrdiff_t>(
            stored.size()),
        stored.begin()
    );

    if (expected != stored) {
        return WalletStoreError::corrupt;
    }

    const std::span<const Byte> payload{
        bundle->data(),
        payload_size
    };

    std::size_t offset{0U};
    std::array<Byte, 8> magic{};

    if (offset + magic.size() >
        payload.size()) {
        return WalletStoreError::corrupt;
    }

    std::copy_n(
        payload.begin(),
        static_cast<std::ptrdiff_t>(
            magic.size()),
        magic.begin()
    );
    offset += magic.size();

    if (magic != kBackupBundleMagic) {
        return WalletStoreError::corrupt;
    }

    const auto version =
        read_little_endian<std::uint32_t>(
            payload,
            offset
        );

    if (!version ||
        *version != kBackupBundleVersion) {
        return WalletStoreError::corrupt;
    }

    if (offset +
            params_.message_start.size() >
        payload.size()) {
        return WalletStoreError::corrupt;
    }

    std::array<Byte, 4> message_start{};
    std::copy_n(
        payload.begin() +
            static_cast<std::ptrdiff_t>(
                offset),
        static_cast<std::ptrdiff_t>(
            message_start.size()),
        message_start.begin()
    );
    offset += message_start.size();

    if (message_start !=
        params_.message_start) {
        return WalletStoreError::wrong_network;
    }

    const auto wallet_size =
        read_compact_size(
            payload,
            offset
        );
    const auto metadata_size =
        read_compact_size(
            payload,
            offset
        );

    if (!wallet_size ||
        !metadata_size ||
        *wallet_size > 8U * 1024U * 1024U ||
        *metadata_size > 8U * 1024U * 1024U) {
        return WalletStoreError::corrupt;
    }

    const std::uint64_t total_size =
        *wallet_size +
        *metadata_size;

    if (total_size >
            static_cast<std::uint64_t>(
                payload.size()) ||
        offset > payload.size() ||
        total_size !=
            static_cast<std::uint64_t>(
                payload.size() - offset)) {
        return WalletStoreError::corrupt;
    }

    const std::size_t wallet_count =
        static_cast<std::size_t>(
            *wallet_size
        );
    const std::size_t metadata_count =
        static_cast<std::size_t>(
            *metadata_size
        );

    const std::span<const Byte>
        wallet_data{
            payload.data() + offset,
            wallet_count
        };

    offset += wallet_count;

    const std::span<const Byte>
        metadata_data{
            payload.data() + offset,
            metadata_count
        };

    // wallet_state.dat is derived cache data. Remove a stale copy before
    // writing restored primary wallet material so a cleanup failure cannot
    // turn a reported restore success into a broken first startup.
    ec.clear();
    const bool state_exists =
        std::filesystem::exists(
            state_path_,
            ec
        );

    if (ec) {
        return WalletStoreError::io_error;
    }

    if (state_exists) {
        const bool removed =
            std::filesystem::remove(
                state_path_,
                ec
            );

        if (ec || !removed) {
            return WalletStoreError::io_error;
        }
    }

    auto result =
        write_atomic(
            path_,
            wallet_data,
            false
        );

    if (result !=
        WalletStoreError::none) {
        return result;
    }

    const auto rollback_wallet =
        [&]() noexcept {
            std::error_code remove_ec;
            std::filesystem::remove(
                path_,
                remove_ec
            );
        };

    if (!metadata_data.empty()) {
        result =
            write_atomic(
                metadata_path_,
                metadata_data,
                true
            );

        if (result !=
            WalletStoreError::none) {
            rollback_wallet();
            return result;
        }
    } else {
        ec.clear();

        if (std::filesystem::exists(
                metadata_path_,
                ec)) {
            if (ec ||
                !std::filesystem::remove(
                    metadata_path_,
                    ec) ||
                ec) {
                rollback_wallet();
                return WalletStoreError::io_error;
            }
        } else if (ec) {
            rollback_wallet();
            return WalletStoreError::io_error;
        }
    }

    return WalletStoreError::none;
}

WalletSyncResult Wallet::sync(
    const Chainstate& chain,
    const Mempool& mempool)
{
    WalletSyncResult out;
    const auto previous_history =
        history_;

    if (!started_) {
        out.error =
            WalletSyncError::not_started;
        return out;
    }

    const auto height =
        chain.height();

    if (!height) {
        out.error =
            WalletSyncError::chain_not_ready;
        return out;
    }

    if (*height ==
        std::numeric_limits<
            std::uint32_t>::max()) {
        out.error =
            WalletSyncError::height_overflow;
        return out;
    }

    const auto tip =
        chain.active_hash(*height);

    if (!tip) {
        out.error =
            WalletSyncError::
                active_chain_inconsistent;
        return out;
    }

    bool rebuild =
        !index_valid_ ||
        indexed_height_ > *height;

    if (!rebuild) {
        const auto indexed_active =
            chain.active_hash(
                indexed_height_
            );

        rebuild =
            !indexed_active ||
            *indexed_active !=
                indexed_tip_;
    }

    std::uint64_t scan_start{0U};

    if (rebuild) {
        reset_index_state();
        confirmed_coins_.clear();
        confirmed_history_.clear();
        index_valid_ = true;
        out.index_rebuilt = true;
        scan_start = 0U;
    } else {
        scan_start =
            static_cast<std::uint64_t>(
                indexed_height_) + 1U;
    }

    std::vector<crypto::PublicKey>
        discovered_keys;

    for (std::uint64_t current = scan_start;
         current <=
             static_cast<std::uint64_t>(
                 *height);
         ++current) {
        const auto active_hash =
            chain.active_hash(
                static_cast<std::uint32_t>(
                    current)
            );

        if (!active_hash) {
            reset_index_state();
            out.error =
                WalletSyncError::
                    active_chain_inconsistent;
            return out;
        }

        const Block* block =
            chain.block(*active_hash);

        if (block == nullptr ||
            !apply_confirmed_block_to_index(
                *block,
                *active_hash,
                static_cast<std::uint32_t>(
                    current),
                discovered_keys
            )) {
            reset_index_state();
            out.error =
                WalletSyncError::
                    active_chain_inconsistent;
            return out;
        }

        indexed_height_ =
            static_cast<std::uint32_t>(
                current);
        indexed_tip_ = *active_hash;
        ++out.blocks_scanned;
    }

    if (rebuild &&
        out.blocks_scanned == 0U) {
        reset_index_state();
        out.error =
            WalletSyncError::
                active_chain_inconsistent;
        return out;
    }

    const std::uint32_t spend_height =
        *height + 1U;

    std::map<
        OutPoint,
        WalletCoin,
        OutPointLess> available;

    WalletBalance next_balance;

    for (const auto& [outpoint, coin] :
         confirmed_coins_) {
        (void)outpoint;

        if (mature_at_height(
                coin.coin,
                spend_height)) {
            if (!add_amount(
                    next_balance.confirmed,
                    coin.coin.output.value) ||
                !add_amount(
                    next_balance.available,
                    coin.coin.output.value)) {
                out.error =
                    WalletSyncError::
                        amount_overflow;
                return out;
            }

            available.emplace(
                coin.outpoint,
                coin
            );
        } else {
            if (!add_amount(
                    next_balance.immature,
                    coin.coin.output.value)) {
                out.error =
                    WalletSyncError::
                        amount_overflow;
                return out;
            }
        }
    }

    std::map<
        OutPoint,
        WalletCoin,
        OutPointLess> pending;

    history_ = confirmed_history_;

    for (auto record : previous_history) {
        const bool now_confirmed =
            std::any_of(
                confirmed_history_.begin(),
                confirmed_history_.end(),
                [&](const auto& confirmed) {
                    return confirmed.txid ==
                           record.txid;
                }
            );

        if (now_confirmed) {
            continue;
        }

        record.status =
            WalletTransactionStatus::inactive;
        record.block_height.reset();
        record.block_hash.reset();
        record.confirmations = 0U;

        const bool duplicate =
            std::any_of(
                history_.begin(),
                history_.end(),
                [&](const auto& existing) {
                    return existing.txid ==
                           record.txid;
                }
            );

        if (!duplicate) {
            history_.push_back(
                std::move(record)
            );
        }
    }

    for (auto& record : history_) {
        if (record.status !=
            WalletTransactionStatus::confirmed) {
            record.confirmations = 0U;
            continue;
        }

        if (!record.block_height ||
            *record.block_height > *height) {
            reset_index_state();
            out.error =
                WalletSyncError::
                    active_chain_inconsistent;
            return out;
        }

        record.confirmations =
            *height -
            *record.block_height +
            1U;
    }

    for (const auto& entry :
         mempool.entries()) {
        const auto& tx =
            entry.transaction;

        Amount spent{0U};

        for (const auto& input :
             tx.inputs) {
            const auto available_it =
                available.find(
                    input.previous_output
                );

            if (available_it !=
                available.end()) {
                const Amount value =
                    available_it->
                        second.coin.output.value;

                if (!add_amount(
                        spent,
                        value) ||
                    value >
                        next_balance.available) {
                    out.error =
                        WalletSyncError::
                            amount_overflow;
                    return out;
                }

                next_balance.available -= value;
                available.erase(
                    available_it
                );
                continue;
            }

            const auto pending_it =
                pending.find(
                    input.previous_output
                );

            if (pending_it !=
                pending.end()) {
                const Amount value =
                    pending_it->
                        second.coin.output.value;

                if (!add_amount(
                        spent,
                        value) ||
                    value >
                        next_balance.pending) {
                    out.error =
                        WalletSyncError::
                            amount_overflow;
                    return out;
                }

                next_balance.pending -= value;
                pending.erase(
                    pending_it
                );
            }
        }

        const Hash256 txid =
            entry.txid;

        Amount received{0U};

        for (std::size_t index = 0U;
             index < tx.outputs.size();
             ++index) {
            if (index >
                static_cast<std::size_t>(
                    std::numeric_limits<
                        std::uint32_t>::max())) {
                out.error =
                    WalletSyncError::
                        amount_overflow;
                return out;
            }

            const auto public_key =
                consensus::
                    parse_p2pk_locking_script(
                        tx.outputs[index]
                            .locking_script
                    );

            if (!public_key ||
                !owns_public_key(
                    *public_key)) {
                continue;
            }

            if (!add_amount(
                    received,
                    tx.outputs[index].value)) {
                out.error =
                    WalletSyncError::
                        amount_overflow;
                return out;
            }

            if (std::find(
                    discovered_keys.begin(),
                    discovered_keys.end(),
                    *public_key) ==
                discovered_keys.end()) {
                discovered_keys.push_back(
                    *public_key
                );
            }

            const OutPoint outpoint{
                .txid = txid,
                .index =
                    static_cast<
                        std::uint32_t>(
                            index),
            };

            WalletCoin owned{
                .outpoint = outpoint,
                .coin = Coin{
                    .output =
                        tx.outputs[index],
                    .height =
                        spend_height,
                    .coinbase = false,
                },
                .public_key = *public_key,
            };

            if (!add_amount(
                    next_balance.pending,
                    owned.coin.output.value)) {
                out.error =
                    WalletSyncError::
                        amount_overflow;
                return out;
            }

            const auto [it, inserted] =
                pending.emplace(
                    outpoint,
                    std::move(owned)
                );

            (void)it;

            if (!inserted) {
                out.error =
                    WalletSyncError::
                        active_chain_inconsistent;
                return out;
            }
        }

        if (spent > 0U ||
            received > 0U) {
            WalletTransactionRecord record;
            record.txid = txid;
            record.status =
                WalletTransactionStatus::
                    unconfirmed;
            record.received = received;
            record.spent = spent;
            record.coinbase = false;
            record.confirmations = 0U;

            if (spent > 0U) {
                record.fee = entry.fee;
            }

            const auto existing =
                std::find_if(
                    history_.begin(),
                    history_.end(),
                    [&](const auto& item) {
                        return item.txid ==
                               txid;
                    }
                );

            if (existing == history_.end()) {
                history_.push_back(
                    std::move(record)
                );
            } else {
                *existing =
                    std::move(record);
            }
        }
    }

    bool metadata_changed{false};
    std::vector<KeyRecord> updated_keys =
        keys_;

    for (auto& key : updated_keys) {
        if (key.used) {
            continue;
        }

        if (std::find(
                discovered_keys.begin(),
                discovered_keys.end(),
                key.public_key) !=
            discovered_keys.end()) {
            key.used = true;
            metadata_changed = true;
        }
    }

    if (metadata_changed) {
        const auto store_error =
            save_keys(updated_keys);

        if (store_error !=
            WalletStoreError::none) {
            for (auto& key : updated_keys) {
                crypto::secure_erase(
                    key.private_key
                );
            }

            out.error =
                WalletSyncError::store_failed;
            return out;
        }

        for (std::size_t i = 0U;
             i < keys_.size();
             ++i) {
            keys_[i].used =
                updated_keys[i].used;
        }
    }

    for (auto& key : updated_keys) {
        crypto::secure_erase(
            key.private_key
        );
    }

    const bool history_changed =
        history_ != previous_history;

    if (out.blocks_scanned > 0U ||
        out.index_rebuilt ||
        history_changed) {
        const auto state_error =
            save_index_state();

        if (state_error !=
            WalletStoreError::none) {
            out.error =
                WalletSyncError::store_failed;
            return out;
        }
    }

    available_coins_ =
        std::move(available);
    pending_coins_ =
        std::move(pending);
    balance_ = next_balance;

    out.balance = balance_;
    out.confirmed_outputs =
        confirmed_coins_.size();
    out.available_outputs =
        available_coins_.size();
    out.pending_outputs =
        pending_coins_.size();

    return out;
}

WalletFeeQuote Wallet::quote_auto_fee(
    std::string_view destination,
    Amount amount,
    const Chainstate& chain,
    const Mempool& mempool)
{
    WalletFeeQuote out;
    out.amount = amount;

    if (!started_) {
        out.error =
            WalletCreateError::not_started;
        return out;
    }

    const auto synced =
        sync(chain, mempool);

    if (!synced.ok()) {
        out.error =
            WalletCreateError::sync_failed;
        out.sync_error = synced.error;
        return out;
    }

    const auto decoded =
        decode_address(
            params_.network,
            destination
        );

    if (!decoded.ok()) {
        out.address_error =
            decoded.error;
        out.error =
            decoded.error ==
                    AddressError::wrong_network
                ? WalletCreateError::
                      wrong_network_address
                : WalletCreateError::
                      invalid_address;
        return out;
    }

    if (amount == 0U) {
        out.error =
            WalletCreateError::zero_amount;
        return out;
    }

    if (!consensus::money_range(amount)) {
        out.error =
            WalletCreateError::
                amount_out_of_range;
        return out;
    }

    out.fee_rate_per_kb =
        recommended_fee_rate(mempool);

    if (!consensus::money_range(
            out.fee_rate_per_kb)) {
        out.error =
            WalletCreateError::
                fee_out_of_range;
        return out;
    }

    std::vector<WalletCoin> candidates;
    candidates.reserve(
        available_coins_.size()
    );

    for (const auto& [outpoint, coin] :
         available_coins_) {
        (void)outpoint;
        candidates.push_back(coin);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const WalletCoin& lhs,
           const WalletCoin& rhs) {
            if (lhs.coin.height !=
                rhs.coin.height) {
                return lhs.coin.height <
                       rhs.coin.height;
            }

            return OutPointLess{}(
                lhs.outpoint,
                rhs.outpoint
            );
        }
    );

    Amount selected_value{0U};
    std::size_t selected_count{0U};

    for (const auto& coin : candidates) {
        if (coin.coin.output.value >
            consensus::kMaxMoney -
                selected_value) {
            out.error =
                WalletCreateError::
                    value_overflow;
            return out;
        }

        selected_value +=
            coin.coin.output.value;
        ++selected_count;

        const auto no_change_size =
            estimate_p2pk_transaction_size(
                selected_count,
                1U
            );

        if (!no_change_size) {
            out.error =
                WalletCreateError::
                    validation_failed;
            return out;
        }

        const auto no_change_fee =
            policy::fee_for_size(
                *no_change_size,
                out.fee_rate_per_kb
            );

        if (!no_change_fee) {
            out.error =
                WalletCreateError::
                    fee_out_of_range;
            return out;
        }

        if (*no_change_fee >
            consensus::kMaxMoney - amount) {
            out.error =
                WalletCreateError::
                    value_overflow;
            return out;
        }

        const Amount no_change_target =
            amount + *no_change_fee;

        if (selected_value <
            no_change_target) {
            continue;
        }

        const auto with_change_size =
            estimate_p2pk_transaction_size(
                selected_count,
                2U
            );

        if (!with_change_size) {
            out.error =
                WalletCreateError::
                    validation_failed;
            return out;
        }

        const auto with_change_fee =
            policy::fee_for_size(
                *with_change_size,
                out.fee_rate_per_kb
            );

        if (!with_change_fee) {
            out.error =
                WalletCreateError::
                    fee_out_of_range;
            return out;
        }

        const bool can_make_change =
            *with_change_fee <=
                consensus::kMaxMoney - amount &&
            selected_value >
                amount + *with_change_fee;

        out.selected_value =
            selected_value;
        out.input_count =
            selected_count;

        if (can_make_change) {
            out.fee =
                *with_change_fee;
            out.change =
                selected_value -
                amount -
                out.fee;
            out.serialized_size =
                *with_change_size;
            out.output_count = 2U;
        } else {
            out.fee =
                selected_value -
                amount;
            out.change = 0U;
            out.serialized_size =
                *no_change_size;
            out.output_count = 1U;
        }

        if (!consensus::money_range(
                out.fee)) {
            out.error =
                WalletCreateError::
                    fee_out_of_range;
            return out;
        }

        return out;
    }

    out.error =
        WalletCreateError::
            insufficient_funds;
    return out;
}

WalletCreateResult
Wallet::create_transaction_auto_fee(
    std::string_view destination,
    Amount amount,
    const Chainstate& chain,
    const Mempool& mempool)
{
    const auto quote =
        quote_auto_fee(
            destination,
            amount,
            chain,
            mempool
        );

    if (!quote.ok()) {
        WalletCreateResult out;
        out.error = quote.error;
        out.address_error =
            quote.address_error;
        out.sync_error =
            quote.sync_error;
        out.amount = amount;
        return out;
    }

    auto out =
        create_transaction(
            destination,
            amount,
            quote.fee,
            chain,
            mempool
        );

    if (!out.ok()) {
        return out;
    }

    out.fee_rate_per_kb =
        quote.fee_rate_per_kb;
    out.serialized_size =
        quote.serialized_size;

    const auto actual_size =
        serialized_transaction_size(
            out.transaction
        );

    if (!actual_size ||
        *actual_size !=
            quote.serialized_size) {
        out.error =
            WalletCreateError::
                validation_failed;
        return out;
    }

    const auto required =
        policy::fee_for_size(
            *actual_size,
            quote.fee_rate_per_kb
        );

    if (!required ||
        out.fee < *required) {
        out.error =
            WalletCreateError::
                validation_failed;
        return out;
    }

    return out;
}

WalletCreateResult Wallet::create_transaction(
    std::string_view destination,
    Amount amount,
    Amount fee,
    const Chainstate& chain,
    const Mempool& mempool)
{
    WalletCreateResult out;
    out.amount = amount;
    out.fee = fee;

    if (!started_) {
        out.error =
            WalletCreateError::not_started;
        return out;
    }

    const auto synced =
        sync(chain, mempool);

    if (!synced.ok()) {
        out.error =
            WalletCreateError::sync_failed;
        out.sync_error = synced.error;
        return out;
    }

    const auto decoded =
        decode_address(
            params_.network,
            destination
        );

    if (!decoded.ok()) {
        out.address_error =
            decoded.error;
        out.error =
            decoded.error ==
                    AddressError::wrong_network
                ? WalletCreateError::
                      wrong_network_address
                : WalletCreateError::
                      invalid_address;
        return out;
    }

    if (amount == 0U) {
        out.error =
            WalletCreateError::zero_amount;
        return out;
    }

    if (!consensus::money_range(amount)) {
        out.error =
            WalletCreateError::
                amount_out_of_range;
        return out;
    }

    if (!consensus::money_range(fee)) {
        out.error =
            WalletCreateError::
                fee_out_of_range;
        return out;
    }

    if (fee >
        consensus::kMaxMoney - amount) {
        out.error =
            WalletCreateError::value_overflow;
        return out;
    }

    const Amount target =
        amount + fee;

    std::vector<WalletCoin> candidates;
    candidates.reserve(
        available_coins_.size()
    );

    for (const auto& [outpoint, coin] :
         available_coins_) {
        (void)outpoint;
        candidates.push_back(coin);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const WalletCoin& lhs,
           const WalletCoin& rhs) {
            if (lhs.coin.height !=
                rhs.coin.height) {
                return lhs.coin.height <
                       rhs.coin.height;
            }

            return OutPointLess{}(
                lhs.outpoint,
                rhs.outpoint
            );
        }
    );

    std::vector<WalletCoin> selected;
    Amount selected_value{0U};

    for (const auto& coin : candidates) {
        if (selected_value >= target) {
            break;
        }

        if (coin.coin.output.value >
            consensus::kMaxMoney -
                selected_value) {
            out.error =
                WalletCreateError::
                    value_overflow;
            return out;
        }

        selected.push_back(coin);
        selected_value +=
            coin.coin.output.value;
    }

    if (selected_value < target) {
        out.error =
            WalletCreateError::
                insufficient_funds;
        return out;
    }

    out.selected_value =
        selected_value;
    out.change =
        selected_value - target;

    Transaction tx;
    tx.inputs.reserve(
        selected.size()
    );

    for (const auto& coin : selected) {
        tx.inputs.push_back(
            TxInput{
                .previous_output =
                    coin.outpoint,
            }
        );
    }

    tx.outputs.push_back(
        TxOutput{
            .value = amount,
            .locking_script =
                consensus::
                    make_p2pk_locking_script(
                        decoded.public_key),
        }
    );

    if (out.change > 0U) {
        const auto change_key =
            reserve_key(true);

        if (!change_key.ok()) {
            out.store_error =
                change_key.store_error;
            out.error =
                change_key.error ==
                        WalletKeyError::
                            store_failed
                    ? WalletCreateError::
                          store_failed
                    : WalletCreateError::
                          key_generation_failed;
            return out;
        }

        out.backup_recommended =
            change_key.backup_recommended;

        tx.outputs.push_back(
            TxOutput{
                .value = out.change,
                .locking_script =
                    consensus::
                        make_p2pk_locking_script(
                            change_key.public_key),
            }
        );
    }

    for (std::size_t i = 0U;
         i < selected.size();
         ++i) {
        const auto* private_key =
            private_key_for(
                selected[i].public_key
            );

        if (private_key == nullptr) {
            out.error =
                WalletCreateError::
                    missing_private_key;
            return out;
        }

        out.auth_error =
            consensus::sign_p2pk_input(
                tx,
                i,
                selected[i].coin.output,
                *private_key
            );

        if (out.auth_error !=
            consensus::InputAuthError::none) {
            out.error =
                WalletCreateError::
                    signing_failed;
            return out;
        }
    }

    if (validate_transaction_structure(tx) !=
        TxStructureError::none) {
        out.error =
            WalletCreateError::
                validation_failed;
        return out;
    }

    const auto transaction_size =
        serialized_transaction_size(tx);

    if (transaction_size) {
        out.serialized_size =
            *transaction_size;

        const auto effective_rate =
            policy::fee_rate_for_size(
                fee,
                *transaction_size
            );

        out.fee_rate_per_kb =
            effective_rate.value_or(0U);
    }

    if (!transaction_size ||
        *transaction_size >
            static_cast<std::size_t>(
                chain.params().limits
                    .max_block_serialized_bytes)) {
        out.error =
            WalletCreateError::
                validation_failed;
        return out;
    }

    const auto height =
        chain.height();

    if (!height ||
        *height ==
            std::numeric_limits<
                std::uint32_t>::max()) {
        out.error =
            WalletCreateError::
                validation_failed;
        return out;
    }

    const std::uint32_t next_height =
        *height + 1U;

    UtxoSet view =
        chain.utxos();

    for (const auto& existing :
         mempool.transactions()) {
        const auto applied =
            view.apply_transaction(
                existing,
                next_height
            );

        if (!applied.ok()) {
            out.validation_error =
                applied.error;
            out.error =
                WalletCreateError::
                    validation_failed;
            return out;
        }
    }

    const auto applied =
        view.apply_transaction(
            tx,
            next_height
        );

    if (!applied.ok() ||
        applied.fee != fee) {
        out.validation_error =
            applied.error;
        out.error =
            WalletCreateError::
                validation_failed;
        return out;
    }

    out.transaction =
        std::move(tx);
    out.txid =
        transaction_id(
            out.transaction
        );

    return out;
}

WalletBalance Wallet::balance() const noexcept
{
    return balance_;
}

std::vector<WalletTransactionRecord>
Wallet::history() const
{
    if (!started_) {
        return {};
    }

    return history_;
}

std::vector<std::string>
Wallet::addresses() const
{
    std::vector<std::string> output;

    if (!started_) {
        return output;
    }

    for (const auto& key : keys_) {
        if (key.internal ||
            !key.used) {
            continue;
        }

        output.push_back(
            encode_address(
                params_.network,
                key.public_key
            )
        );
    }

    return output;
}

bool Wallet::owns_public_key(
    const crypto::PublicKey& public_key) const noexcept
{
    return std::any_of(
        keys_.begin(),
        keys_.end(),
        [&](const KeyRecord& key) {
            return key.public_key ==
                   public_key;
        }
    );
}

const std::filesystem::path&
Wallet::path() const noexcept
{
    return path_;
}

WalletStoreError Wallet::load(
    std::string_view passphrase)
{
    clear_keys();
    encrypted_ = false;
    crypto::secure_erase(encryption_key_);
    crypto::secure_erase(encryption_salt_);

    if (recovery_seed_) {
        crypto::secure_erase(*recovery_seed_);
        recovery_seed_.reset();
    }

    std::error_code ec;

    if (!std::filesystem::exists(
            path_,
            ec)) {
        return ec
            ? WalletStoreError::io_error
            : WalletStoreError::not_found;
    }

    auto bytes =
        read_file(path_);

    if (!bytes) {
        return WalletStoreError::corrupt;
    }

    SecretBytesGuard guard{&*bytes};

    if (bytes->size() <
        kWalletMagic.size() +
            sizeof(std::uint32_t)) {
        return WalletStoreError::corrupt;
    }

    const std::span<const Byte> all{
        bytes->data(),
        bytes->size()
    };

    if (!std::equal(
            kWalletMagic.begin(),
            kWalletMagic.end(),
            all.begin())) {
        return WalletStoreError::corrupt;
    }

    std::size_t version_offset =
        kWalletMagic.size();

    const auto version =
        read_little_endian<
            std::uint32_t>(
                all,
                version_offset
            );

    if (!version) {
        return WalletStoreError::corrupt;
    }

    if (*version ==
        kLegacyWalletVersion) {
        if (bytes->size() <
            kWalletMagic.size() +
                sizeof(std::uint32_t) +
                1U +
                params_.message_start.size() +
                1U +
                kKeyRecordSize +
                kChecksumSize) {
            return WalletStoreError::corrupt;
        }

        const std::span<const Byte> body{
            bytes->data(),
            bytes->size() -
                kChecksumSize
        };

        const auto checksum =
            crypto::double_sha256(body);

        if (!std::equal(
                checksum.begin(),
                checksum.end(),
                bytes->end() -
                    static_cast<
                        std::ptrdiff_t>(
                            kChecksumSize))) {
            return WalletStoreError::corrupt;
        }

        std::size_t offset{
            kWalletMagic.size()
        };

        const auto parsed_version =
            read_little_endian<
                std::uint32_t>(
                    body,
                    offset
                );

        if (!parsed_version ||
            *parsed_version !=
                kLegacyWalletVersion ||
            offset >= body.size()) {
            return WalletStoreError::corrupt;
        }

        const Byte network =
            body[offset++];

        if (network !=
            static_cast<Byte>(
                params_.network)) {
            return WalletStoreError::
                wrong_network;
        }

        if (offset +
                params_.message_start.size() >
            body.size() ||
            !std::equal(
                params_.message_start.begin(),
                params_.message_start.end(),
                body.begin() +
                    static_cast<
                        std::ptrdiff_t>(
                            offset))) {
            return WalletStoreError::
                wrong_network;
        }

        offset +=
            params_.message_start.size();

        const auto count =
            read_compact_size(
                body,
                offset
            );

        if (!count ||
            *count == 0U ||
            *count > kMaxWalletKeys ||
            *count >
                static_cast<std::uint64_t>(
                    (body.size() - offset) /
                    kKeyRecordSize)) {
            return WalletStoreError::corrupt;
        }

        std::vector<KeyRecord> loaded;
        loaded.reserve(
            static_cast<std::size_t>(
                *count)
        );

        for (std::uint64_t index = 0U;
             index < *count;
             ++index) {
            if (offset >= body.size()) {
                return WalletStoreError::corrupt;
            }

            const Byte flags =
                body[offset++];

            if ((flags &
                 static_cast<Byte>(
                     ~kKnownKeyFlags)) != 0U ||
                offset +
                    crypto::PrivateKey{}.size() >
                    body.size()) {
                return WalletStoreError::corrupt;
            }

            crypto::PrivateKey private_key{};
            SecretArrayGuard private_guard{
                &private_key
            };

            std::copy_n(
                body.begin() +
                    static_cast<
                        std::ptrdiff_t>(
                            offset),
                static_cast<
                    std::ptrdiff_t>(
                        private_key.size()),
                private_key.begin()
            );

            offset +=
                private_key.size();

            const auto public_key =
                crypto::derive_public_key(
                    private_key
                );

            if (!public_key) {
                return WalletStoreError::corrupt;
            }

            const bool duplicate =
                std::any_of(
                    loaded.begin(),
                    loaded.end(),
                    [&](const KeyRecord& key) {
                        return key.public_key ==
                               *public_key;
                    }
                );

            if (duplicate) {
                return WalletStoreError::corrupt;
            }

            loaded.emplace_back(
                private_key,
                *public_key,
                (flags & kInternalFlag) != 0U,
                (flags & kUsedFlag) != 0U
            );
        }

        if (offset != body.size()) {
            return WalletStoreError::corrupt;
        }

        keys_ = std::move(loaded);

#ifndef _WIN32
        if (!restrict_file_permissions(path_)) {
            clear_keys();
            return WalletStoreError::io_error;
        }
#endif

        return WalletStoreError::none;
    }

    if (*version !=
        kEncryptedWalletVersion) {
        return WalletStoreError::corrupt;
    }

    if (passphrase.empty()) {
        return WalletStoreError::
            passphrase_required;
    }

    constexpr std::size_t minimum_v2{
        8U +
        sizeof(std::uint32_t) +
        1U +
        4U +
        sizeof(std::uint32_t) * 2U +
        WalletSalt{}.size() +
        WalletNonce{}.size() +
        1U +
        RecoverySeed{}.size() +
        1U +
        kEncryptedKeyRecordSize +
        kEncryptedTagSize
    };

    if (all.size() < minimum_v2) {
        return WalletStoreError::corrupt;
    }

    std::size_t offset{
        kWalletMagic.size()
    };

    const auto parsed_version =
        read_little_endian<
            std::uint32_t>(
                all,
                offset
            );

    if (!parsed_version ||
        *parsed_version !=
            kEncryptedWalletVersion ||
        offset >= all.size()) {
        return WalletStoreError::corrupt;
    }

    const Byte network =
        all[offset++];

    if (network !=
        static_cast<Byte>(
            params_.network)) {
        return WalletStoreError::
            wrong_network;
    }

    if (offset +
            params_.message_start.size() >
        all.size() ||
        !std::equal(
            params_.message_start.begin(),
            params_.message_start.end(),
            all.begin() +
                static_cast<
                    std::ptrdiff_t>(
                        offset))) {
        return WalletStoreError::
            wrong_network;
    }

    offset +=
        params_.message_start.size();

    const auto memory_blocks =
        read_little_endian<
            std::uint32_t>(
                all,
                offset
            );
    const auto passes =
        read_little_endian<
            std::uint32_t>(
                all,
                offset
            );

    if (!memory_blocks ||
        !passes ||
        *memory_blocks < 8U ||
        *memory_blocks >
            kMaxArgon2MemoryBlocks ||
        *passes == 0U ||
        *passes >
            kMaxArgon2Passes) {
        return WalletStoreError::corrupt;
    }

    WalletSalt salt{};
    WalletNonce nonce{};

    if (offset + salt.size() +
            nonce.size() >
        all.size()) {
        return WalletStoreError::corrupt;
    }

    std::copy_n(
        all.begin() +
            static_cast<
                std::ptrdiff_t>(offset),
        static_cast<
            std::ptrdiff_t>(salt.size()),
        salt.begin()
    );
    offset += salt.size();

    std::copy_n(
        all.begin() +
            static_cast<
                std::ptrdiff_t>(offset),
        static_cast<
            std::ptrdiff_t>(nonce.size()),
        nonce.begin()
    );
    offset += nonce.size();

    const auto ciphertext_size =
        read_compact_size(
            all,
            offset
        );

    if (!ciphertext_size ||
        *ciphertext_size >
            static_cast<std::uint64_t>(
                all.size()) ||
        *ciphertext_size >
            static_cast<std::uint64_t>(
                std::numeric_limits<
                    std::size_t>::max())) {
        return WalletStoreError::corrupt;
    }

    const std::size_t payload_size =
        static_cast<std::size_t>(
            *ciphertext_size
        );

    if (payload_size >
            all.size() - offset ||
        all.size() - offset -
                payload_size !=
            kEncryptedTagSize) {
        return WalletStoreError::corrupt;
    }

    const std::size_t associated_size =
        offset;

    const std::span<const Byte> ciphertext{
        all.data() + offset,
        payload_size
    };
    offset += payload_size;

    WalletTag tag{};
    std::copy_n(
        all.begin() +
            static_cast<
                std::ptrdiff_t>(offset),
        static_cast<
            std::ptrdiff_t>(tag.size()),
        tag.begin()
    );

    WalletEncryptionKey key{};
    SecretArrayGuard key_guard{&key};

    if (!derive_wallet_encryption_key(
            passphrase,
            salt,
            *memory_blocks,
            *passes,
            key)) {
        return WalletStoreError::crypto_error;
    }

    Bytes plaintext;
    SecretBytesGuard plaintext_guard{
        &plaintext
    };

    if (!decrypt_wallet_payload(
            ciphertext,
            std::span<const Byte>{
                all.data(),
                associated_size
            },
            key,
            nonce,
            tag,
            plaintext)) {
        return WalletStoreError::
            invalid_passphrase;
    }

    std::size_t payload_offset{0U};

    if (plaintext.size() <
        RecoverySeed{}.size() + 1U) {
        return WalletStoreError::corrupt;
    }

    RecoverySeed seed{};
    SecretArrayGuard seed_guard{&seed};

    std::copy_n(
        plaintext.begin(),
        static_cast<std::ptrdiff_t>(
            seed.size()),
        seed.begin()
    );
    payload_offset += seed.size();

    const auto count =
        read_compact_size(
            std::span<const Byte>{
                plaintext.data(),
                plaintext.size()
            },
            payload_offset
        );

    if (!count ||
        *count == 0U ||
        *count > kMaxWalletKeys ||
        *count >
            static_cast<std::uint64_t>(
                (plaintext.size() -
                 payload_offset) /
                kEncryptedKeyRecordSize)) {
        return WalletStoreError::corrupt;
    }

    std::vector<KeyRecord> loaded;
    loaded.reserve(
        static_cast<std::size_t>(
            *count)
    );

    for (std::uint64_t record_index = 0U;
         record_index < *count;
         ++record_index) {
        if (payload_offset + 2U +
                sizeof(std::uint32_t) +
                crypto::PrivateKey{}.size() >
            plaintext.size()) {
            return WalletStoreError::corrupt;
        }

        const Byte flags =
            plaintext[payload_offset++];
        const Byte origin =
            plaintext[payload_offset++];

        if ((flags &
             static_cast<Byte>(
                 ~kKnownKeyFlags)) != 0U ||
            origin > 1U) {
            return WalletStoreError::corrupt;
        }

        const auto hd_index =
            read_little_endian<
                std::uint32_t>(
                    std::span<const Byte>{
                        plaintext.data(),
                        plaintext.size()
                    },
                    payload_offset
                );

        if (!hd_index) {
            return WalletStoreError::corrupt;
        }

        crypto::PrivateKey private_key{};
        SecretArrayGuard private_guard{
            &private_key
        };

        std::copy_n(
            plaintext.begin() +
                static_cast<
                    std::ptrdiff_t>(
                        payload_offset),
            static_cast<
                std::ptrdiff_t>(
                    private_key.size()),
            private_key.begin()
        );
        payload_offset +=
            private_key.size();

        const bool internal =
            (flags & kInternalFlag) != 0U;
        const bool deterministic =
            origin == 1U;

        if (!deterministic &&
            *hd_index != 0U) {
            return WalletStoreError::corrupt;
        }

        if (deterministic) {
            auto expected =
                derive_hd_private_key(
                    seed,
                    params_.network,
                    internal,
                    *hd_index
                );

            if (!expected ||
                *expected != private_key) {
                if (expected) {
                    crypto::secure_erase(
                        *expected
                    );
                }
                return WalletStoreError::corrupt;
            }

            crypto::secure_erase(*expected);
        }

        const auto public_key =
            crypto::derive_public_key(
                private_key
            );

        if (!public_key) {
            return WalletStoreError::corrupt;
        }

        const bool duplicate =
            std::any_of(
                loaded.begin(),
                loaded.end(),
                [&](const KeyRecord& item) {
                    return item.public_key ==
                           *public_key;
                }
            );

        if (duplicate) {
            return WalletStoreError::corrupt;
        }

        loaded.emplace_back(
            private_key,
            *public_key,
            internal,
            (flags & kUsedFlag) != 0U,
            deterministic,
            *hd_index
        );
    }

    if (payload_offset !=
        plaintext.size()) {
        return WalletStoreError::corrupt;
    }

    keys_ = std::move(loaded);
    encrypted_ = true;
    recovery_seed_ = seed;
    encryption_key_ = key;
    encryption_salt_ = salt;
    argon2_memory_blocks_ =
        *memory_blocks;
    argon2_passes_ = *passes;

#ifndef _WIN32
    if (!restrict_file_permissions(path_)) {
        clear_keys();
        crypto::secure_erase(
            encryption_key_);
        crypto::secure_erase(
            encryption_salt_);
        crypto::secure_erase(
            *recovery_seed_);
        recovery_seed_.reset();
        encrypted_ = false;
        return WalletStoreError::io_error;
    }
#endif

    return WalletStoreError::none;
}

WalletStoreError Wallet::save_keys(
    const std::vector<KeyRecord>& keys) const
{
    if (keys.empty() ||
        keys.size() >
            kMaxWalletKeys) {
        return WalletStoreError::corrupt;
    }

    if (!encrypted_) {
        Bytes body;
        body.reserve(
            kWalletMagic.size() +
            sizeof(std::uint32_t) +
            1U +
            params_.message_start.size() +
            9U +
            keys.size() *
                kKeyRecordSize +
            kChecksumSize
        );

        body.insert(
            body.end(),
            kWalletMagic.begin(),
            kWalletMagic.end()
        );

        append_little_endian(
            body,
            kLegacyWalletVersion
        );

        body.push_back(
            static_cast<Byte>(
                params_.network)
        );

        body.insert(
            body.end(),
            params_.message_start.begin(),
            params_.message_start.end()
        );

        append_compact_size(
            body,
            static_cast<std::uint64_t>(
                keys.size())
        );

        for (const auto& key : keys) {
            const auto derived =
                crypto::derive_public_key(
                    key.private_key
                );

            if (!derived ||
                *derived != key.public_key ||
                !crypto::
                    is_valid_public_key(
                        key.public_key)) {
                crypto::secure_erase(body);
                return WalletStoreError::corrupt;
            }

            Byte flags{0U};

            if (key.internal) {
                flags |= kInternalFlag;
            }

            if (key.used) {
                flags |= kUsedFlag;
            }

            body.push_back(flags);

            body.insert(
                body.end(),
                key.private_key.begin(),
                key.private_key.end()
            );
        }

        const auto checksum =
            crypto::double_sha256(body);

        body.insert(
            body.end(),
            checksum.begin(),
            checksum.end()
        );

        const auto result =
            write_atomic(
                path_,
                body,
                true
            );

        crypto::secure_erase(body);
        return result;
    }

    if (!recovery_seed_) {
        return WalletStoreError::crypto_error;
    }

    Bytes plaintext;
    plaintext.reserve(
        recovery_seed_->size() +
        9U +
        keys.size() *
            kEncryptedKeyRecordSize
    );

    plaintext.insert(
        plaintext.end(),
        recovery_seed_->begin(),
        recovery_seed_->end()
    );

    append_compact_size(
        plaintext,
        static_cast<std::uint64_t>(
            keys.size())
    );

    for (const auto& key : keys) {
        const auto derived =
            crypto::derive_public_key(
                key.private_key
            );

        if (!derived ||
            *derived != key.public_key ||
            !crypto::is_valid_public_key(
                key.public_key)) {
            crypto::secure_erase(plaintext);
            return WalletStoreError::corrupt;
        }

        if (key.deterministic) {
            auto expected =
                derive_hd_private_key(
                    *recovery_seed_,
                    params_.network,
                    key.internal,
                    key.hd_index
                );

            if (!expected ||
                *expected !=
                    key.private_key) {
                if (expected) {
                    crypto::secure_erase(
                        *expected
                    );
                }
                crypto::secure_erase(
                    plaintext
                );
                return WalletStoreError::corrupt;
            }

            crypto::secure_erase(*expected);
        } else if (key.hd_index != 0U) {
            crypto::secure_erase(plaintext);
            return WalletStoreError::corrupt;
        }

        Byte flags{0U};

        if (key.internal) {
            flags |= kInternalFlag;
        }

        if (key.used) {
            flags |= kUsedFlag;
        }

        plaintext.push_back(flags);
        plaintext.push_back(
            key.deterministic ? 1U : 0U
        );

        append_little_endian(
            plaintext,
            key.hd_index
        );

        plaintext.insert(
            plaintext.end(),
            key.private_key.begin(),
            key.private_key.end()
        );
    }

    WalletNonce nonce{};

    if (!crypto::secure_random_bytes(
            nonce)) {
        crypto::secure_erase(plaintext);
        return WalletStoreError::crypto_error;
    }

    Bytes output;
    output.reserve(
        kWalletMagic.size() +
        sizeof(std::uint32_t) +
        1U +
        params_.message_start.size() +
        sizeof(std::uint32_t) * 2U +
        encryption_salt_.size() +
        nonce.size() +
        9U +
        plaintext.size() +
        WalletTag{}.size()
    );

    output.insert(
        output.end(),
        kWalletMagic.begin(),
        kWalletMagic.end()
    );

    append_little_endian(
        output,
        kEncryptedWalletVersion
    );

    output.push_back(
        static_cast<Byte>(
            params_.network)
    );

    output.insert(
        output.end(),
        params_.message_start.begin(),
        params_.message_start.end()
    );

    append_little_endian(
        output,
        argon2_memory_blocks_
    );
    append_little_endian(
        output,
        argon2_passes_
    );

    output.insert(
        output.end(),
        encryption_salt_.begin(),
        encryption_salt_.end()
    );
    output.insert(
        output.end(),
        nonce.begin(),
        nonce.end()
    );

    append_compact_size(
        output,
        static_cast<std::uint64_t>(
            plaintext.size())
    );

    Bytes ciphertext;
    WalletTag tag{};

    const bool encrypted =
        encrypt_wallet_payload(
            plaintext,
            std::span<const Byte>{
                output.data(),
                output.size()
            },
            encryption_key_,
            nonce,
            ciphertext,
            tag
        );

    crypto::secure_erase(plaintext);

    if (!encrypted) {
        crypto::secure_erase(ciphertext);
        return WalletStoreError::crypto_error;
    }

    output.insert(
        output.end(),
        ciphertext.begin(),
        ciphertext.end()
    );
    output.insert(
        output.end(),
        tag.begin(),
        tag.end()
    );

    crypto::secure_erase(ciphertext);

    const auto result =
        write_atomic(
            path_,
            output,
            true
        );

    crypto::secure_erase(output);
    return result;
}

WalletKeyResult Wallet::append_key(
    const crypto::PrivateKey& private_key,
    bool internal,
    bool used)
{
    WalletKeyResult out;

    if (!crypto::is_valid_private_key(
            private_key)) {
        out.error =
            WalletKeyError::invalid_key;
        return out;
    }

    const auto public_key =
        crypto::derive_public_key(
            private_key
        );

    if (!public_key) {
        out.error =
            WalletKeyError::invalid_key;
        return out;
    }

    out.public_key = *public_key;
    out.address =
        encode_address(
            params_.network,
            *public_key
        );

    if (owns_public_key(
            *public_key)) {
        out.error =
            WalletKeyError::duplicate_key;
        return out;
    }

    if (keys_.size() >=
        kMaxWalletKeys) {
        out.error =
            WalletKeyError::key_limit;
        return out;
    }

    std::vector<KeyRecord> candidate =
        keys_;

    candidate.emplace_back(
        private_key,
        *public_key,
        internal,
        used
    );

    out.store_error =
        save_keys(candidate);

    for (auto& key : candidate) {
        crypto::secure_erase(
            key.private_key
        );
    }

    if (out.store_error !=
        WalletStoreError::none) {
        out.error =
            WalletKeyError::store_failed;
        return out;
    }

    keys_.emplace_back(
        private_key,
        *public_key,
        internal,
        used
    );

    out.backup_recommended = true;
    return out;
}

WalletKeyResult Wallet::reserve_key(
    bool internal)
{
    WalletKeyResult out;

    if (!started_) {
        out.error =
            WalletKeyError::not_started;
        return out;
    }

    const auto existing =
        std::find_if(
            keys_.begin(),
            keys_.end(),
            [&](const KeyRecord& key) {
                return key.internal ==
                           internal &&
                       !key.used;
            }
        );

    if (existing != keys_.end()) {
        const std::size_t index =
            static_cast<std::size_t>(
                std::distance(
                    keys_.begin(),
                    existing)
            );

        std::vector<KeyRecord> candidate =
            keys_;

        KeyRecord selected =
            std::move(candidate[index]);
        selected.used = true;

        candidate.erase(
            candidate.begin() +
            static_cast<std::ptrdiff_t>(
                index)
        );
        candidate.push_back(
            std::move(selected)
        );

        out.public_key =
            candidate.back().public_key;
        out.address =
            encode_address(
                params_.network,
                out.public_key
            );

        out.store_error =
            save_keys(candidate);

        if (out.store_error !=
            WalletStoreError::none) {
            for (auto& key : candidate) {
                crypto::secure_erase(
                    key.private_key
                );
            }

            out.error =
                WalletKeyError::store_failed;
            return out;
        }

        clear_keys();
        keys_ = std::move(candidate);
        return out;
    }

    if (keys_.size() >
        kMaxWalletKeys -
            kWalletKeypoolSize) {
        out.error =
            WalletKeyError::key_limit;
        return out;
    }

    std::vector<KeyRecord> candidate =
        keys_;
    const std::size_t first_new =
        candidate.size();

    if (!generate_pool_records(
            candidate,
            internal,
            kWalletKeypoolSize)) {
        for (auto& key : candidate) {
            crypto::secure_erase(
                key.private_key
            );
        }

        out.error =
            WalletKeyError::random_failed;
        return out;
    }

    candidate[first_new].used = true;

    out.public_key =
        candidate[first_new].public_key;
    out.address =
        encode_address(
            params_.network,
            out.public_key
        );
    out.backup_recommended = true;

    out.store_error =
        save_keys(candidate);

    if (out.store_error !=
        WalletStoreError::none) {
        for (auto& key : candidate) {
            crypto::secure_erase(
                key.private_key
            );
        }

        out.error =
            WalletKeyError::store_failed;
        return out;
    }

    clear_keys();
    keys_ = std::move(candidate);
    return out;
}

bool Wallet::generate_pool_records(
    std::vector<KeyRecord>& records,
    bool internal,
    std::size_t count) const
{
    const std::size_t original_size =
        records.size();

    if (original_size > kMaxWalletKeys ||
        count >
            kMaxWalletKeys -
                original_size) {
        return false;
    }

    records.reserve(
        original_size + count
    );

    if (encrypted_ && recovery_seed_) {
        std::uint32_t next_index{0U};

        for (const auto& record : records) {
            if (!record.deterministic ||
                record.internal != internal) {
                continue;
            }

            if (record.hd_index ==
                std::numeric_limits<
                    std::uint32_t>::max()) {
                return false;
            }

            next_index =
                std::max(
                    next_index,
                    record.hd_index + 1U
                );
        }

        while (records.size() <
               original_size + count) {
            auto private_key =
                derive_hd_private_key(
                    *recovery_seed_,
                    params_.network,
                    internal,
                    next_index
                );

            if (!private_key) {
                return false;
            }

            const auto public_key =
                crypto::derive_public_key(
                    *private_key
                );

            if (!public_key) {
                crypto::secure_erase(
                    *private_key
                );
                return false;
            }

            const bool duplicate =
                std::any_of(
                    records.begin(),
                    records.end(),
                    [&](const KeyRecord& key) {
                        return key.public_key ==
                               *public_key;
                    }
                );

            if (duplicate) {
                crypto::secure_erase(
                    *private_key
                );
                return false;
            }

            records.emplace_back(
                *private_key,
                *public_key,
                internal,
                false,
                true,
                next_index
            );

            crypto::secure_erase(
                *private_key
            );

            if (records.size() <
                    original_size + count &&
                next_index ==
                    std::numeric_limits<
                        std::uint32_t>::max()) {
                return false;
            }

            ++next_index;
        }

        return true;
    }

    while (records.size() <
           original_size + count) {
        auto private_key =
            crypto::generate_private_key();

        if (!private_key) {
            for (std::size_t i =
                     original_size;
                 i < records.size();
                 ++i) {
                crypto::secure_erase(
                    records[i].private_key
                );
            }

            records.resize(original_size);
            return false;
        }

        const auto public_key =
            crypto::derive_public_key(
                *private_key
            );

        if (!public_key) {
            crypto::secure_erase(
                *private_key
            );
            continue;
        }

        const bool duplicate =
            std::any_of(
                records.begin(),
                records.end(),
                [&](const KeyRecord& key) {
                    return key.public_key ==
                           *public_key;
                }
            );

        if (duplicate) {
            crypto::secure_erase(
                *private_key
            );
            continue;
        }

        records.emplace_back(
            *private_key,
            *public_key,
            internal,
            false
        );

        crypto::secure_erase(
            *private_key
        );
    }

    return true;
}

const crypto::PrivateKey*
Wallet::private_key_for(
    const crypto::PublicKey& public_key) const noexcept
{
    const auto it =
        std::find_if(
            keys_.begin(),
            keys_.end(),
            [&](const KeyRecord& key) {
                return key.public_key ==
                       public_key;
            }
        );

    return it == keys_.end()
        ? nullptr
        : &it->private_key;
}

void Wallet::clear_keys() noexcept
{
    for (auto& key : keys_) {
        crypto::secure_erase(
            key.private_key
        );
    }

    keys_.clear();
}

} // namespace quintum::wallet
