#include "net/runtime.hpp"

#include "consensus/tx_auth.hpp"
#include "core/serialize.hpp"
#include "crypto/random.hpp"
#include "crypto/sha256.hpp"
#include "net/relay.hpp"
#include "net/sync.hpp"

#include <algorithm>
#ifdef __ANDROID__
#include <android/log.h>
#endif
#include <array>
#include <chrono>
#include <limits>
#include <random>
#include <thread>
#include <utility>

namespace quintum::net {
namespace {
#ifdef __ANDROID__
void log_outbound_failure(const char* phase, const PeerAddress& address, int code) noexcept
{
    __android_log_print(ANDROID_LOG_WARN, "QUINTUM-P2P",
        "%s peer=%s:%u error=%d", phase,
        format_peer_host(address).c_str(),
        static_cast<unsigned>(address.port), code);
}
#endif


std::uint64_t unix_time_now() noexcept
{
    const auto seconds =
        std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now()
                .time_since_epoch()
        ).count();

    return seconds < 0
        ? 0U
        : static_cast<std::uint64_t>(seconds);
}

bool same_endpoint(
    const PeerAddress& a,
    const PeerAddress& b) noexcept
{
    if (a.network != b.network ||
        a.port != b.port) {
        return false;
    }

    if (a.network == AddressNetwork::ipv4) {
        return a.ipv4 == b.ipv4;
    }

    return a.host == b.host;
}

bool contains_hash(
    const std::vector<Hash256>& values,
    const Hash256& hash)
{
    return std::find(
               values.begin(),
               values.end(),
               hash) != values.end();
}

void erase_hash(
    std::vector<Hash256>& values,
    const Hash256& hash)
{
    values.erase(
        std::remove(
            values.begin(),
            values.end(),
            hash
        ),
        values.end()
    );
}

bool elapsed(
    std::uint64_t now,
    std::uint64_t since,
    std::uint64_t interval) noexcept
{
    return now >= since &&
           now - since >= interval;
}

std::uint64_t deadline_after(
    std::uint64_t now,
    std::uint64_t delay) noexcept
{
    return now >
               std::numeric_limits<
                   std::uint64_t>::max() -
                   delay
        ? std::numeric_limits<
              std::uint64_t>::max()
        : now + delay;
}

void append_text(
    Bytes& out,
    std::string_view value)
{
    append_compact_size(
        out,
        static_cast<std::uint64_t>(
            value.size()
        )
    );

    for (const char raw : value) {
        const auto ch =
            static_cast<unsigned char>(raw);
        out.push_back(
            static_cast<Byte>(ch)
        );
    }
}

Hash256 make_preview_id(
    const NetworkWalletSendPreview& preview)
{
    Bytes bytes;
    append_text(
        bytes,
        "QUINTUM-SEND-PREVIEW-V1"
    );

    bytes.insert(
        bytes.end(),
        preview.state_hash.begin(),
        preview.state_hash.end()
    );

    append_text(
        bytes,
        preview.destination
    );

    append_little_endian(
        bytes,
        preview.amount
    );
    append_little_endian(
        bytes,
        preview.quote.fee
    );
    append_little_endian(
        bytes,
        preview.quote.fee_rate_per_kb
    );
    append_little_endian(
        bytes,
        preview.quote.change
    );
    append_little_endian(
        bytes,
        preview.quote.selected_value
    );
    append_little_endian(
        bytes,
        static_cast<std::uint64_t>(
            preview.quote.serialized_size
        )
    );
    append_little_endian(
        bytes,
        static_cast<std::uint64_t>(
            preview.quote.input_count
        )
    );
    append_little_endian(
        bytes,
        static_cast<std::uint64_t>(
            preview.quote.output_count
        )
    );

    return crypto::double_sha256(bytes);
}

bool same_fee_quote(
    const wallet::WalletFeeQuote& lhs,
    const wallet::WalletFeeQuote& rhs) noexcept
{
    return lhs.error == rhs.error &&
           lhs.address_error == rhs.address_error &&
           lhs.sync_error == rhs.sync_error &&
           lhs.amount == rhs.amount &&
           lhs.fee == rhs.fee &&
           lhs.fee_rate_per_kb ==
               rhs.fee_rate_per_kb &&
           lhs.change == rhs.change &&
           lhs.selected_value ==
               rhs.selected_value &&
           lhs.serialized_size ==
               rhs.serialized_size &&
           lhs.input_count ==
               rhs.input_count &&
           lhs.output_count ==
               rhs.output_count;
}

std::uint64_t make_runtime_nonce() noexcept
{
    try {
        std::random_device random;
        const std::uint64_t high =
            static_cast<std::uint64_t>(random());
        const std::uint64_t low =
            static_cast<std::uint64_t>(random());

        const std::uint64_t value =
            (high << 32U) ^ low ^
            unix_time_now();

        return value == 0U ? 1U : value;
    } catch (...) {
        const auto fallback =
            unix_time_now() ^
            static_cast<std::uint64_t>(
                reinterpret_cast<std::uintptr_t>(
                    &make_runtime_nonce));

        return fallback == 0U ? 1U : fallback;
    }
}

} // namespace

NetworkRuntime::NetworkRuntime(
    const consensus::ChainParams& params,
    std::filesystem::path directory)
    : params_(params),
      directory_(std::move(directory)),
      node_(params_, directory_),
      wallet_(params_, directory_),
      addrman_(
          params_,
          directory_,
          params_.network ==
              consensus::Network::regtest),
      discovery_(addrman_),
      listener_(params_)
{
}

NetworkRuntime::~NetworkRuntime()
{
    stop();
}

NetworkRuntimeStartResult NetworkRuntime::start(
    NetworkRuntimeConfig config)
{
    NetworkRuntimeStartResult out;

    if (running_.load()) {
        out.error =
            NetworkRuntimeStartError::
                already_running;
        return out;
    }

    config_ = std::move(config);
    wallet_enabled_ = config_.wallet_enabled;

    if (config_.max_block_requests_in_flight == 0U) {
        out.error =
            NetworkRuntimeStartError::
                invalid_configuration;
        return out;
    }

    if (config_.max_pending_compact_blocks == 0U) {
        config_.max_pending_compact_blocks =
            config_.max_block_requests_in_flight;
    }

    if (config_.max_pending_compact_blocks >
        config_.max_block_requests_in_flight) {
        out.error =
            NetworkRuntimeStartError::
                invalid_configuration;
        return out;
    }

    if (!wallet_enabled_ &&
        (!config_.wallet_passphrase.empty() ||
         !config_.wallet_recovery_mnemonic.empty())) {
        if (!config_.wallet_passphrase.empty()) {
            crypto::secure_erase(
                std::span<Byte>{
                    reinterpret_cast<Byte*>(
                        config_.wallet_passphrase.data()),
                    config_.wallet_passphrase.size()
                }
            );
            config_.wallet_passphrase.clear();
            config_.wallet_passphrase.shrink_to_fit();
        }

        if (!config_.wallet_recovery_mnemonic.empty()) {
            crypto::secure_erase(
                std::span<Byte>{
                    reinterpret_cast<Byte*>(
                        config_.wallet_recovery_mnemonic.data()),
                    config_.wallet_recovery_mnemonic.size()
                }
            );
            config_.wallet_recovery_mnemonic.clear();
            config_.wallet_recovery_mnemonic.shrink_to_fit();
        }

        out.error =
            NetworkRuntimeStartError::
                invalid_configuration;
        return out;
    }

    const bool allow_local =
        config_.allow_local_peers ||
        params_.network ==
            consensus::Network::regtest;

    addrman_.set_allow_local(allow_local);

    {
        std::scoped_lock lock(state_mutex_);

        out.node = node_.start();

        if (out.node.ok() &&
            wallet_enabled_) {
            if (!config_.wallet_recovery_mnemonic.empty()) {
                out.wallet_recovery =
                    wallet_.recover_from_mnemonic(
                        config_.wallet_recovery_mnemonic,
                        config_.wallet_passphrase,
                        node_.chain(),
                        node_.mempool(),
                        config_.wallet_recovery_gap_limit
                    );

                out.recovered_wallet =
                    out.wallet_recovery.ok();

                if (out.wallet_recovery.ok()) {
                    out.wallet.error =
                        wallet::WalletStartError::none;
                    out.wallet.created = true;
                    out.wallet.backup_recommended = true;

                    const auto recovered_addresses =
                        wallet_.addresses();

                    if (!recovered_addresses.empty()) {
                        out.wallet.receive_address =
                            recovered_addresses.back();
                    }

                    out.wallet_sync =
                        out.wallet_recovery.sync.error;
                } else {
                    out.wallet.error =
                        wallet::WalletStartError::
                            store_failed;
                    out.wallet.store_error =
                        out.wallet_recovery.store_error;
                }
            } else {
                out.wallet =
                    wallet_.start(
                        config_.wallet_passphrase
                    );

                if (out.wallet.ok()) {
                    const auto synced =
                        wallet_.sync(
                            node_.chain(),
                            node_.mempool()
                        );

                    out.wallet_sync =
                        synced.error;
                }
            }
        }
    }

    if (!config_.wallet_passphrase.empty()) {
        crypto::secure_erase(
            std::span<Byte>{
                reinterpret_cast<Byte*>(
                    config_.wallet_passphrase.data()),
                config_.wallet_passphrase.size()
            }
        );
        config_.wallet_passphrase.clear();
        config_.wallet_passphrase.shrink_to_fit();
    }

    if (!config_.wallet_recovery_mnemonic.empty()) {
        crypto::secure_erase(
            std::span<Byte>{
                reinterpret_cast<Byte*>(
                    config_.wallet_recovery_mnemonic.data()),
                config_.wallet_recovery_mnemonic.size()
            }
        );
        config_.wallet_recovery_mnemonic.clear();
        config_.wallet_recovery_mnemonic.shrink_to_fit();
    }

    if (!out.node.ok()) {
        out.error =
            NetworkRuntimeStartError::node_failed;
        return out;
    }

    if (wallet_enabled_ &&
        (!out.wallet.ok() ||
         out.wallet_sync !=
             wallet::WalletSyncError::none)) {
        out.error =
            NetworkRuntimeStartError::wallet_failed;
        return out;
    }

    const std::uint64_t now =
        unix_time_now();

    out.address_store =
        discovery_.initialize(
            params_.network,
            now
        );

    if (out.address_store !=
        AddrStoreError::none) {
        out.error =
            NetworkRuntimeStartError::
                address_store_failed;
        return out;
    }

    if (!config_.bootstrap_peers.empty()) {
        (void)addrman_.add(
            config_.bootstrap_peers
        );

        out.address_store =
            addrman_.save();

        if (out.address_store !=
            AddrStoreError::none) {
            out.error =
                NetworkRuntimeStartError::
                    address_store_failed;
            return out;
        }
    }

    known_address_count_.store(
        addrman_.size()
    );

    const bool using_default_port =
        !config_.listen_port.has_value();

    const std::uint16_t port =
        config_.listen_port.value_or(
            params_.p2p_port
        );

    out.peer_error =
        listener_.listen(
            config_.bind_address,
            port
        );

    if (out.peer_error != PeerError::none &&
        using_default_port &&
        config_.
            allow_ephemeral_listener_fallback &&
        (out.peer_error ==
             PeerError::bind_failed ||
         out.peer_error ==
             PeerError::listen_failed)) {
        out.peer_error =
            listener_.listen(
                config_.bind_address,
                0U
            );
    }

    if (out.peer_error != PeerError::none) {
        out.error =
            NetworkRuntimeStartError::
                listener_failed;
        return out;
    }

    runtime_nonce_ =
        make_runtime_nonce();
    ping_counter_ = 0U;
    next_outbound_attempt_ = now;
    stop_requested_.store(false);
    listen_port_.store(
        listener_.local_port()
    );
    running_.store(true);

    try {
        worker_ = std::thread(
            [this] {
                run_loop();
            }
        );
    } catch (...) {
        running_.store(false);
        stop_requested_.store(true);
        listener_.close();
        listen_port_.store(0U);
        out.error =
            NetworkRuntimeStartError::
                worker_start_failed;
        return out;
    }

    if (config_.enable_nat_mapping) {
        try {
            nat_worker_ = std::thread(
                [this] {
                    run_nat_loop();
                }
            );
        } catch (...) {
            // Automatic reachability is best-effort. Failure to create
            // its worker must never prevent the node from operating.
            nat_mapping_method_.store(
                NatMappingMethod::none
            );
            nat_external_port_.store(0U);
        }
    }

    return out;
}

