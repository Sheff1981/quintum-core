#include "wallet/wallet.hpp"

#include "consensus/monetary.hpp"
#include "core/serialize.hpp"
#include "crypto/random.hpp"
#include "crypto/sha256.hpp"

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
constexpr std::uint32_t kWalletVersion{1U};
constexpr std::size_t kChecksumSize{32U};
constexpr Byte kInternalFlag{0x01U};
constexpr Byte kUsedFlag{0x02U};
constexpr Byte kKnownKeyFlags{
    kInternalFlag | kUsedFlag
};
constexpr std::size_t kKeyRecordSize{
    1U + crypto::PrivateKey{}.size()
};

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

bool restrict_permissions(
    const std::filesystem::path& path) noexcept
{
#ifdef _WIN32
    (void)path;
    return true;
#else
    return ::chmod(
               path.c_str(),
               S_IRUSR | S_IWUSR) == 0;
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

#ifndef _WIN32
    if (!restrict_permissions(temporary)) {
        (void)std::fclose(file);
        std::filesystem::remove(
            temporary,
            ec
        );
        return WalletStoreError::io_error;
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
        return WalletStoreError::io_error;
    }

    return WalletStoreError::none;
}

std::optional<Bytes> read_file(
    const std::filesystem::path& path)
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

    constexpr std::uintmax_t max_size{
        8U +
        sizeof(std::uint32_t) +
        1U +
        4U +
        9U +
        static_cast<std::uintmax_t>(
            kMaxWalletKeys) *
            static_cast<std::uintmax_t>(
                kKeyRecordSize) +
        kChecksumSize
    };

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
        return std::nullopt;
    }

    return bytes;
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
      path_(directory_ / "wallet.dat")
{
}

Wallet::~Wallet()
{
    clear_keys();
}

