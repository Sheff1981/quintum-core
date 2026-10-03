#pragma once

#include "chain/chainstate.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/random.hpp"
#include "crypto/secp256k1.hpp"
#include "node/mempool.hpp"
#include "wallet/address.hpp"
#include "wallet/mnemonic.hpp"
#include "wallet/secure.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace quintum::wallet {

inline constexpr std::size_t kMaxWalletKeys = 100'000U;
inline constexpr std::size_t kWalletKeypoolSize = 100U;

enum class WalletStoreError {
    none,
    not_found,
    io_error,
    corrupt,
    wrong_network,
    target_exists,
    passphrase_required,
    invalid_passphrase,
    crypto_error,
};

enum class WalletStartError {
    none,
    store_failed,
    key_generation_failed,
};

struct WalletStartResult {
    WalletStartError error{WalletStartError::none};
    WalletStoreError store_error{WalletStoreError::none};
    bool created{false};
    bool backup_recommended{false};
    std::string receive_address{};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WalletStartError::none;
    }
};

enum class WalletKeyError {
    none,
    not_started,
    invalid_key,
    duplicate_key,
    key_limit,
    random_failed,
    store_failed,
};

struct WalletKeyResult {
    WalletKeyError error{WalletKeyError::none};
    WalletStoreError store_error{WalletStoreError::none};
    crypto::PublicKey public_key{};
    std::string address{};
    bool backup_recommended{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WalletKeyError::none;
    }
};

enum class WalletSyncError {
    none,
    not_started,
    chain_not_ready,
    height_overflow,
    active_chain_inconsistent,
    amount_overflow,
    store_failed,
};

struct WalletBalance {
    Amount confirmed{0U};
    Amount available{0U};
    Amount pending{0U};
    Amount immature{0U};

    bool operator==(const WalletBalance&) const = default;
};

enum class WalletTransactionStatus {
    unconfirmed,
    confirmed,
    inactive,
};

struct WalletTransactionRecord {
    Hash256 txid{};
    WalletTransactionStatus status{
        WalletTransactionStatus::unconfirmed
    };
    Amount received{0U};
    Amount spent{0U};
    std::optional<Amount> fee{};
    bool coinbase{false};
    std::optional<std::uint32_t> block_height{};
    std::optional<Hash256> block_hash{};
    std::uint32_t confirmations{0U};

    bool operator==(
        const WalletTransactionRecord&) const = default;
};

struct WalletSyncResult {
    WalletSyncError error{WalletSyncError::none};
    WalletBalance balance{};
    std::size_t confirmed_outputs{0U};
    std::size_t available_outputs{0U};
    std::size_t pending_outputs{0U};
    std::size_t blocks_scanned{0U};
    bool index_rebuilt{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WalletSyncError::none;
    }
};

inline constexpr std::uint32_t kWalletRecoveryGapLimit{100U};

enum class WalletRecoveryError {
    none,
    already_started,
    target_exists,
    invalid_mnemonic,
    invalid_passphrase,
    chain_not_ready,
    invalid_gap_limit,
    derivation_failed,
    key_limit,
    store_failed,
    sync_failed,
};

struct WalletRecoveryResult {
    WalletRecoveryError error{WalletRecoveryError::none};
    MnemonicError mnemonic_error{MnemonicError::none};
    WalletStoreError store_error{WalletStoreError::none};
    WalletSyncResult sync{};
    std::size_t receive_keys{0U};
    std::size_t change_keys{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WalletRecoveryError::none;
    }
};

struct WalletCoin {
    OutPoint outpoint{};
    Coin coin{};
    crypto::PublicKey public_key{};
};

enum class WalletCreateError {
    none,
    not_started,
    sync_failed,
    invalid_address,
    wrong_network_address,
    zero_amount,
    amount_out_of_range,
    fee_out_of_range,
    value_overflow,
    insufficient_funds,
    key_generation_failed,
    store_failed,
    missing_private_key,
    signing_failed,
    validation_failed,
};

struct WalletCreateResult {
    WalletCreateError error{WalletCreateError::none};
    AddressError address_error{AddressError::none};
    WalletSyncError sync_error{WalletSyncError::none};
    WalletStoreError store_error{WalletStoreError::none};
    consensus::InputAuthError auth_error{
        consensus::InputAuthError::none
    };
    UtxoApplyError validation_error{
        UtxoApplyError::none
    };
    Transaction transaction{};
    Hash256 txid{};
    Amount amount{0U};
    Amount fee{0U};
    Amount change{0U};
    Amount selected_value{0U};
    bool backup_recommended{false};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WalletCreateError::none;
    }
};

class Wallet {
public:
    Wallet(
        const consensus::ChainParams& params,
        std::filesystem::path directory
    );

    ~Wallet();

    Wallet(const Wallet&) = delete;
    Wallet& operator=(const Wallet&) = delete;

    [[nodiscard]] WalletStartResult start(
        std::string_view passphrase = {}
    );
    [[nodiscard]] bool started() const noexcept;

    [[nodiscard]] WalletKeyResult new_receive_address();

    [[nodiscard]] WalletStoreError encrypt_wallet(
        std::string_view passphrase
    );

    [[nodiscard]] WalletStoreError recover_from_seed(
        const RecoverySeed& seed,
        std::string_view passphrase
    );