void NetworkRuntime::stop() noexcept
{
    if (!running_.load() &&
        !worker_.joinable() &&
        !nat_worker_.joinable()) {
        return;
    }

    stop_requested_.store(true);

    if (worker_.joinable()) {
        worker_.join();
    }

    if (nat_worker_.joinable()) {
        nat_worker_.join();
    }

    nat_mapper_.unmap();
    nat_mapping_method_.store(
        NatMappingMethod::none
    );
    nat_external_port_.store(0U);

    listener_.close();

    for (auto& peer : peers_) {
        peer.session.close();
    }
    peers_.clear();
    reconnect_candidates_.clear();
    stem_relays_.clear();
    invalid_object_cache_.clear();
    origin_stem_route_.reset();
    origin_stem_epoch_deadline_ = 0U;

    {
        std::scoped_lock lock(
            announcement_mutex_
        );
        announcements_.clear();
        pending_private_transactions_.clear();
    }

    peer_count_.store(0U);
    outbound_count_.store(0U);
    listen_port_.store(0U);
    running_.store(false);
}

bool NetworkRuntime::running() const noexcept
{
    return running_.load();
}

NetworkRuntimeStatus NetworkRuntime::status() const
{
    NetworkRuntimeStatus out;
    out.running = running_.load();
    out.wallet_enabled = wallet_enabled_;
    out.listen_port = listen_port_.load();
    out.peers = peer_count_.load();
    out.outbound_peers =
        outbound_count_.load();
    out.known_addresses =
        known_address_count_.load();
    out.nat_mapping_method =
        nat_mapping_method_.load();
    out.nat_external_port =
        nat_external_port_.load();

    std::scoped_lock lock(state_mutex_);

    out.height = node_.chain().height();

    if (have_peer_height_.load()) {
        out.peer_best_height =
            peer_best_height_.load();
    }

    if (out.height &&
        out.peer_best_height &&
        *out.peer_best_height > *out.height) {
        out.synchronizing = true;
        out.sync_progress =
            static_cast<double>(*out.height) /
            static_cast<double>(
                *out.peer_best_height
            );
    } else {
        out.synchronizing = false;
        out.sync_progress = 1.0;
    }

    out.tip = node_.chain().tip_hash();

    if (out.height) {
        const auto header =
            node_.chain().active_header(
                *out.height
            );

        if (header) {
            out.difficulty_bits =
                header->bits;
            out.difficulty =
                consensus::difficulty_from_bits(
                    header->bits,
                    params_.pow.pow_limit_bits
                );
        }
    }

    out.mempool_transactions =
        node_.mempool().size();
    out.min_relay_fee_rate_per_kb =
        node_.mempool()
            .min_relay_fee_rate_per_kb();
    out.recommended_fee_rate_per_kb =
        wallet::recommended_fee_rate(
            node_.mempool()
        );

    if (wallet_enabled_) {
        out.wallet_balance =
            wallet_.balance();

        const auto wallet_addresses =
            wallet_.addresses();

        if (!wallet_addresses.empty()) {
            out.receive_address =
                wallet_addresses.back();
        }
    }

    return out;
}

std::vector<wallet::WalletTransactionRecord>
NetworkRuntime::wallet_history() const
{
    std::scoped_lock lock(state_mutex_);

    if (!wallet_enabled_) {
        return {};
    }

    return wallet_.history();
}

WalletDesktopSnapshot
NetworkRuntime::desktop_snapshot() const
{
    WalletDesktopSnapshot out;

    out.status.running =
        running_.load();
    out.status.wallet_enabled =
        wallet_enabled_;
    out.status.listen_port =
        listen_port_.load();
    out.status.peers =
        peer_count_.load();
    out.status.outbound_peers =
        outbound_count_.load();
    out.status.known_addresses =
        known_address_count_.load();
    out.status.nat_mapping_method =
        nat_mapping_method_.load();
    out.status.nat_external_port =
        nat_external_port_.load();

    std::scoped_lock lock(state_mutex_);

    out.status.height =
        node_.chain().height();

    if (have_peer_height_.load()) {
        out.status.peer_best_height =
            peer_best_height_.load();
    }

    if (out.status.height &&
        out.status.peer_best_height &&
        *out.status.peer_best_height >
            *out.status.height) {
        out.status.synchronizing = true;
        out.status.sync_progress =
            static_cast<double>(
                *out.status.height
            ) /
            static_cast<double>(
                *out.status.peer_best_height
            );
    } else {
        out.status.synchronizing = false;
        out.status.sync_progress = 1.0;
    }

    out.status.tip =
        node_.chain().tip_hash();

    if (out.status.height) {
        const auto header =
            node_.chain().active_header(
                *out.status.height
            );

        if (header) {
            out.status.difficulty_bits =
                header->bits;
            out.status.difficulty =
                consensus::difficulty_from_bits(
                    header->bits,
                    params_.pow.pow_limit_bits
                );
        }
    }

    out.status.mempool_transactions =
        node_.mempool().size();
    out.status.min_relay_fee_rate_per_kb =
        node_.mempool()
            .min_relay_fee_rate_per_kb();
    out.status.recommended_fee_rate_per_kb =
        wallet::recommended_fee_rate(
            node_.mempool()
        );

    if (wallet_enabled_) {
        out.status.wallet_balance =
            wallet_.balance();

        const auto addresses =
            wallet_.addresses();

        if (!addresses.empty()) {
            out.status.receive_address =
                addresses.back();
        }

        out.address_book =
            wallet_.address_book();

        const auto history =
            wallet_.history();

        out.transactions.reserve(
            history.size()
        );

        for (const auto& record : history) {
            out.transactions.push_back(
                WalletTransactionView{
                    .record = record,
                    .label =
                        wallet_.transaction_label(
                            record.txid
                        ),
                }
            );
        }
    }

    return out;
}

wallet::WalletMetadataError
NetworkRuntime::set_address_label(
    std::string_view address,
    std::string_view label)
{
    std::scoped_lock lock(state_mutex_);

    return wallet_.set_address_label(
        address,
        label
    );
}

wallet::WalletMetadataError
NetworkRuntime::set_transaction_label(
    const Hash256& txid,
    std::string_view label)
{
    std::scoped_lock lock(state_mutex_);

    return wallet_.set_transaction_label(
        txid,
        label
    );
}

std::optional<std::string>
NetworkRuntime::wallet_recovery_mnemonic() const
{
    std::scoped_lock lock(state_mutex_);
    return wallet_.recovery_mnemonic();
}

bool NetworkRuntime::verify_wallet_passphrase(
    std::string_view passphrase) const
{
    std::scoped_lock lock(state_mutex_);

    return wallet_.verify_passphrase(
        passphrase
    );
}

bool NetworkRuntime::wallet_encrypted() const
{
    std::scoped_lock lock(state_mutex_);
    return wallet_.encrypted();
}

NodeTransactionResult
NetworkRuntime::submit_transaction(
    const Transaction& transaction)
{
    NodeTransactionResult out;

    {
        std::scoped_lock lock(state_mutex_);
        out = node_.submit_transaction(
            transaction
        );

        if (out.ok()) {
            (void)sync_wallet_locked();
        }
    }

    if (out.ok()) {
        queue_private_transaction(
            out.mempool.txid
        );
    }

    return out;
}

wallet::WalletKeyResult
NetworkRuntime::new_receive_address()
{
    std::scoped_lock lock(state_mutex_);
    return wallet_.new_receive_address();
}

wallet::WalletStoreError
NetworkRuntime::encrypt_wallet(
    std::string_view passphrase)
{
    std::scoped_lock lock(state_mutex_);
    return wallet_.encrypt_wallet(
        passphrase
    );
}

wallet::WalletStoreError
NetworkRuntime::backup_wallet(
    const std::filesystem::path& destination,
    bool overwrite)
{
    std::scoped_lock lock(state_mutex_);
    return wallet_.backup(
        destination,
        overwrite
    );
}

wallet::WalletStoreError
NetworkRuntime::backup_wallet_bundle(
    const std::filesystem::path& destination,
    bool overwrite)
{
    std::scoped_lock lock(state_mutex_);
    return wallet_.backup_bundle(
        destination,
        overwrite
    );
}

wallet::WalletStoreError
NetworkRuntime::restore_wallet_bundle(
    const std::filesystem::path& source)
{
    std::scoped_lock lock(state_mutex_);

    if (running_.load()) {
        return wallet::WalletStoreError::
            target_exists;
    }

    return wallet_.restore_bundle(source);
}

wallet::WalletFeeQuote
NetworkRuntime::quote_send_fee(
    std::string_view destination,
    Amount amount)
{
    std::scoped_lock lock(state_mutex_);

    return wallet_.quote_auto_fee(
        destination,
        amount,
        node_.chain(),
        node_.mempool()
    );
}

Hash256
NetworkRuntime::send_state_hash_locked() const
{
    Bytes bytes;

    append_text(
        bytes,
        "QUINTUM-SEND-STATE-V1"
    );

    const auto height =
        node_.chain().height();

    bytes.push_back(
        height ? 1U : 0U
    );

    if (height) {
        append_little_endian(
            bytes,
            *height
        );
    }

    const auto tip =
        node_.chain().tip_hash();

    bytes.push_back(
        tip ? 1U : 0U
    );

    if (tip) {
        bytes.insert(
            bytes.end(),
            tip->begin(),
            tip->end()
        );
    }

    const auto& entries =
        node_.mempool().entries();

    append_compact_size(
        bytes,
        static_cast<std::uint64_t>(
            entries.size()
        )
    );

    for (const auto& entry : entries) {
        bytes.insert(
            bytes.end(),
            entry.txid.begin(),
            entry.txid.end()
        );

        append_little_endian(
            bytes,
            entry.fee
        );

        append_little_endian(
            bytes,
            static_cast<std::uint64_t>(
                entry.serialized_size
            )
        );
    }

    return crypto::double_sha256(bytes);
}