WalletStartResult Wallet::start()
{
    WalletStartResult out;

    if (started_) {
        const auto existing =
            addresses();

        if (!existing.empty()) {
            out.receive_address =
                existing.front();
        }

        return out;
    }

    out.store_error = load();

    if (out.store_error ==
        WalletStoreError::none) {
        started_ = true;

        const auto existing =
            addresses();

        if (!existing.empty()) {
            out.receive_address =
                existing.front();
        }

        return out;
    }

    if (out.store_error !=
        WalletStoreError::not_found) {
        out.error =
            WalletStartError::store_failed;
        return out;
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

        out.error =
            WalletStartError::store_failed;
        return out;
    }

    keys_ = std::move(initial);
    started_ = true;
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

WalletKeyResult Wallet::import_private_key(
    const crypto::PrivateKey& private_key)
{
    if (!started_) {
        WalletKeyResult out;
        out.error =
            WalletKeyError::not_started;
        return out;
    }

    return append_key(
        private_key,
        false,
        true
    );
}

WalletStoreError Wallet::backup(
    const std::filesystem::path& destination,
    bool overwrite) const
{
    if (!started_) {
        return WalletStoreError::io_error;
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

WalletSyncResult Wallet::sync(
    const Chainstate& chain,
    const Mempool& mempool)
{
    WalletSyncResult out;

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

    const std::uint32_t spend_height =
        *height + 1U;

    std::map<
        OutPoint,
        WalletCoin,
        OutPointLess> confirmed;

    std::vector<crypto::PublicKey>
        discovered_keys;

    const auto remember_key =
        [&](const crypto::PublicKey& public_key) {
            if (std::find(
                    discovered_keys.begin(),
                    discovered_keys.end(),
                    public_key) ==
                discovered_keys.end()) {
                discovered_keys.push_back(
                    public_key
                );
            }
        };

    for (std::uint64_t current = 0U;
         current <=
             static_cast<std::uint64_t>(
                 *height);
         ++current) {
        const auto active_hash =
            chain.active_hash(
                static_cast<std::uint32_t>(
                    current));

        if (!active_hash) {
            out.error =
                WalletSyncError::
                    active_chain_inconsistent;
            return out;
        }

        const Block* block =
            chain.block(*active_hash);

        if (block == nullptr) {
            out.error =
                WalletSyncError::
                    active_chain_inconsistent;
            return out;
        }

        for (const auto& tx :
             block->transactions) {
            for (const auto& input :
                 tx.inputs) {
                confirmed.erase(
                    input.previous_output
                );
            }

            const Hash256 txid =
                transaction_id(tx);

            for (std::size_t index = 0U;
                 index < tx.outputs.size();
                 ++index) {
                if (index >
                    static_cast<std::size_t>(
                        std::numeric_limits<
                            std::uint32_t>::max())) {
                    out.error =
                        WalletSyncError::
                            active_chain_inconsistent;
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

                remember_key(*public_key);

                remember_key(*public_key);

                const OutPoint outpoint{
                    .txid = txid,
                    .index =
                        static_cast<
                            std::uint32_t>(
                                index),
                };

                confirmed.emplace(
                    outpoint,
                    WalletCoin{
                        .outpoint = outpoint,
                        .coin = Coin{
                            .output =
                                tx.outputs[index],
                            .height =
                                static_cast<
                                    std::uint32_t>(
                                        current),
                            .coinbase =
                                tx.is_coinbase(),
                        },
                        .public_key =
                            *public_key,
                    }
                );
            }
        }
    }

    std::map<
        OutPoint,
        WalletCoin,
        OutPointLess> available;

    WalletBalance next_balance;

    for (const auto& [outpoint, coin] :
         confirmed) {
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

    for (const auto& tx :
         mempool.transactions()) {
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

                if (value >
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

                if (value >
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
            transaction_id(tx);

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

            pending.emplace(
                outpoint,
                std::move(owned)
            );
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

    confirmed_coins_ =
        std::move(confirmed);
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

WalletStoreError Wallet::load()
{
    clear_keys();

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

    if (!bytes ||
        bytes->size() <
            kWalletMagic.size() +
            sizeof(std::uint32_t) +
            1U +
            params_.message_start.size() +
            1U +
            kKeyRecordSize +
            kChecksumSize) {
        return WalletStoreError::corrupt;
    }

    SecretBytesGuard guard{&*bytes};

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

    std::size_t offset{0U};

    if (!std::equal(
            kWalletMagic.begin(),
            kWalletMagic.end(),
            body.begin())) {
        return WalletStoreError::corrupt;
    }

    offset +=
        kWalletMagic.size();

    const auto version =
        read_little_endian<
            std::uint32_t>(
                body,
                offset
            );

    if (!version ||
        *version != kWalletVersion) {
        return WalletStoreError::corrupt;
    }

    if (offset >= body.size()) {
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
            for (auto& key : loaded) {
                crypto::secure_erase(
                    key.private_key
                );
            }
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
            for (auto& key : loaded) {
                crypto::secure_erase(
                    key.private_key
                );
            }
            return WalletStoreError::corrupt;
        }

        crypto::PrivateKey private_key{};

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
            crypto::secure_erase(
                private_key
            );
            for (auto& key : loaded) {
                crypto::secure_erase(
                    key.private_key
                );
            }
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
            crypto::secure_erase(
                private_key
            );
            for (auto& key : loaded) {
                crypto::secure_erase(
                    key.private_key
                );
            }
            return WalletStoreError::corrupt;
        }

        loaded.push_back(
            KeyRecord{
                .private_key =
                    private_key,
                .public_key =
                    *public_key,
                .internal =
                    (flags &
                     kInternalFlag) != 0U,
                .used =
                    (flags &
                     kUsedFlag) != 0U,
            }
        );

        crypto::secure_erase(
            private_key
        );
    }

    if (offset != body.size()) {
        for (auto& key : loaded) {
            crypto::secure_erase(
                key.private_key
            );
        }
        return WalletStoreError::corrupt;
    }

    keys_ = std::move(loaded);

#ifndef _WIN32
    if (!restrict_permissions(path_)) {
        clear_keys();
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
        kWalletVersion
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

    candidate.push_back(
        KeyRecord{
            .private_key =
                private_key,
            .public_key =
                *public_key,
            .internal =
                internal,
            .used =
                used,
        }
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

    keys_.push_back(
        KeyRecord{
            .private_key =
                private_key,
            .public_key =
                *public_key,
            .internal =
                internal,
            .used =
                used,
        }
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
        candidate[index].used = true;

        out.public_key =
            candidate[index].public_key;
        out.address =
            encode_address(
                params_.network,
                out.public_key
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

        keys_[index].used = true;
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

        records.push_back(
            KeyRecord{
                .private_key =
                    *private_key,
                .public_key =
                    *public_key,
                .internal =
                    internal,
                .used = false,
            }
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