    [[nodiscard]] WalletRecoveryResult recover_from_mnemonic(
        std::string_view mnemonic,
        std::string_view passphrase,
        const Chainstate& chain,
        const Mempool& mempool,
        std::uint32_t gap_limit = kWalletRecoveryGapLimit
    );

    [[nodiscard]] bool encrypted() const noexcept;

    [[nodiscard]] std::optional<RecoverySeed>
    recovery_seed() const noexcept;

    [[nodiscard]] std::optional<std::string>
    recovery_mnemonic() const;

    [[nodiscard]] WalletKeyResult import_private_key(
        const crypto::PrivateKey& private_key
    );

    [[nodiscard]] WalletStoreError backup(
        const std::filesystem::path& destination,
        bool overwrite = false
    ) const;

    [[nodiscard]] WalletSyncResult sync(
        const Chainstate& chain,
        const Mempool& mempool
    );

    [[nodiscard]] WalletCreateResult create_transaction(
        std::string_view destination,
        Amount amount,
        Amount fee,
        const Chainstate& chain,
        const Mempool& mempool
    );

    [[nodiscard]] WalletBalance balance() const noexcept;
    [[nodiscard]] std::vector<WalletTransactionRecord>
    history() const;
    [[nodiscard]] std::vector<std::string> addresses() const;

    [[nodiscard]] bool owns_public_key(
        const crypto::PublicKey& public_key
    ) const noexcept;

    [[nodiscard]] const std::filesystem::path&
    path() const noexcept;

private:
    struct KeyRecord {
        crypto::PrivateKey private_key{};
        crypto::PublicKey public_key{};
        bool internal{false};
        bool used{false};
        bool deterministic{false};
        std::uint32_t hd_index{0U};

        KeyRecord() = default;

        KeyRecord(
            const crypto::PrivateKey& secret,
            const crypto::PublicKey& public_value,
            bool internal_value,
            bool used_value,
            bool deterministic_value = false,
            std::uint32_t hd_index_value = 0U
        ) noexcept
            : private_key(secret),
              public_key(public_value),
              internal(internal_value),
              used(used_value),
              deterministic(deterministic_value),
              hd_index(hd_index_value)
        {
        }

        KeyRecord(const KeyRecord&) = default;
        KeyRecord& operator=(const KeyRecord&) = default;

        KeyRecord(KeyRecord&& other) noexcept
            : private_key(other.private_key),
              public_key(other.public_key),
              internal(other.internal),
              used(other.used),
              deterministic(other.deterministic),
              hd_index(other.hd_index)
        {
            crypto::secure_erase(
                other.private_key
            );
        }

        KeyRecord& operator=(KeyRecord&& other) noexcept
        {
            if (this == &other) {
                return *this;
            }

            crypto::secure_erase(private_key);

            private_key = other.private_key;
            public_key = other.public_key;
            internal = other.internal;
            used = other.used;
            deterministic = other.deterministic;
            hd_index = other.hd_index;

            crypto::secure_erase(
                other.private_key
            );

            return *this;
        }

        ~KeyRecord()
        {
            crypto::secure_erase(private_key);
        }
    };

    [[nodiscard]] WalletStoreError load(
        std::string_view passphrase
    );

    void load_index_state() noexcept;
    [[nodiscard]] WalletStoreError save_index_state() const;
    void reset_index_state() noexcept;
    [[nodiscard]] Hash256 wallet_index_id() const;

    [[nodiscard]] bool apply_confirmed_block_to_index(
        const Block& block,
        const Hash256& block_hash,
        std::uint32_t height,
        std::vector<crypto::PublicKey>& discovered_keys
    );
    [[nodiscard]] WalletStoreError save_keys(
        const std::vector<KeyRecord>& keys
    ) const;

    [[nodiscard]] WalletKeyResult append_key(
        const crypto::PrivateKey& private_key,
        bool internal,
        bool used
    );

    [[nodiscard]] WalletKeyResult reserve_key(
        bool internal
    );

    [[nodiscard]] bool generate_pool_records(
        std::vector<KeyRecord>& records,
        bool internal,
        std::size_t count
    ) const;

    [[nodiscard]] const crypto::PrivateKey*
    private_key_for(
        const crypto::PublicKey& public_key
    ) const noexcept;

    void clear_keys() noexcept;

    consensus::ChainParams params_{};
    std::filesystem::path directory_{};
    std::filesystem::path path_{};
    std::filesystem::path state_path_{};
    bool started_{false};
    bool encrypted_{false};
    std::optional<RecoverySeed> recovery_seed_{};
    WalletEncryptionKey encryption_key_{};
    WalletSalt encryption_salt_{};
    std::uint32_t argon2_memory_blocks_{
        kWalletArgon2MemoryBlocks
    };
    std::uint32_t argon2_passes_{
        kWalletArgon2Passes
    };
    std::vector<KeyRecord> keys_{};

    bool index_valid_{false};
    std::uint32_t indexed_height_{0U};
    Hash256 indexed_tip_{};
    std::vector<WalletTransactionRecord>
        confirmed_history_{};
    std::vector<WalletTransactionRecord>
        history_{};

    WalletBalance balance_{};
    std::map<OutPoint, WalletCoin, OutPointLess>
        confirmed_coins_{};
    std::map<OutPoint, WalletCoin, OutPointLess>
        available_coins_{};
    std::map<OutPoint, WalletCoin, OutPointLess>
        pending_coins_{};
};

} // namespace quintum::wallet