NetworkWalletSendPreview
NetworkRuntime::preview_send(
    std::string_view destination,
    Amount amount)
{
    std::scoped_lock lock(state_mutex_);

    NetworkWalletSendPreview out;
    out.destination =
        std::string{destination};
    out.amount = amount;
    out.state_hash =
        send_state_hash_locked();

    out.quote =
        wallet_.quote_auto_fee(
            destination,
            amount,
            node_.chain(),
            node_.mempool()
        );

    if (!out.quote.ok()) {
        return out;
    }

    const auto decoded =
        wallet::decode_address(
            params_.network,
            destination
        );

    const std::string canonical_destination =
        decoded.ok()
            ? wallet::encode_address(
                  params_.network,
                  decoded.public_key
              )
            : std::string{destination};

    const auto address_book =
        wallet_.address_book();

    const auto found =
        std::find_if(
            address_book.begin(),
            address_book.end(),
            [&](const wallet::WalletAddressBookEntry& entry) {
                return entry.address ==
                       canonical_destination;
            }
        );

    if (found !=
        address_book.end()) {
        out.recipient_label =
            found->label;
    }

    out.preview_id =
        make_preview_id(out);

    return out;
}

NetworkWalletSendResult
NetworkRuntime::send_to_address_auto_fee_locked(
    std::string_view destination,
    Amount amount)
{
    NetworkWalletSendResult out;

    out.wallet =
        wallet_.create_transaction_auto_fee(
            destination,
            amount,
            node_.chain(),
            node_.mempool()
        );

    if (!out.wallet.ok()) {
        out.error =
            NetworkWalletSendError::
                wallet_create_failed;
        return out;
    }

    out.node =
        node_.submit_transaction(
            out.wallet.transaction
        );

    if (!out.node.ok()) {
        out.error =
            NetworkWalletSendError::
                node_rejected;
        return out;
    }

    const auto synced =
        wallet_.sync(
            node_.chain(),
            node_.mempool()
        );

    out.wallet_sync =
        synced.error;

    if (!synced.ok()) {
        out.error =
            NetworkWalletSendError::
                wallet_sync_failed;
    }

    return out;
}

NetworkWalletSendResult
NetworkRuntime::confirm_send(
    const NetworkWalletSendPreview& preview,
    std::string_view passphrase)
{
    NetworkWalletSendResult out;

    {
        std::scoped_lock lock(state_mutex_);

        if (!preview.ok() ||
            make_preview_id(preview) !=
                preview.preview_id) {
            out.error =
                NetworkWalletSendError::
                    invalid_preview;
            return out;
        }

        if (send_state_hash_locked() !=
            preview.state_hash) {
            out.error =
                NetworkWalletSendError::
                    stale_preview;
            return out;
        }

        const auto current_quote =
            wallet_.quote_auto_fee(
                preview.destination,
                preview.amount,
                node_.chain(),
                node_.mempool()
            );

        if (!current_quote.ok() ||
            !same_fee_quote(
                current_quote,
                preview.quote)) {
            out.error =
                NetworkWalletSendError::
                    stale_preview;
            return out;
        }

        if (wallet_.encrypted()) {
            if (passphrase.empty()) {
                out.error =
                    NetworkWalletSendError::
                        passphrase_required;
                return out;
            }

            if (!wallet_.verify_passphrase(
                    passphrase)) {
                out.error =
                    NetworkWalletSendError::
                        invalid_passphrase;
                return out;
            }
        }

        out =
            send_to_address_auto_fee_locked(
                preview.destination,
                preview.amount
            );
    }

    if (out.ok()) {
        queue_private_transaction(
            out.node.mempool.txid
        );
    }

    return out;
}

NetworkWalletSendResult
NetworkRuntime::send_to_address_auto_fee(
    std::string_view destination,
    Amount amount)
{
    NetworkWalletSendResult out;

    {
        std::scoped_lock lock(state_mutex_);

        out =
            send_to_address_auto_fee_locked(
                destination,
                amount
            );
    }

    if (out.ok()) {
        queue_private_transaction(
            out.node.mempool.txid
        );
    }

    return out;
}

NetworkWalletSendResult
NetworkRuntime::send_to_address(
    std::string_view destination,
    Amount amount,
    Amount fee)
{
    NetworkWalletSendResult out;

    {
        std::scoped_lock lock(state_mutex_);

        out.wallet =
            wallet_.create_transaction(
                destination,
                amount,
                fee,
                node_.chain(),
                node_.mempool()
            );

        if (!out.wallet.ok()) {
            out.error =
                NetworkWalletSendError::
                    wallet_create_failed;
            return out;
        }

        out.node =
            node_.submit_transaction(
                out.wallet.transaction
            );

        if (!out.node.ok()) {
            out.error =
                NetworkWalletSendError::
                    node_rejected;
            return out;
        }

        const auto synced =
            wallet_.sync(
                node_.chain(),
                node_.mempool()
            );

        out.wallet_sync =
            synced.error;

        if (!synced.ok()) {
            out.error =
                NetworkWalletSendError::
                    wallet_sync_failed;
            return out;
        }
    }

    queue_private_transaction(
        out.node.mempool.txid
    );

    return out;
}

NodeMineResult
NetworkRuntime::mine_mempool_block(
    const Bytes& payout_script,
    std::uint64_t max_attempts)
{
    return mine_mempool_block_at(
        payout_script,
        unix_time_now(),
        max_attempts
    );
}

NodeMineResult
NetworkRuntime::mine_mempool_block_parallel(
    const Bytes& payout_script,
    std::uint64_t max_attempts,
    std::size_t worker_count,
    bool full_memory,
    const std::atomic<bool>* cancel)
{
    NodeMineResult out;

    {
        std::scoped_lock lock(state_mutex_);
        out = node_.mine_mempool_block_parallel_at(
            payout_script,
            unix_time_now(),
            max_attempts,
            worker_count,
            full_memory,
            wallet_mining_nonce_,
            cancel
        );

        if (out.error == NodeMineError::proof_of_work_exhausted) {
            wallet_mining_nonce_ =
                out.mining.nonce == std::numeric_limits<std::uint64_t>::max()
                    ? 0U
                    : out.mining.nonce + 1U;
        } else if (out.ok()) {
            wallet_mining_nonce_ = 0U;
        }
    }

    if (out.ok()) {
        queue_announcement(
            kInventoryBlock,
            block_hash(out.block.header)
        );
    }

    return out;
}

NodeMineResult
NetworkRuntime::mine_wallet_block(
    std::uint64_t max_attempts)
{
    std::string address;

    {
        std::scoped_lock lock(state_mutex_);

        const auto addresses =
            wallet_.addresses();

        if (addresses.empty()) {
            NodeMineResult out;
            out.error =
                NodeMineError::template_failed;
            return out;
        }

        address = addresses.back();
    }

    const auto decoded =
        wallet::decode_address(
            params_.network,
            address
        );

    if (!decoded.ok()) {
        NodeMineResult out;
        out.error =
            NodeMineError::template_failed;
        return out;
    }

    const Bytes payout_script =
        consensus::make_p2pk_locking_script(
            decoded.public_key
        );

    NodeMineResult out;

    {
        std::scoped_lock lock(state_mutex_);

        std::size_t mining_threads =
            config_.randomx_mining_threads;

        if (mining_threads == 0U) {
            const unsigned hardware =
                std::thread::
                    hardware_concurrency();

            mining_threads =
                hardware > 1U
                    ? static_cast<std::size_t>(
                          hardware - 1U)
                    : 1U;
        }

        mining_threads =
            std::clamp<std::size_t>(
                mining_threads,
                1U,
                64U
            );

        out =
            node_.
                mine_mempool_block_parallel_at(
                    payout_script,
                    unix_time_now(),
                    max_attempts,
                    mining_threads,
                    config_.
                        randomx_full_memory_mining,
                    wallet_mining_nonce_
                );

        if (out.ok()) {
            wallet_mining_nonce_ = 0U;
            (void)sync_wallet_locked();
        } else if (
            out.error ==
                NodeMineError::
                    proof_of_work_exhausted) {
            if (out.mining.nonce ==
                std::numeric_limits<
                    std::uint64_t>::max()) {
                wallet_mining_nonce_ = 0U;
            } else {
                wallet_mining_nonce_ =
                    out.mining.nonce + 1U;
            }
        }
    }

    if (out.ok()) {
        queue_announcement(
            kInventoryBlock,
            block_hash(out.block.header)
        );
    }

    return out;
}

NodeMineResult
NetworkRuntime::mine_mempool_block_at(
    const Bytes& payout_script,
    std::uint64_t adjusted_time,
    std::uint64_t max_attempts)
{
    NodeMineResult out;

    {
        std::scoped_lock lock(state_mutex_);
        out = node_.mine_mempool_block_at(
            payout_script,
            adjusted_time,
            max_attempts
        );

        if (out.ok()) {
            (void)sync_wallet_locked();
        }
    }

    if (out.ok()) {
        queue_announcement(
            kInventoryBlock,
            block_hash(out.block.header)
        );
    }

    return out;
}

NodeSubmitResult
NetworkRuntime::submit_block(
    const Block& block)
{
    NodeSubmitResult out;

    {
        std::scoped_lock lock(state_mutex_);
        out = node_.submit_block(block);

        if (out.ok()) {
            (void)sync_wallet_locked();
        }
    }

    if (out.ok()) {
        queue_announcement(
            kInventoryBlock,
            block_hash(block.header)
        );
    }

    return out;
}

std::optional<Block>
NetworkRuntime::block(
    const Hash256& hash) const
{
    std::scoped_lock lock(state_mutex_);

    const Block* value =
        node_.chain().block(hash);

    if (value == nullptr) {
        return std::nullopt;
    }

    return *value;
}

std::optional<Hash256>
NetworkRuntime::active_hash(
    std::uint32_t height) const
{
    std::scoped_lock lock(state_mutex_);
    return node_.chain().active_hash(height);
}

std::optional<std::uint32_t>
NetworkRuntime::active_height(
    const Hash256& hash) const
{
    std::scoped_lock lock(state_mutex_);
    return node_.chain().active_height(hash);
}

Hash256 NetworkRuntime::cumulative_work() const
{
    std::scoped_lock lock(state_mutex_);
    return node_.chain().cumulative_work();
}

std::vector<Hash256>
NetworkRuntime::mempool_transaction_ids(
    std::size_t limit) const
{
    std::scoped_lock lock(state_mutex_);
    return node_.mempool().transaction_ids(limit);
}

std::size_t NetworkRuntime::mempool_bytes() const
{
    std::scoped_lock lock(state_mutex_);
    return node_.mempool().total_bytes();
}

