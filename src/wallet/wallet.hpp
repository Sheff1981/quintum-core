#pragma once

#include "chain/chainstate.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/random.hpp"
#include "crypto/secp256k1.hpp"
#include "node/mempool.hpp"
#include "wallet/address.hpp"

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

struct WalletSyncResult {
    WalletSyncError error{WalletSyncError::none};
    WalletBalance balance{};
    std::size_t confirmed_outputs{0U};
    std::size_t available_outputs{0U};
    std::size_t pending_outputs{0U};

    [[nodiscard]] bool ok() const noexcept
    {
        return error == WalletSyncError::none;
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

    [[nodiscard]] WalletStartResult start();
    [[nodiscard]] bool started() const noexcept;

    [[nodiscard]] WalletKeyResult new_receive_address();

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

        KeyRecord() = default;

        KeyRecord(
            const crypto::PrivateKey& secret,
            const crypto::PublicKey& public_value,
            bool internal_value,
            bool used_value
        ) noexcept
            : private_key(secret),
              public_key(public_value),
              internal(internal_value),
              used(used_value)
        {
        }

        KeyRecord(const KeyRecord&) = default;
        KeyRecord& operator=(const KeyRecord&) = default;

        KeyRecord(KeyRecord&& other) noexcept
            : private_key(other.private_key),
              public_key(other.public_key),
              internal(other.internal),
              used(other.used)
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

    [[nodiscard]] WalletStoreError load();
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
    bool started_{false};
    std::vector<KeyRecord> keys_{};

    WalletBalance balance_{};
    std::map<OutPoint, WalletCoin, OutPointLess>
        confirmed_coins_{};
    std::map<OutPoint, WalletCoin, OutPointLess>
        available_coins_{};
    std::map<OutPoint, WalletCoin, OutPointLess>
        pending_coins_{};
};

} // namespace quintum::wallet
