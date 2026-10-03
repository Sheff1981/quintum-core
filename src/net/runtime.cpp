#include "net/runtime.hpp"

#include "consensus/tx_auth.hpp"
#include "core/serialize.hpp"
#include "crypto/random.hpp"
#include "crypto/sha256.hpp"
#include "net/relay.hpp"
#include "net/sync.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <random>
#include <thread>
#include <utility>

namespace quintum::net {
namespace {

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
    return a.ipv4 == b.ipv4 &&
           a.port == b.port;
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

    for (const unsigned char ch : value) {
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

    const bool allow_local =
        config_.allow_local_peers ||
        params_.network ==
            consensus::Network::regtest;

    addrman_.set_allow_local(allow_local);

    {
        std::scoped_lock lock(state_mutex_);

        out.node = node_.start();

        if (out.node.ok()) {
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

    if (!out.wallet.ok() ||
        out.wallet_sync !=
            wallet::WalletSyncError::none) {
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

    const std::uint16_t port =
        config_.listen_port.value_or(
            params_.p2p_port
        );

    out.peer_error =
        listener_.listen(
            config_.bind_address,
            port
        );

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

    worker_ = std::thread(
        [this] {
            run_loop();
        }
    );

    return out;
}

void NetworkRuntime::stop() noexcept
{
    if (!running_.load() &&
        !worker_.joinable()) {
        return;
    }

    stop_requested_.store(true);

    if (worker_.joinable()) {
        worker_.join();
    }

    listener_.close();

    for (auto& peer : peers_) {
        peer.session.close();
    }
    peers_.clear();
    reconnect_candidates_.clear();

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
    out.listen_port = listen_port_.load();
    out.peers = peer_count_.load();
    out.outbound_peers =
        outbound_count_.load();
    out.known_addresses =
        known_address_count_.load();

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
    out.mempool_transactions =
        node_.mempool().size();
    out.wallet_balance =
        wallet_.balance();
    out.min_relay_fee_rate_per_kb =
        node_.mempool()
            .min_relay_fee_rate_per_kb();
    out.recommended_fee_rate_per_kb =
        wallet::recommended_fee_rate(
            node_.mempool()
        );

    const auto wallet_addresses =
        wallet_.addresses();

    if (!wallet_addresses.empty()) {
        out.receive_address =
            wallet_addresses.back();
    }

    return out;
}

std::vector<wallet::WalletTransactionRecord>
NetworkRuntime::wallet_history() const
{
    std::scoped_lock lock(state_mutex_);
    return wallet_.history();
}

WalletDesktopSnapshot
NetworkRuntime::desktop_snapshot() const
{
    WalletDesktopSnapshot out;

    out.status.running =
        running_.load();
    out.status.listen_port =
        listen_port_.load();
    out.status.peers =
        peer_count_.load();
    out.status.outbound_peers =
        outbound_count_.load();
    out.status.known_addresses =
        known_address_count_.load();

    std::scoped_lock lock(state_mutex_);

    out.status.height =
        node_.chain().height();
    out.status.tip =
        node_.chain().tip_hash();
    out.status.mempool_transactions =
        node_.mempool().size();
    out.status.wallet_balance =
        wallet_.balance();
    out.status.min_relay_fee_rate_per_kb =
        node_.mempool()
            .min_relay_fee_rate_per_kb();
    out.status.recommended_fee_rate_per_kb =
        wallet::recommended_fee_rate(
            node_.mempool()
        );

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
        queue_announcement(
            kInventoryTransaction,
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
    const NetworkWalletSendPreview& preview)
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

        out =
            send_to_address_auto_fee_locked(
                preview.destination,
                preview.amount
            );
    }

    if (out.ok()) {
        queue_announcement(
            kInventoryTransaction,
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
        queue_announcement(
            kInventoryTransaction,
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

    queue_announcement(
        kInventoryTransaction,
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

    return mine_mempool_block(
        payout_script,
        max_attempts
    );
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
            kProtocolVersion,
        .services = 1U,
        .timestamp = now,
        .nonce = runtime_nonce_,
        .start_height = height,
    };
}

void NetworkRuntime::run_loop() noexcept
{
    try {
        while (!stop_requested_.load()) {
            const std::uint64_t now =
                unix_time_now();

            accept_inbound(now);
            maintain_outbound(now);
            service_peers(now);
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
            connect_and_handshake(
                params_,
                format_ipv4(
                    it->address.ipv4),
                it->address.port,
                local_version(now),
                config_.io_timeout_ms
            );

        if (!connected.ok()) {
            addrman_.mark_failure(
                it->address,
                now
            );
            (void)addrman_.save();

            it->next_attempt =
                now >
                    std::numeric_limits<
                        std::uint64_t>::max() -
                        config_.
                            reconnect_delay_seconds
                    ? std::numeric_limits<
                          std::uint64_t>::max()
                    : now +
                          config_.
                              reconnect_delay_seconds;

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

        if (!prepare_live_peer(
                peer,
                now,
                true)) {
            peer.session.close();
            addrman_.mark_failure(
                it->address,
                now
            );
            (void)addrman_.save();

            it->next_attempt =
                now +
                config_.
                    reconnect_delay_seconds;
            return;
        }

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

    auto connected =
        discovery_.connect_one(
            params_,
            local_version(now),
            now,
            config_.io_timeout_ms,
            excluded
        );

    known_address_count_.store(
        addrman_.size()
    );

    if (!connected.ok()) {
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

    peers_.push_back(
        std::move(peer)
    );

    update_peer_counts();
}

bool NetworkRuntime::prepare_live_peer(
    LivePeer& peer,
    std::uint64_t now,
    bool initial_sync)
{
    if (!peer.session.valid()) {
        return false;
    }

    if (!initial_sync) {
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

    if (message.command == "getaddr") {
        if (!message.payload.empty()) {
            return false;
        }

        const auto addresses =
            addrman_.addresses();

        const auto payload =
            serialize_addresses(addresses);

        return peer.session.send_command(
                   "addr",
                   payload) ==
               PeerError::none;
    }

    if (message.command == "addr") {
        const auto parsed =
            parse_addresses(
                message.payload,
                config_.allow_local_peers ||
                    params_.network ==
                        consensus::Network::regtest
            );

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
        std::scoped_lock lock(state_mutex_);

        return serve_relay_message(
                   peer.session,
                   node_,
                   message
               ).ok();
    }

    if (message.command == "inv") {
        return process_inventory(
            peer,
            message
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
                erase_hash(
                    peer.requested_transactions,
                    item.hash
                );
            } else if (item.type ==
                       kInventoryBlock) {
                erase_hash(
                    peer.requested_blocks,
                    item.hash
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
    const WireMessage& message)
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
                        item.hash) ||
                    contains_hash(
                        peer.
                            requested_transactions,
                        item.hash)) {
                    continue;
                }

                peer.requested_transactions.
                    push_back(item.hash);
                wanted.push_back(item);
                continue;
            }

            if (item.type ==
                kInventoryBlock) {
                if (node_.chain().has_block(
                        item.hash) ||
                    contains_hash(
                        peer.requested_blocks,
                        item.hash)) {
                    continue;
                }

                peer.requested_blocks.
                    push_back(item.hash);
                wanted.push_back(item);
            }
        }
    }

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
        contains_hash(
            peer.requested_transactions,
            txid
        );

    if (!requested) {
        return false;
    }

    erase_hash(
        peer.requested_transactions,
        txid
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

    const bool requested =
        contains_hash(
            peer.requested_blocks,
            hash
        );

    if (!requested) {
        return false;
    }

    erase_hash(
        peer.requested_blocks,
        hash
    );

    NodeSubmitResult submitted;

    {
        std::scoped_lock lock(state_mutex_);
        submitted =
            node_.submit_block_at(
                *block,
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

        if (submitted.ok() &&
            !sync_wallet_locked()) {
            return false;
        }
    }

    if (!submitted.ok()) {
        return submitted.connect.chain.error ==
               ChainConnectError::
                   duplicate_block;
    }

    peer.reported_height =
        std::max(
            peer.reported_height,
            submitted.connect.chain.height
        );
    update_peer_counts();

    queue_announcement(
        kInventoryBlock,
        hash
    );

    return true;
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
        now >
            std::numeric_limits<
                std::uint64_t>::max() -
                config_.
                    reconnect_delay_seconds
            ? std::numeric_limits<
                  std::uint64_t>::max()
            : now +
                  config_.
                      reconnect_delay_seconds;

    if (it !=
        reconnect_candidates_.end()) {
        it->next_attempt =
            std::min(
                it->next_attempt,
                next
            );
        return;
    }

    reconnect_candidates_.push_back(
        ReconnectCandidate{
            .address = address,
            .next_attempt = next,
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

bool NetworkRuntime::sync_wallet_locked()
{
    const auto synced =
        wallet_.sync(
            node_.chain(),
            node_.mempool()
        );

    return synced.ok();
}

} // namespace quintum::net