NetworkMiningTemplateResult
NetworkRuntime::mining_template(
    const Bytes& payout_script,
    std::uint64_t adjusted_time)
{
    NetworkMiningTemplateResult out;

    std::scoped_lock lock(state_mutex_);

    const std::size_t block_transaction_limit =
        params_.limits.max_block_transactions > 0U
            ? static_cast<std::size_t>(
                  params_.limits.max_block_transactions - 1U)
            : 0U;

    const auto transactions =
        node_.mempool().transactions(
            block_transaction_limit
        );

    std::size_t low{0U};
    std::size_t high{transactions.size()};
    std::size_t best{0U};

    while (low <= high) {
        const std::size_t mid =
            low + (high - low) / 2U;

        const auto candidate =
            mining::create_block_template(
                node_.chain(),
                payout_script,
                adjusted_time,
                std::span<const Transaction>(
                    transactions.data(),
                    mid
                )
            );

        if (candidate.ok()) {
            best = mid;
            low = mid + 1U;
            continue;
        }

        if (candidate.error ==
            mining::BlockTemplateError::
                resource_limits_exceeded) {
            if (mid == 0U) {
                break;
            }

            high = mid - 1U;
            continue;
        }

        out.block_template = candidate;
        return out;
    }

    out.block_template =
        mining::create_block_template(
            node_.chain(),
            payout_script,
            adjusted_time,
            std::span<const Transaction>(
                transactions.data(),
                best
            )
        );

    if (out.block_template.ok() &&
        params_.pow.pow_algorithm ==
            consensus::PowAlgorithm::randomx_v2) {
        out.randomx_seed =
            node_.chain().next_randomx_seed_key();
    }

    return out;
}

bool NetworkRuntime::has_mempool_transaction(
    const Hash256& txid) const
{
    std::scoped_lock lock(state_mutex_);
    return node_.mempool().contains(txid);
}

VersionMessage NetworkRuntime::local_version(
    std::uint64_t now) const
{
    std::uint32_t height{0U};

    {
        std::scoped_lock lock(state_mutex_);
        if (const auto current =
                node_.chain().height()) {
            height = *current;
        }
    }

    return VersionMessage{
        .protocol_version =
            params_.p2p_protocol_version,
        .services =
            kServiceNetwork |
            kServiceCompactBlocks |
            kServiceEncryptedTransport |
            kServiceAddrV2 |
            kServiceChainWork |
            (config_.enable_dandelion_relay
                 ? kServiceDandelionRelay
                 : 0U),
        .timestamp = now,
        .nonce = runtime_nonce_,
        .start_height = height,
        .listen_port =
            static_cast<std::uint16_t>(
                params_.p2p_protocol_version >=
                        kPeerAddressProtocolVersion
                    ? listen_port_.load()
                    : std::uint16_t{0U}
            ),
    };
}

void NetworkRuntime::run_nat_loop() noexcept
{
    constexpr std::uint64_t retry_seconds{300U};
    constexpr std::uint64_t renew_seconds{3'600U};

    std::uint64_t next_retry{0U};
    std::uint64_t next_renew{0U};

    try {
        while (!stop_requested_.load()) {
            const std::uint64_t now =
                unix_time_now();

            if (!nat_mapper_.active()) {
                if (now >= next_retry) {
                    const auto mapped =
                        nat_mapper_.map_tcp(
                            listen_port_.load()
                        );

                    if (mapped.ok()) {
                        nat_mapping_method_.store(
                            mapped.method
                        );
                        nat_external_port_.store(
                            mapped.external_port
                        );
                        next_renew =
                            now + renew_seconds;
                    } else {
                        nat_mapping_method_.store(
                            NatMappingMethod::none
                        );
                        nat_external_port_.store(0U);
                        next_retry =
                            now + retry_seconds;
                    }
                }
            } else if (
                nat_mapper_.method() ==
                    NatMappingMethod::nat_pmp &&
                now >= next_renew) {
                if (nat_mapper_.renew()) {
                    nat_external_port_.store(
                        nat_mapper_.external_port()
                    );
                    next_renew =
                        now + renew_seconds;
                } else {
                    nat_mapper_.unmap();
                    nat_mapping_method_.store(
                        NatMappingMethod::none
                    );
                    nat_external_port_.store(0U);
                    next_retry =
                        now + retry_seconds;
                }
            }

            std::this_thread::sleep_for(
                std::chrono::seconds(1)
            );
        }
    } catch (...) {
        // NAT traversal must not terminate the node.
    }

    nat_mapper_.unmap();
    nat_mapping_method_.store(
        NatMappingMethod::none
    );
    nat_external_port_.store(0U);
}

void NetworkRuntime::run_loop() noexcept
{
    try {
        while (!stop_requested_.load()) {
            const std::uint64_t now =
                unix_time_now();

#ifdef __ANDROID__
            if (android_p2p_diagnostic_.load(std::memory_order_relaxed) == 0) {
                android_p2p_diagnostic_.store(11, std::memory_order_relaxed);
            }
#endif
            // Outbound discovery must not wait behind inbound handshake polling.
            maintain_outbound(now);
#ifdef __ANDROID__
            android_p2p_diagnostic_.store(
                android_p2p_diagnostic_.load(std::memory_order_relaxed) == 11 ? 12 :
                android_p2p_diagnostic_.load(std::memory_order_relaxed),
                std::memory_order_relaxed);
#endif
            accept_inbound(now);
            flush_private_transactions(now);
            service_peers(now);
            service_stem_embargo(now);
            flush_announcements();
            prune_closed(now);

            std::this_thread::sleep_for(
                std::chrono::milliseconds(5)
            );
        }

        for (auto& peer : peers_) {
            peer.session.close();
        }

        peers_.clear();
        update_peer_counts();
    } catch (...) {
        for (auto& peer : peers_) {
            peer.session.close();
        }

        peers_.clear();
        update_peer_counts();
    }

    running_.store(false);
    stop_requested_.store(true);
}

void NetworkRuntime::accept_inbound(
    std::uint64_t now)
{
    if (peers_.size() >=
        config_.max_connections) {
        return;
    }

    auto accepted =
        listener_.accept_and_handshake(
            local_version(now),
            config_.accept_poll_ms,
            config_.io_timeout_ms
        );

    if (!accepted.ok()) {
        return;
    }

    const auto& remote_version =
        accepted.session->remote_version();

    if (accepted.observed_ipv4 &&
        remote_version.protocol_version >=
            kPeerAddressProtocolVersion &&
        remote_version.listen_port != 0U) {
        const bool allow_local =
            config_.allow_local_peers ||
            params_.network ==
                consensus::Network::regtest;

        const PeerAddress announced{
            .ipv4 = *accepted.observed_ipv4,
            .port = remote_version.listen_port,
            .services = remote_version.services,
            .last_seen = now,
        };

        if (valid_peer_address(
                announced,
                allow_local)) {
            (void)addrman_.add(
                announced
            );

            if (addrman_.save() !=
                AddrStoreError::none) {
                accepted.session->close();
                return;
            }

            known_address_count_.store(
                addrman_.size()
            );
        }
    }

    LivePeer peer{
        .session =
            std::move(*accepted.session),
        .address = std::nullopt,
        .last_activity = now,
        .reported_height = 0U,
    };

    peer.reported_height =
        peer.session.remote_version().start_height;

    if (!prepare_live_peer(
            peer,
            now,
            false)) {
        peer.session.close();
        return;
    }

    peers_.push_back(
        std::move(peer)
    );
    update_peer_counts();
}

void NetworkRuntime::maintain_outbound(
    std::uint64_t now)
{
    if (outbound_count_.load() >=
        config_.target_outbound ||
        peers_.size() >=
            config_.max_connections ||
        now < next_outbound_attempt_) {
        return;
    }

    for (auto it =
             reconnect_candidates_.begin();
         it != reconnect_candidates_.end();
         ++it) {
        if (it->next_attempt > now) {
            continue;
        }

        auto connected =
            connect_peer_address(
                params_,
                it->address,
                local_version(now),
                config_.io_timeout_ms,
                config_.proxies
            );

        if (!connected.ok()) {
#ifdef __ANDROID__
            android_p2p_diagnostic_.store(1000 + static_cast<int>(connected.error));
        android_last_connect_error_.store(1000 + static_cast<int>(connected.error));
            log_outbound_failure("reconnect", it->address, static_cast<int>(connected.error));
#endif
            addrman_.mark_failure(
                it->address,
                now
            );
            (void)addrman_.save();

            if (it->failures <
                std::numeric_limits<
                    std::uint32_t>::max()) {
                ++it->failures;
            }

            it->next_attempt =
                deadline_after(
                    now,
                    reconnect_backoff_delay(
                        config_.reconnect_delay_seconds,
                        it->failures
                    )
                );

            known_address_count_.store(
                addrman_.size()
            );

            next_outbound_attempt_ =
                now +
                config_.
                    outbound_retry_seconds;
            return;
        }

        addrman_.mark_success(
            it->address,
            now
        );
        (void)addrman_.save();

        LivePeer peer{
            .session =
                std::move(*connected.session),
            .address = it->address,
            .last_activity = now,
            .reported_height = 0U,
        };

        peer.reported_height =
            peer.session.remote_version().start_height;

        if (!prepare_live_peer(
                peer,
                now,
                true)) {
#ifdef __ANDROID__
            android_p2p_diagnostic_.store(3001);
        android_last_connect_error_.store(3001);
            log_outbound_failure("reconnect-post-handshake", it->address, -1);
#endif
            peer.session.close();
            addrman_.mark_failure(
                it->address,
                now
            );
            (void)addrman_.save();

            if (it->failures <
                std::numeric_limits<
                    std::uint32_t>::max()) {
                ++it->failures;
            }

            it->next_attempt =
                deadline_after(
                    now,
                    reconnect_backoff_delay(
                        config_.reconnect_delay_seconds,
                        it->failures
                    )
                );
            return;
        }

#ifdef __ANDROID__
        {
            const auto& remote = peer.session.remote_version();
            std::scoped_lock lock(android_peer_details_mutex_);
            android_peer_details_ = "Endpoint: " +
                (peer.address ? format_peer_host(*peer.address) + ":" +
                    std::to_string(peer.address->port) : std::string("unknown")) +
                "\\nP2P protocol: " + std::to_string(remote.protocol_version) +
                "\\nServices: " + std::to_string(remote.services) +
                "\\nAdvertised height: " + std::to_string(remote.start_height) +
                "\\nListening port: " + std::to_string(remote.listen_port) +
                "\\nHandshake: completed";
        }
        android_p2p_diagnostic_.store(4000);
#endif
        peers_.push_back(
            std::move(peer)
        );
        reconnect_candidates_.erase(it);
        known_address_count_.store(
            addrman_.size()
        );
        update_peer_counts();
        return;
    }

    std::vector<PeerAddress> excluded;
    excluded.reserve(peers_.size());

    for (const auto& peer : peers_) {
        if (peer.address) {
            excluded.push_back(
                *peer.address
            );
        }
    }

#ifdef __ANDROID__
    android_connect_attempts_.fetch_add(1U, std::memory_order_relaxed);
    android_p2p_diagnostic_.store(10);
    android_connect_started_ms_.store(static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count()));
#endif
#ifdef __ANDROID__
    const auto discovery_started = std::chrono::steady_clock::now();
    __android_log_print(ANDROID_LOG_INFO, "QUINTUM-P2P",
        "discovery-start known=%zu timeout_ms=%u",
        addrman_.size(), static_cast<unsigned>(config_.io_timeout_ms));
#endif
    auto connected =
        discovery_.connect_one(
            params_,
            local_version(now),
            now,
            config_.io_timeout_ms,
            excluded,
            config_.proxies
        );

#ifdef __ANDROID__
    const auto discovery_elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - discovery_started).count();
    __android_log_print(connected.ok() ? ANDROID_LOG_INFO : ANDROID_LOG_WARN,
        "QUINTUM-P2P", "discovery-finish elapsed_ms=%lld success=%d error=%d peer_error=%d",
        static_cast<long long>(discovery_elapsed_ms), connected.ok() ? 1 : 0,
        static_cast<int>(connected.error), static_cast<int>(connected.peer_error));
#endif
    known_address_count_.store(
        addrman_.size()
    );
#ifdef __ANDROID__
    android_connect_started_ms_.store(0U);
#endif

    if (!connected.ok()) {
#ifdef __ANDROID__
        android_p2p_diagnostic_.store(2000 + static_cast<int>(connected.error) * 100 + static_cast<int>(connected.peer_error));
        android_last_connect_error_.store(2000 + static_cast<int>(connected.error) * 100 + static_cast<int>(connected.peer_error));
        if (connected.address) {
            log_outbound_failure("discovery", *connected.address, static_cast<int>(connected.peer_error));
        } else {
            __android_log_print(ANDROID_LOG_WARN, "QUINTUM-P2P",
                "discovery no endpoint error=%d known=%zu",
                static_cast<int>(connected.error), addrman_.size());
        }
#endif
        next_outbound_attempt_ =
            now +
            config_.outbound_retry_seconds;
        return;
    }

    LivePeer peer{
        .session =
            std::move(*connected.session),
        .address = connected.address,
        .last_activity = now,
        .reported_height = 0U,
    };

    peer.reported_height =
        peer.session.remote_version().start_height;

    if (!prepare_live_peer(
            peer,
            now,
            true)) {
#ifdef __ANDROID__
        android_p2p_diagnostic_.store(3002);
        android_last_connect_error_.store(3002);
        if (connected.address) {
            log_outbound_failure("discovery-post-handshake", *connected.address, -1);
        }
#endif
        peer.session.close();

        if (connected.address) {
            addrman_.mark_failure(
                *connected.address,
                now
            );
            (void)addrman_.save();

            schedule_reconnect(
                *connected.address,
                now
            );
        }

        next_outbound_attempt_ =
            now +
            config_.outbound_retry_seconds;
        return;
    }

#ifdef __ANDROID__
    if (connected.address) {
        __android_log_print(ANDROID_LOG_INFO, "QUINTUM-P2P",
            "outbound-ready peer=%s:%u",
            format_peer_host(*connected.address).c_str(),
            static_cast<unsigned>(connected.address->port));
    }
#endif
#ifdef __ANDROID__
    {
        const auto& remote = peer.session.remote_version();
        std::scoped_lock lock(android_peer_details_mutex_);
        android_peer_details_ = "Endpoint: " +
            (peer.address ? format_peer_host(*peer.address) + ":" +
                std::to_string(peer.address->port) : std::string("unknown")) +
            "\\nP2P protocol: " + std::to_string(remote.protocol_version) +
            "\\nServices: " + std::to_string(remote.services) +
            "\\nAdvertised height: " + std::to_string(remote.start_height) +
            "\\nListening port: " + std::to_string(remote.listen_port) +
            "\\nHandshake: completed";
    }
    android_p2p_diagnostic_.store(4000);
#endif
    peers_.push_back(
        std::move(peer)
    );

    update_peer_counts();
}

bool NetworkRuntime::prepare_live_peer(
    LivePeer& peer,
    std::uint64_t now,
    bool outbound)
{
    if (!peer.session.valid()) {
        return false;
    }

    std::uint32_t local_height{0U};
    Hash256 local_work{};

    {
        std::scoped_lock lock(state_mutex_);

        if (const auto height =
                node_.chain().height()) {
            local_height = *height;
        }

        local_work =
            node_.chain().cumulative_work();
    }

    std::optional<Hash256> remote_work;

    // Stage 38 keeps P2P protocol v2 wire-compatible with existing nodes.
    // New nodes advertise a service bit and exchange cumulative chainwork
    // immediately after the version/verack handshake. Older peers simply
    // omit the capability and retain the previous height-based fallback.
    if ((peer.session.remote_version().services &
            kServiceChainWork) != 0U) {
        const auto local_payload =
            serialize_chain_work(local_work);

        if (outbound) {
            if (peer.session.send_command(
                    "chainwork",
                    local_payload) !=
                PeerError::none) {
                return false;
            }

            WireMessage response;
            if (peer.session.receive_command(
                    response) !=
                    PeerError::none ||
                response.command != "chainwork") {
                return false;
            }

            remote_work =
                parse_chain_work(
                    response.payload
                );
        } else {
            WireMessage request;
            if (peer.session.receive_command(
                    request) !=
                    PeerError::none ||
                request.command != "chainwork") {
                return false;
            }

            remote_work =
                parse_chain_work(
                    request.payload
                );

            if (!remote_work ||
                peer.session.send_command(
                    "chainwork",
                    local_payload) !=
                    PeerError::none) {
                return false;
            }
        }

        if (!remote_work) {
            return false;
        }
    }

    // Exactly one side drives setup. Capable peers compare cumulative work,
    // which is the PoW fork-choice rule. Legacy peers fall back to height.
    // Equal work is broken by TCP direction so both sides never issue
    // synchronous request/response flows at the same time.
    const bool active_setup =
        sync_driver_should_run(
            local_work,
            remote_work,
            local_height,
            peer.reported_height,
            outbound
        );

    if (!active_setup) {
        peer.last_activity = now;
        return true;
    }

    std::optional<Hash256> synchronized_tip;

    {
        std::scoped_lock lock(state_mutex_);

        const auto synced =
            sync_from_peer(
                peer.session,
                node_,
                now
            );

        if (!synced.ok()) {
            return false;
        }

        if (synced.blocks_accepted > 0U ||
            synced.reorganized) {
            synchronized_tip =
                node_.chain().tip_hash();
        }

        if (!sync_wallet_locked()) {
            return false;
        }
    }

    if (synchronized_tip) {
        queue_announcement(
            kInventoryBlock,
            *synchronized_tip
        );
    }

    const auto learned =
        discovery_.learn_from_peer(
            peer.session,
            config_.allow_local_peers ||
                params_.network ==
                    consensus::Network::regtest
        );

    if (!learned.ok()) {
        return false;
    }

    known_address_count_.store(
        addrman_.size()
    );

    {
        std::scoped_lock lock(state_mutex_);

        const auto mempool_sync =
            sync_mempool_from_peer(
                peer.session,
                node_
            );

        if (!mempool_sync.ok()) {
            return false;
        }

        if (!sync_wallet_locked()) {
            return false;
        }
    }

    std::optional<Hash256> tip;

    {
        std::scoped_lock lock(state_mutex_);
        tip = node_.chain().tip_hash();
    }

    if (tip &&
        announce_block(
            peer.session,
            *tip) != PeerError::none) {
        return false;
    }

    peer.last_activity = now;
    return true;
}

void NetworkRuntime::service_peers(
    std::uint64_t now)
{
    for (auto& peer : peers_) {
        if (!peer.session.valid()) {
            continue;
        }

        {
            std::scoped_lock lock(state_mutex_);

            peer.requested_transactions.erase(
                std::remove_if(
                    peer.requested_transactions.begin(),
                    peer.requested_transactions.end(),
                    [&](const PendingTransactionRequest& request) {
                        return node_.mempool().contains(
                            request.hash
                        );
                    }
                ),
                peer.requested_transactions.end()
            );

            peer.requested_blocks.erase(
                std::remove_if(
                    peer.requested_blocks.begin(),
                    peer.requested_blocks.end(),
                    [&](const PendingBlockRequest& request) {
                        return node_.chain().has_block(
                            request.hash
                        );
                    }
                ),
                peer.requested_blocks.end()
            );
        }

        if (!drain_transaction_requests(
                peer,
                now)) {
            peer.session.close();
            continue;
        }

        if (config_.transaction_request_timeout_seconds > 0U) {
            const bool stalled_transaction_request =
                std::any_of(
                    peer.requested_transactions.begin(),
                    peer.requested_transactions.end(),
                    [&](const PendingTransactionRequest& request) {
                        return elapsed(
                            now,
                            request.requested_at,
                            config_.
                                transaction_request_timeout_seconds
                        );
                    }
                );

            if (stalled_transaction_request) {
                peer.session.close();
                continue;
            }
        }

        if (config_.block_request_timeout_seconds > 0U) {
            const bool stalled_block_request =
                std::any_of(
                    peer.requested_blocks.begin(),
                    peer.requested_blocks.end(),
                    [&](const PendingBlockRequest& request) {
                        return elapsed(
                            now,
                            request.requested_at,
                            config_.
                                block_request_timeout_seconds
                        );
                    }
                );

            if (stalled_block_request) {
                peer.session.close();
                continue;
            }
        }

        if (peer.pending_ping) {
            if (elapsed(
                    now,
                    peer.ping_sent_at,
                    config_.
                        ping_timeout_seconds)) {
                peer.session.close();
                continue;
            }
        } else if (elapsed(
                       now,
                       peer.last_activity,
                       config_.
                           ping_interval_seconds)) {
            ++ping_counter_;

            const std::uint64_t nonce =
                runtime_nonce_ ^
                ping_counter_ ^
                now;

            const auto payload =
                serialize_nonce(nonce);

            if (peer.session.send_command(
                    "ping",
                    payload) !=
                PeerError::none) {
                peer.session.close();
                continue;
            }

            peer.pending_ping = nonce;
            peer.ping_sent_at = now;
        }

        if (!peer.session.wait_readable(0U)) {
            continue;
        }

        WireMessage message;
        const auto receive =
            peer.session.receive_command(
                message
            );

        if (receive != PeerError::none) {
            peer.session.close();
            continue;
        }

        peer.last_activity = now;

        if (peer.message_window_started == 0U ||
            elapsed(
                now,
                peer.message_window_started,
                1U)) {
            peer.message_window_started = now;
            peer.messages_in_window = 0U;
            peer.stem_transactions_in_window = 0U;
        }

        if (peer.messages_in_window <
            std::numeric_limits<std::uint32_t>::max()) {
            ++peer.messages_in_window;
        }

        if (config_.max_messages_per_second > 0U &&
            peer.messages_in_window >
                config_.max_messages_per_second) {
            peer.session.close();
            continue;
        }

        if (message.command == "stemtx") {
            if (peer.stem_transactions_in_window <
                std::numeric_limits<std::uint32_t>::max()) {
                ++peer.stem_transactions_in_window;
            }

            if (config_.
                    max_stem_transactions_per_second > 0U &&
                peer.stem_transactions_in_window >
                    config_.
                        max_stem_transactions_per_second) {
                peer.session.close();
                continue;
            }
        }

        if (!process_message(
                peer,
                message,
                now)) {
            peer.session.close();
        }
    }
}

bool NetworkRuntime::process_message(
    LivePeer& peer,
    const WireMessage& message,
    std::uint64_t now)
{
    if (message.command == "ping") {
        const auto nonce =
            parse_nonce(message.payload);

        if (!nonce) {
            return false;
        }

        const auto payload =
            serialize_nonce(*nonce);

        return peer.session.send_command(
                   "pong",
                   payload) ==
               PeerError::none;
    }

    if (message.command == "pong") {
        const auto nonce =
            parse_nonce(message.payload);

        if (!nonce ||
            !peer.pending_ping ||
            *nonce != *peer.pending_ping) {
            return false;
        }

        peer.pending_ping.reset();
        return true;
    }

    if (message.command == "getaddr" ||
        message.command == "getaddrv2") {
        if (!message.payload.empty()) {
            return false;
        }

        const bool use_v2 =
            message.command == "getaddrv2";

        if (use_v2 &&
            (peer.session.remote_version().services &
                kServiceAddrV2) == 0U) {
            return false;
        }

        const auto addresses =
            addrman_.addresses();

        const auto payload = use_v2
            ? serialize_addresses_v2(addresses)
            : serialize_addresses(addresses);

        return peer.session.send_command(
                   use_v2 ? "addrv2" : "addr",
                   payload) ==
               PeerError::none;
    }

    if (message.command == "addr" ||
        message.command == "addrv2") {
        const bool use_v2 =
            message.command == "addrv2";

        if (use_v2 &&
            (peer.session.remote_version().services &
                kServiceAddrV2) == 0U) {
            return false;
        }

        const bool allow_local =
            config_.allow_local_peers ||
            params_.network ==
                consensus::Network::regtest;

        const auto parsed = use_v2
            ? parse_addresses_v2(
                  message.payload,
                  allow_local)
            : parse_addresses(
                  message.payload,
                  allow_local);

        if (!parsed) {
            return false;
        }

        (void)addrman_.add(*parsed);

        if (addrman_.save() !=
            AddrStoreError::none) {
            return false;
        }

        known_address_count_.store(
            addrman_.size()
        );
        return true;
    }

    if (message.command == "getheaders") {
        std::scoped_lock lock(state_mutex_);

        return serve_sync_message(
                   peer.session,
                   node_.chain(),
                   message
               ).ok();
    }

    if (message.command == "getdata" ||
        message.command == "mempool") {
        const auto hidden =
            hidden_transaction_ids();

        std::scoped_lock lock(state_mutex_);

        return serve_relay_message(
                   peer.session,
                   node_,
                   message,
                   hidden
               ).ok();
    }

    if (message.command == "getblocktxn") {
        return process_get_block_transactions(
            peer,
            message
        );
    }

    if (message.command == "inv") {
        return process_inventory(
            peer,
            message,
            now
        );
    }

    if (message.command == "stemtx") {
        return process_stem_transaction(
            peer,
            message,
            now
        );
    }

    if (message.command == "tx") {
        return process_transaction(
            peer,
            message
        );
    }

    if (message.command == "block") {
        return process_block(
            peer,
            message,
            now
        );
    }

    if (message.command == "cmpctblock") {
        return process_compact_block(
            peer,
            message,
            now
        );
    }

    if (message.command == "blocktxn") {
        return process_block_transactions(
            peer,
            message,
            now
        );
    }

    if (message.command == "notfound") {
        const auto inventory =
            parse_inventory(
                message.payload
            );

        if (!inventory) {
            return false;
        }

        for (const auto& item : *inventory) {
            if (item.type ==
                kInventoryTransaction) {
                peer.requested_transactions.erase(
                    std::remove_if(
                        peer.requested_transactions.begin(),
                        peer.requested_transactions.end(),
                        [&](const PendingTransactionRequest& request) {
                            return request.hash == item.hash;
                        }
                    ),
                    peer.requested_transactions.end()
                );
            } else if (item.type ==
                           kInventoryBlock ||
                       item.type ==
                           kInventoryCompactBlock) {
                peer.requested_blocks.erase(
                    std::remove_if(
                        peer.requested_blocks.begin(),
                        peer.requested_blocks.end(),
                        [&](const PendingBlockRequest& request) {
                            return request.hash == item.hash;
                        }
                    ),
                    peer.requested_blocks.end()
                );

                peer.pending_compact_blocks.erase(
                    std::remove_if(
                        peer.pending_compact_blocks.begin(),
                        peer.pending_compact_blocks.end(),
                        [&](const PendingCompactBlock& pending) {
                            return pending.hash == item.hash;
                        }
                    ),
                    peer.pending_compact_blocks.end()
                );
            }
        }

        return true;
    }

    // Forward-compatible behavior: unknown commands are ignored.
    return true;
}

bool NetworkRuntime::process_inventory(
    LivePeer& peer,
    const WireMessage& message,
    std::uint64_t now)
{
    const auto inventory =
        parse_inventory(
            message.payload
        );

    if (!inventory ||
        inventory->size() >
            kMaxRelayInventoryItems) {
        return false;
    }

    std::vector<InventoryItem> wanted;
    wanted.reserve(inventory->size());

    {
        std::scoped_lock lock(state_mutex_);

        for (const auto& item : *inventory) {
            if (item.type ==
                kInventoryTransaction) {
                if (node_.mempool().contains(
                        item.hash)) {
                    // Once the transaction is observed in normal diffusion,
                    // stop hiding any local stem copy immediately.
                    promote_private_transaction(
                        item.hash
                    );
                    erase_hash(
                        peer.deferred_transactions,
                        item.hash
                    );
                    continue;
                }

                const bool already_requested =
                    std::any_of(
                        peer.requested_transactions.begin(),
                        peer.requested_transactions.end(),
                        [&](const PendingTransactionRequest& request) {
                            return request.hash == item.hash;
                        }
                    );

                if (already_requested ||
                    contains_hash(
                        peer.deferred_transactions,
                        item.hash)) {
                    continue;
                }

                const std::size_t capacity =
                    std::max<std::size_t>(
                        1U,
                        config_.
                            max_transaction_requests_in_flight
                    );

                if (peer.requested_transactions.size() >=
                    capacity) {
                    if (peer.deferred_transactions.size() >=
                        config_.
                            max_deferred_transaction_requests) {
                        return false;
                    }

                    peer.deferred_transactions.push_back(
                        item.hash
                    );
                    continue;
                }

                peer.requested_transactions.push_back(
                    PendingTransactionRequest{
                        .hash = item.hash,
                        .requested_at = now,
                    }
                );
                wanted.push_back(item);
                continue;
            }

            if (item.type ==
                kInventoryBlock) {
                if (const auto active_height =
                        node_.chain().active_height(
                            item.hash)) {
                    const auto local_tip =
                        node_.chain().tip_hash();

                    // An announcement of our exact active tip is a direct
                    // observation that the peer has converged to this tip.
                    // Unlike ordinary older-block inventory, this may
                    // legitimately lower the peer height after a
                    // shorter-but-heavier reorg.
                    if (local_tip &&
                        *local_tip == item.hash) {
                        peer.reported_height =
                            *active_height;
                    } else {
                        peer.reported_height =
                            std::max(
                                peer.reported_height,
                                *active_height
                            );
                    }
                    continue;
                }

                const bool already_requested =
                    std::any_of(
                        peer.requested_blocks.begin(),
                        peer.requested_blocks.end(),
                        [&](const PendingBlockRequest& request) {
                            return request.hash == item.hash;
                        }
                    );

                if (node_.chain().has_block(
                        item.hash) ||
                    already_requested) {
                    continue;
                }

                if (peer.requested_blocks.size() >=
                    config_.max_block_requests_in_flight) {
                    continue;
                }

                peer.requested_blocks.push_back(
                    PendingBlockRequest{
                        .hash = item.hash,
                        .requested_at = now,
                    }
                );

                InventoryItem requested = item;

                if ((peer.session.remote_version().services &
                     kServiceCompactBlocks) != 0U) {
                    requested.type =
                        kInventoryCompactBlock;
                }

                wanted.push_back(requested);
            }
        }
    }

    update_peer_counts();

    if (wanted.empty()) {
        return true;
    }

    const auto payload =
        serialize_inventory(wanted);

    return peer.session.send_command(
               "getdata",
               payload) ==
           PeerError::none;
}

bool NetworkRuntime::drain_transaction_requests(
    LivePeer& peer,
    std::uint64_t now)
{
    if (peer.deferred_transactions.empty()) {
        return true;
    }

    const std::size_t capacity =
        std::max<std::size_t>(
            1U,
            config_.
                max_transaction_requests_in_flight
        );

    if (peer.requested_transactions.size() >=
        capacity) {
        return true;
    }

    const std::size_t available =
        capacity -
        peer.requested_transactions.size();

    const std::size_t count =
        std::min(
            available,
            peer.deferred_transactions.size()
        );

    std::vector<InventoryItem> wanted;
    wanted.reserve(count);

    for (std::size_t i = 0U;
         i < count;
         ++i) {
        const Hash256 hash =
            peer.deferred_transactions[i];

        peer.requested_transactions.push_back(
            PendingTransactionRequest{
                .hash = hash,
                .requested_at = now,
            }
        );

        wanted.push_back(
            InventoryItem{
                .type = kInventoryTransaction,
                .hash = hash,
            }
        );
    }

    peer.deferred_transactions.erase(
        peer.deferred_transactions.begin(),
        peer.deferred_transactions.begin() +
            static_cast<std::ptrdiff_t>(count)
    );

    return peer.session.send_command(
               "getdata",
               serialize_inventory(wanted)) ==
           PeerError::none;
}

bool NetworkRuntime::process_transaction(
    LivePeer& peer,
    const WireMessage& message)
{
    const auto transaction =
        parse_transaction_payload(
            message.payload,
            params_.limits
        );

    if (!transaction) {
        return false;
    }

    const Hash256 txid =
        transaction_id(*transaction);

    const bool requested =
        std::any_of(
            peer.requested_transactions.begin(),
            peer.requested_transactions.end(),
            [&](const PendingTransactionRequest& request) {
                return request.hash == txid;
            }
        );

    if (!requested) {
        return false;
    }

    peer.requested_transactions.erase(
        std::remove_if(
            peer.requested_transactions.begin(),
            peer.requested_transactions.end(),
            [&](const PendingTransactionRequest& request) {
                return request.hash == txid;
            }
        ),
        peer.requested_transactions.end()
    );

    NodeTransactionResult submitted;

    {
        std::scoped_lock lock(state_mutex_);
        submitted =
            node_.submit_transaction(
                *transaction
            );

        if (submitted.ok() &&
            !sync_wallet_locked()) {
            return false;
        }
    }

    if (!submitted.ok()) {
        return submitted.mempool.error ==
               MempoolError::duplicate;
    }

    queue_announcement(
        kInventoryTransaction,
        txid
    );

    return true;
}

bool NetworkRuntime::process_stem_transaction(
    LivePeer& peer,
    const WireMessage& message,
    std::uint64_t now)
{
    if (!config_.enable_dandelion_relay ||
        !peer.session.encrypted() ||
        (peer.session.remote_version().services &
         kServiceDandelionRelay) == 0U) {
        return false;
    }

    const auto transaction =
        parse_transaction_payload(
            message.payload,
            params_.limits
        );

    if (!transaction) {
        return false;
    }

    const Hash256 txid =
        transaction_id(*transaction);

    NodeTransactionResult submitted;

    {
        std::scoped_lock lock(state_mutex_);
        submitted =
            node_.submit_transaction(
                *transaction
            );

        if (submitted.ok() &&
            !sync_wallet_locked()) {
            return false;
        }
    }

    if (!submitted.ok()) {
        if (submitted.mempool.error !=
            MempoolError::duplicate) {
            return false;
        }

        // A stem loop or duplicate means the transaction has lost
        // its one-way stem property. Diffuse it normally from now on.
        promote_private_transaction(txid);
        return true;
    }

    const auto random =
        secure_dandelion_random();

    if (!random ||
        dandelion_should_fluff(
            *random,
            config_.dandelion_fluff_percent)) {
        queue_announcement(
            kInventoryTransaction,
            txid
        );
        return true;
    }

    if (!relay_stem_transaction(
            txid,
            &peer,
            now)) {
        queue_announcement(
            kInventoryTransaction,
            txid
        );
    }

    return true;
}

bool NetworkRuntime::process_block(
    LivePeer& peer,
    const WireMessage& message,
    std::uint64_t now)
{
    const auto block =
        parse_block_payload(
            message.payload,
            params_.limits
        );

    if (!block) {
        return false;
    }

    const Hash256 hash =
        block_hash(block->header);

    return process_received_block(
        peer,
        *block,
        hash,
        now
    );
}

bool NetworkRuntime::process_compact_block(
    LivePeer& peer,
    const WireMessage& message,
    std::uint64_t now)
{
    if ((peer.session.remote_version().services &
         kServiceCompactBlocks) == 0U) {
        return false;
    }

    const auto compact =
        parse_compact_block(
            message.payload,
            params_.limits
        );

    if (!compact) {
        return false;
    }

    const Hash256 hash =
        block_hash(compact->header);

    const bool requested =
        std::any_of(
            peer.requested_blocks.begin(),
            peer.requested_blocks.end(),
            [&](const PendingBlockRequest& request) {
                return request.hash == hash;
            }
        );

    if (!requested) {
        return false;
    }

    CompactReconstructionResult rebuilt;

    {
        std::scoped_lock lock(state_mutex_);
        rebuilt =
            reconstruct_compact_block(
                *compact,
                node_.mempool()
            );
    }

    if (!rebuilt.ok()) {
        return false;
    }

    if (rebuilt.complete()) {
        return process_received_block(
            peer,
            *rebuilt.block,
            hash,
            now
        );
    }

    if (rebuilt.missing_indexes.empty() ||
        peer.pending_compact_blocks.size() >=
            config_.max_pending_compact_blocks) {
        return false;
    }

    const bool already_pending =
        std::any_of(
            peer.pending_compact_blocks.begin(),
            peer.pending_compact_blocks.end(),
            [&](const PendingCompactBlock& pending) {
                return pending.hash == hash;
            }
        );

    if (already_pending) {
        return false;
    }

    BlockTransactionsRequest request{
        .block_hash = hash,
        .indexes =
            rebuilt.missing_indexes,
    };

    peer.pending_compact_blocks.push_back(
        PendingCompactBlock{
            .hash = hash,
            .compact = *compact,
            .missing_indexes =
                rebuilt.missing_indexes,
        }
    );

    const auto payload =
        serialize_getblocktxn(request);

    if (peer.session.send_command(
            "getblocktxn",
            payload) !=
        PeerError::none) {
        peer.pending_compact_blocks.pop_back();
        return false;
    }

    const auto request_it =
        std::find_if(
            peer.requested_blocks.begin(),
            peer.requested_blocks.end(),
            [&](const PendingBlockRequest& pending) {
                return pending.hash == hash;
            }
        );

    if (request_it !=
        peer.requested_blocks.end()) {
        // A compact block was delivered, so the peer made real progress.
        // Start a fresh deadline for the missing-transaction round trip
        // instead of charging it against the original block request.
        request_it->requested_at = now;
    }

    return true;
}

bool NetworkRuntime::process_get_block_transactions(
    LivePeer& peer,
    const WireMessage& message)
{
    if ((peer.session.remote_version().services &
         kServiceCompactBlocks) == 0U) {
        return false;
    }

    const auto request =
        parse_getblocktxn(
            message.payload,
            params_.limits.
                max_block_transactions
        );

    if (!request ||
        std::find(
            request->indexes.begin(),
            request->indexes.end(),
            0U
        ) != request->indexes.end()) {
        return false;
    }

    BlockTransactions response;
    response.block_hash =
        request->block_hash;

    bool found{false};

    {
        std::scoped_lock lock(state_mutex_);

        const Block* block =
            node_.chain().block(
                request->block_hash
            );

        if (block != nullptr) {
            found = true;
            response.transactions.reserve(
                request->indexes.size()
            );

            for (const auto index :
                 request->indexes) {
                if (index >=
                    block->transactions.size()) {
                    return false;
                }

                response.transactions.emplace_back(
                    index,
                    block->transactions[index]
                );
            }
        }
    }

    if (!found) {
        const std::array<InventoryItem, 1>
            missing{
                InventoryItem{
                    .type =
                        kInventoryCompactBlock,
                    .hash =
                        request->block_hash,
                }
            };

        return peer.session.send_command(
                   "notfound",
                   serialize_inventory(missing)
               ) == PeerError::none;
    }

    return peer.session.send_command(
               "blocktxn",
               serialize_blocktxn(response)
           ) == PeerError::none;
}

bool NetworkRuntime::process_block_transactions(
    LivePeer& peer,
    const WireMessage& message,
    std::uint64_t now)
{
    if ((peer.session.remote_version().services &
         kServiceCompactBlocks) == 0U) {
        return false;
    }

    const auto response =
        parse_blocktxn(
            message.payload,
            params_.limits
        );

    if (!response) {
        return false;
    }

    const auto pending_it =
        std::find_if(
            peer.pending_compact_blocks.begin(),
            peer.pending_compact_blocks.end(),
            [&](const PendingCompactBlock& pending) {
                return pending.hash ==
                    response->block_hash;
            }
        );

    if (pending_it ==
        peer.pending_compact_blocks.end()) {
        return false;
    }

    if (response->transactions.size() !=
        pending_it->missing_indexes.size()) {
        return false;
    }

    for (std::size_t i = 0U;
         i < response->transactions.size();
         ++i) {
        if (response->transactions[i].first !=
            pending_it->missing_indexes[i]) {
            return false;
        }
    }

    CompactReconstructionResult rebuilt;

    {
        std::scoped_lock lock(state_mutex_);
        rebuilt =
            complete_compact_block(
                pending_it->compact,
                node_.mempool(),
                *response
            );
    }

    if (!rebuilt.complete()) {
        return false;
    }

    const Hash256 hash =
        pending_it->hash;

    peer.pending_compact_blocks.erase(
        pending_it
    );

    return process_received_block(
        peer,
        *rebuilt.block,
        hash,
        now
    );
}

bool NetworkRuntime::process_received_block(
    LivePeer& peer,
    const Block& block,
    const Hash256& hash,
    std::uint64_t now)
{
    const bool requested =
        std::any_of(
            peer.requested_blocks.begin(),
            peer.requested_blocks.end(),
            [&](const PendingBlockRequest& request) {
                return request.hash == hash;
            }
        );

    if (!requested) {
        return false;
    }

    peer.requested_blocks.erase(
        std::remove_if(
            peer.requested_blocks.begin(),
            peer.requested_blocks.end(),
            [&](const PendingBlockRequest& request) {
                return request.hash == hash;
            }
        ),
        peer.requested_blocks.end()
    );

    peer.pending_compact_blocks.erase(
        std::remove_if(
            peer.pending_compact_blocks.begin(),
            peer.pending_compact_blocks.end(),
            [&](const PendingCompactBlock& pending) {
                return pending.hash == hash;
            }
        ),
        peer.pending_compact_blocks.end()
    );

    // Cache the digest of the complete canonical block, never an inventory
    // hash announced by a peer. This prevents untrusted announcements from
    // poisoning validation of a different object.
    const Hash256 object_digest =
        crypto::double_sha256(
            serialize_block_payload(block)
        );

    if (invalid_object_cached(
            object_digest,
            now)) {
        return false;
    }

    NodeSubmitResult submitted;
    std::optional<std::uint32_t>
        accepted_height;

    {
        std::scoped_lock lock(state_mutex_);
        submitted =
            node_.submit_block_at(
                block,
                now
            );

        if (!submitted.ok() &&
            submitted.connect.chain.error ==
                ChainConnectError::
                    unknown_parent) {
            const auto synced =
                sync_from_peer(
                    peer.session,
                    node_,
                    now
                );

            if (!synced.ok()) {
                return false;
            }

            if (!sync_wallet_locked()) {
                return false;
            }

            const auto synchronized_tip =
                node_.chain().tip_hash();

            if (const auto synchronized_height =
                    node_.chain().height()) {
                peer.reported_height =
                    std::max(
                        peer.reported_height,
                        *synchronized_height
                    );
            }

            if (synchronized_tip) {
                queue_announcement(
                    kInventoryBlock,
                    *synchronized_tip
                );
            }

            return true;
        }

        if (submitted.ok()) {
            if (!sync_wallet_locked()) {
                return false;
            }

            accepted_height =
                node_.chain().height();
        }
    }

    if (!submitted.ok()) {
        const auto error =
            submitted.connect.chain.error;

        if (error ==
            ChainConnectError::duplicate_block) {
            return true;
        }

        // Only context-independent failures are safe to remember. Errors
        // involving ancestry, time, transactions, storage or reorg state may
        // become valid/retryable as local chain state changes.
        switch (error) {
        case ChainConnectError::invalid_block_structure:
        case ChainConnectError::wrong_genesis:
        case ChainConnectError::invalid_proof_of_work:
        case ChainConnectError::resource_limits_exceeded:
        case ChainConnectError::height_overflow:
        case ChainConnectError::fee_sum_overflow:
        case ChainConnectError::invalid_coinbase_reward:
            if (submitted.error ==
                NodeSubmitError::chain_rejected) {
                cache_invalid_object(
                    object_digest,
                    now
                );
            }
            break;
        default:
            break;
        }

        return false;
    }

    if (accepted_height) {
        peer.reported_height =
            std::max(
                peer.reported_height,
                *accepted_height
            );
    }

    update_peer_counts();

    queue_announcement(
        kInventoryBlock,
        hash
    );

    return true;
}

void NetworkRuntime::flush_private_transactions(
    std::uint64_t now)
{
    std::vector<Hash256> pending;

    {
        std::scoped_lock lock(
            announcement_mutex_
        );
        pending.swap(
            pending_private_transactions_
        );
    }

    for (const auto& txid : pending) {
        if (!config_.enable_dandelion_relay ||
            !relay_stem_transaction(
                txid,
                nullptr,
                now)) {
            queue_announcement(
                kInventoryTransaction,
                txid
            );
        }
    }
}

void NetworkRuntime::service_stem_embargo(
    std::uint64_t now)
{
    std::vector<Hash256> expired;
    std::vector<StemRelayState> surviving;
    surviving.reserve(stem_relays_.size());

    {
        std::scoped_lock lock(state_mutex_);

        for (const auto& relay :
             stem_relays_) {
            if (!node_.mempool().contains(
                    relay.txid)) {
                continue;
            }

            if (relay.embargo_deadline <=
                now) {
                expired.push_back(
                    relay.txid
                );
                continue;
            }

            surviving.push_back(relay);
        }
    }

    stem_relays_ =
        std::move(surviving);

    for (const auto& txid : expired) {
        queue_announcement(
            kInventoryTransaction,
            txid
        );
    }
}

bool NetworkRuntime::relay_stem_transaction(
    const Hash256& txid,
    LivePeer* source,
    std::uint64_t now)
{
    std::vector<std::size_t> candidates;
    candidates.reserve(peers_.size());

    for (std::size_t i = 0U;
         i < peers_.size();
         ++i) {
        auto& peer = peers_[i];

        if (!peer.session.valid() ||
            !peer.session.encrypted() ||
            !peer.address ||
            &peer == source ||
            (peer.session.remote_version().services &
             kServiceDandelionRelay) == 0U) {
            continue;
        }

        candidates.push_back(i);
    }

    if (candidates.empty()) {
        return false;
    }

    std::optional<std::size_t> selected;

    if (source == nullptr &&
        origin_stem_route_ &&
        now < origin_stem_epoch_deadline_) {
        for (const auto candidate :
             candidates) {
            const auto& address =
                peers_[candidate].address;

            if (address &&
                same_endpoint(
                    *address,
                    *origin_stem_route_)) {
                selected = candidate;
                break;
            }
        }
    }

    if (!selected) {
        const auto route_random =
            secure_dandelion_random();

        if (!route_random) {
            return false;
        }

        selected =
            candidates[
                static_cast<std::size_t>(
                    *route_random %
                    static_cast<std::uint64_t>(
                        candidates.size()
                    )
                )
            ];

        if (source == nullptr) {
            origin_stem_route_ =
                peers_[*selected].address;

            const std::uint64_t epoch =
                config_.dandelion_epoch_seconds;

            origin_stem_epoch_deadline_ =
                now >
                    std::numeric_limits<
                        std::uint64_t>::max() -
                        epoch
                    ? std::numeric_limits<
                          std::uint64_t>::max()
                    : now + epoch;
        }
    }

    // A private relay without fresh randomness for its embargo would
    // become distinguishable and predictable. Fail closed to normal
    // diffusion before transmitting the stem.
    const auto delay_random =
        secure_dandelion_random();

    if (!delay_random) {
        return false;
    }

    const std::uint64_t delay =
        dandelion_embargo_delay(
            *delay_random,
            config_.
                dandelion_embargo_min_seconds,
            config_.
                dandelion_embargo_jitter_seconds
        );

    const std::uint64_t deadline =
        now >
            std::numeric_limits<
                std::uint64_t>::max() -
                delay
            ? std::numeric_limits<
                  std::uint64_t>::max()
            : now + delay;

    std::optional<Transaction> transaction;

    {
        std::scoped_lock lock(state_mutex_);

        const Transaction* value =
            node_.mempool().transaction(
                txid
            );

        if (value == nullptr) {
            // The transaction may already have been mined. Nothing to relay.
            return true;
        }

        transaction = *value;
    }

    const auto payload =
        serialize_transaction_payload(
            *transaction
        );

    auto& destination =
        peers_[*selected];

    if (destination.session.send_command(
            "stemtx",
            payload) !=
        PeerError::none) {
        destination.session.close();
        return false;
    }

    const auto existing =
        std::find_if(
            stem_relays_.begin(),
            stem_relays_.end(),
            [&](const StemRelayState& relay) {
                return relay.txid == txid;
            }
        );

    if (existing ==
        stem_relays_.end()) {
        stem_relays_.push_back(
            StemRelayState{
                .txid = txid,
                .embargo_deadline = deadline,
            }
        );
    } else {
        existing->embargo_deadline =
            deadline;
    }

    return true;
}

void NetworkRuntime::erase_stem_relay(
    const Hash256& txid)
{
    stem_relays_.erase(
        std::remove_if(
            stem_relays_.begin(),
            stem_relays_.end(),
            [&](const StemRelayState& relay) {
                return relay.txid == txid;
            }
        ),
        stem_relays_.end()
    );
}

void NetworkRuntime::promote_private_transaction(
    const Hash256& txid)
{
    erase_stem_relay(txid);

    std::scoped_lock lock(
        announcement_mutex_
    );

    erase_hash(
        pending_private_transactions_,
        txid
    );

    const bool already_queued =
        std::any_of(
            announcements_.begin(),
            announcements_.end(),
            [&](const PendingAnnouncement& item) {
                return item.type ==
                           kInventoryTransaction &&
                       item.hash == txid;
            }
        );

    if (!already_queued) {
        announcements_.push_back(
            PendingAnnouncement{
                .type = kInventoryTransaction,
                .hash = txid,
            }
        );
    }
}

std::vector<Hash256>
NetworkRuntime::hidden_transaction_ids()
{
    std::vector<Hash256> hidden;
    hidden.reserve(
        stem_relays_.size()
    );

    for (const auto& relay :
         stem_relays_) {
        hidden.push_back(relay.txid);
    }

    {
        std::scoped_lock lock(
            announcement_mutex_
        );

        hidden.reserve(
            hidden.size() +
            pending_private_transactions_.size()
        );

        for (const auto& txid :
             pending_private_transactions_) {
            if (!contains_hash(
                    hidden,
                    txid)) {
                hidden.push_back(txid);
            }
        }
    }

    return hidden;
}

void NetworkRuntime::queue_private_transaction(
    const Hash256& txid)
{
    if (!config_.enable_dandelion_relay) {
        queue_announcement(
            kInventoryTransaction,
            txid
        );
        return;
    }

    std::scoped_lock lock(
        announcement_mutex_
    );

    if (!contains_hash(
            pending_private_transactions_,
            txid)) {
        pending_private_transactions_.
            push_back(txid);
    }
}

void NetworkRuntime::flush_announcements()
{
    std::vector<PendingAnnouncement>
        pending;

    {
        std::scoped_lock lock(
            announcement_mutex_
        );
        pending.swap(announcements_);
    }

    if (pending.empty()) {
        return;
    }

    for (const auto& announcement :
         pending) {
        const std::array<InventoryItem, 1>
            item{
                InventoryItem{
                    .type =
                        announcement.type,
                    .hash =
                        announcement.hash,
                }
            };

        const auto payload =
            serialize_inventory(item);

        for (auto& peer : peers_) {
            if (!peer.session.valid()) {
                continue;
            }

            if (peer.session.send_command(
                    "inv",
                    payload) !=
                PeerError::none) {
                peer.session.close();
            }
        }
    }
}

void NetworkRuntime::prune_closed(
    std::uint64_t now)
{
    for (auto& peer : peers_) {
        if (peer.session.valid() ||
            !peer.address) {
            continue;
        }

        addrman_.mark_failure(
            *peer.address,
            now
        );
        (void)addrman_.save();

        schedule_reconnect(
            *peer.address,
            now
        );
    }

    peers_.erase(
        std::remove_if(
            peers_.begin(),
            peers_.end(),
            [](const LivePeer& peer) {
                return !peer.session.valid();
            }
        ),
        peers_.end()
    );

    known_address_count_.store(
        addrman_.size()
    );
    update_peer_counts();
}

void NetworkRuntime::queue_announcement(
    std::uint32_t type,
    const Hash256& hash)
{
    std::scoped_lock lock(
        announcement_mutex_
    );

    const bool duplicate =
        std::any_of(
            announcements_.begin(),
            announcements_.end(),
            [&](const PendingAnnouncement& item) {
                return item.type == type &&
                       item.hash == hash;
            }
        );

    if (!duplicate) {
        announcements_.push_back(
            PendingAnnouncement{
                .type = type,
                .hash = hash,
            }
        );
    }
}

void NetworkRuntime::schedule_reconnect(
    const PeerAddress& address,
    std::uint64_t now)
{
    const auto it =
        std::find_if(
            reconnect_candidates_.begin(),
            reconnect_candidates_.end(),
            [&](const ReconnectCandidate& item) {
                return same_endpoint(
                    item.address,
                    address
                );
            }
        );

    const std::uint64_t next =
        deadline_after(
            now,
            reconnect_backoff_delay(
                config_.reconnect_delay_seconds,
                0U
            )
        );

    if (it !=
        reconnect_candidates_.end()) {
        // Never shorten an already-earned backoff window.
        it->next_attempt =
            std::max(
                it->next_attempt,
                next
            );
        return;
    }

    reconnect_candidates_.push_back(
        ReconnectCandidate{
            .address = address,
            .next_attempt = next,
            .failures = 0U,
        }
    );
}

void NetworkRuntime::update_peer_counts() noexcept
{
    std::size_t outbound{0U};
    std::uint32_t best_height{0U};
    bool have_height{false};

    for (const auto& peer : peers_) {
        if (!peer.session.valid()) {
            continue;
        }

        if (peer.address) {
            ++outbound;
        }

        best_height =
            std::max(
                best_height,
                peer.reported_height
            );
        have_height = true;
    }

    peer_count_.store(peers_.size());
    outbound_count_.store(outbound);
    peer_best_height_.store(best_height);
    have_peer_height_.store(have_height);
}

void NetworkRuntime::prune_invalid_object_cache(
    std::uint64_t now)
{
    invalid_object_cache_.erase(
        std::remove_if(
            invalid_object_cache_.begin(),
            invalid_object_cache_.end(),
            [now](const InvalidObjectEntry& entry) {
                return entry.expires_at <= now;
            }
        ),
        invalid_object_cache_.end()
    );

    while (invalid_object_cache_.size() >
           config_.max_invalid_object_cache_entries) {
        invalid_object_cache_.pop_front();
    }
}

bool NetworkRuntime::invalid_object_cached(
    const Hash256& digest,
    std::uint64_t now)
{
    prune_invalid_object_cache(now);

    return std::any_of(
        invalid_object_cache_.begin(),
        invalid_object_cache_.end(),
        [&](const InvalidObjectEntry& entry) {
            return entry.digest == digest;
        }
    );
}

void NetworkRuntime::cache_invalid_object(
    const Hash256& digest,
    std::uint64_t now)
{
    if (config_.max_invalid_object_cache_entries == 0U ||
        config_.invalid_object_cache_ttl_seconds == 0U) {
        return;
    }

    prune_invalid_object_cache(now);

    if (std::any_of(
            invalid_object_cache_.begin(),
            invalid_object_cache_.end(),
            [&](const InvalidObjectEntry& entry) {
                return entry.digest == digest;
            })) {
        return;
    }

    while (invalid_object_cache_.size() >=
           config_.max_invalid_object_cache_entries) {
        invalid_object_cache_.pop_front();
    }

    const auto ttl =
        config_.invalid_object_cache_ttl_seconds;
    const auto expires_at =
        now > std::numeric_limits<std::uint64_t>::max() - ttl
            ? std::numeric_limits<std::uint64_t>::max()
            : now + ttl;

    invalid_object_cache_.push_back(
        InvalidObjectEntry{
            .digest = digest,
            .expires_at = expires_at,
        }
    );
}

bool NetworkRuntime::sync_wallet_locked()
{
    if (!wallet_enabled_) {
        return true;
    }

    const auto synced =
        wallet_.sync(
            node_.chain(),
            node_.mempool()
        );

    return synced.ok();
}

} // namespace quintum::net
