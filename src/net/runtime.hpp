#pragma once

#include "net/address.hpp"
#include "net/discovery.hpp"
#include "net/peer.hpp"
#include "node/node.hpp"
#include "wallet/fee_policy.hpp"
#include "wallet/wallet.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace quintum::net {

struct NetworkRuntimeConfig {
    std::string bind_address{"0.0.0.0"};
    std::optional<std::uint16_t> listen_port{};
    bool allow_local_peers{false};
    std::size_t target_outbound{8U};
    std::size_t max_connections{32U};
    std::uint32_t accept_poll_ms{25U};
    std::uint32_t io_timeout_ms{5'000U};
    std::uint64_t outbound_retry_seconds{1U};
    std::uint64_t reconnect_delay_seconds{5U};
    std::uint64_t ping_interval_seconds{120U};
    std::uint64_t ping_timeout_seconds{30U};
    std::vector<PeerAddress> bootstrap_peers{};
    std::string wallet_passphrase{};
};

enum class NetworkRuntimeStartError {
    none,
    already_running,
    node_failed,
    wallet_failed,
    address_store_failed,
    listener_failed,
};

struct NetworkRuntimeStartResult {
    NetworkRuntimeStartError error{
        NetworkRuntimeStartError::none
    };
    NodeStartResult node{};
    wallet::WalletStartResult wallet{};
    wallet::WalletSyncError wallet_sync{
        wallet::WalletSyncError::none
    };
    AddrStoreError address_store{
        AddrStoreError::none
    };
    PeerError peer_error{PeerError::none};

    [[nodiscard]] bool ok() const noexcept
    {
        return error ==
            NetworkRuntimeStartError::none;
    }
};

struct NetworkRuntimeStatus {
    bool running{false};
    std::uint16_t listen_port{0U};
    std::size_t peers{0U};
    std::size_t outbound_peers{0U};
    std::size_t known_addresses{0U};
    std::optional<std::uint32_t> height{};
    std::optional<Hash256> tip{};
    std::size_t mempool_transactions{0U};
    wallet::WalletBalance wallet_balance{};
    Amount min_relay_fee_rate_per_kb{
        policy::kDefaultMinRelayFeeRatePerKb
    };
    Amount recommended_fee_rate_per_kb{
        wallet::kDefaultFeeRatePerKb
    };
    std::string receive_address{};
};

struct WalletTransactionView {
    wallet::WalletTransactionRecord record{};
    std::optional<std::string> label{};
};

struct WalletDesktopSnapshot {
    NetworkRuntimeStatus status{};
    std::vector<WalletTransactionView> transactions{};
    std::vector<wallet::WalletAddressBookEntry> address_book{};
};

struct NetworkWalletSendPreview {
    wallet::WalletFeeQuote quote{};
    std::string destination{};
    Amount amount{0U};
    std::optional<std::string> recipient_label{};
    Hash256 state_hash{};
    Hash256 preview_id{};

    [[nodiscard]] bool ok() const noexcept
    {
        return quote.ok();
    }
};

enum class NetworkWalletSendError {
    none,
    invalid_preview,
    stale_preview,
    wallet_create_failed,
    node_rejected,
    wallet_sync_failed,
};

struct NetworkWalletSendResult {
    NetworkWalletSendError error{
        NetworkWalletSendError::none
    };
    wallet::WalletCreateResult wallet{};
    NodeTransactionResult node{};
    wallet::WalletSyncError wallet_sync{
        wallet::WalletSyncError::none
    };

    [[nodiscard]] bool ok() const noexcept
    {
        return error ==
            NetworkWalletSendError::none;
    }
};

class NetworkRuntime {
public:
    NetworkRuntime(
        const consensus::ChainParams& params,
        std::filesystem::path directory
    );

    ~NetworkRuntime();

    NetworkRuntime(const NetworkRuntime&) = delete;
    NetworkRuntime& operator=(
        const NetworkRuntime&) = delete;

    [[nodiscard]] NetworkRuntimeStartResult start(
        NetworkRuntimeConfig config = {}
    );

    void stop() noexcept;

    [[nodiscard]] bool running() const noexcept;
    [[nodiscard]] NetworkRuntimeStatus status() const;

    [[nodiscard]] std::vector<
        wallet::WalletTransactionRecord>
    wallet_history() const;

    [[nodiscard]] WalletDesktopSnapshot
    desktop_snapshot() const;

    [[nodiscard]] wallet::WalletMetadataError
    set_address_label(
        std::string_view address,
        std::string_view label
    );

    [[nodiscard]] wallet::WalletMetadataError
    set_transaction_label(
        const Hash256& txid,
        std::string_view label
    );

    [[nodiscard]] std::optional<std::string>
    wallet_recovery_mnemonic() const;

    [[nodiscard]] NodeTransactionResult submit_transaction(
        const Transaction& transaction
    );

    [[nodiscard]] wallet::WalletKeyResult
    new_receive_address();

    [[nodiscard]] wallet::WalletStoreError
    encrypt_wallet(
        std::string_view passphrase
    );

    [[nodiscard]] wallet::WalletStoreError
    backup_wallet(
        const std::filesystem::path& destination,
        bool overwrite = false
    );

    [[nodiscard]] wallet::WalletFeeQuote
    quote_send_fee(
        std::string_view destination,
        Amount amount
    );

    [[nodiscard]] NetworkWalletSendPreview
    preview_send(
        std::string_view destination,
        Amount amount
    );

    [[nodiscard]] NetworkWalletSendResult
    confirm_send(
        const NetworkWalletSendPreview& preview
    );

    [[nodiscard]] NetworkWalletSendResult
    send_to_address_auto_fee(
        std::string_view destination,
        Amount amount
    );

    [[nodiscard]] NetworkWalletSendResult
    send_to_address(
        std::string_view destination,
        Amount amount,
        Amount fee
    );

    [[nodiscard]] NodeMineResult mine_mempool_block(
        const Bytes& payout_script,
        std::uint64_t max_attempts
    );

    [[nodiscard]] NodeMineResult mine_mempool_block_at(
        const Bytes& payout_script,
        std::uint64_t adjusted_time,
        std::uint64_t max_attempts
    );

    [[nodiscard]] bool has_mempool_transaction(
        const Hash256& txid
    ) const;

private:
    struct PendingAnnouncement {
        std::uint32_t type{0U};
        Hash256 hash{};
    };

    struct ReconnectCandidate {
        PeerAddress address{};
        std::uint64_t next_attempt{0U};
    };

    struct LivePeer {
        PeerSession session{};
        std::optional<PeerAddress> address{};
        std::uint64_t last_activity{0U};
        std::uint64_t ping_sent_at{0U};
        std::optional<std::uint64_t> pending_ping{};
        std::vector<Hash256> requested_transactions{};
        std::vector<Hash256> requested_blocks{};
    };

    [[nodiscard]] VersionMessage local_version(
        std::uint64_t now
    ) const;

    void run_loop() noexcept;
    void accept_inbound(std::uint64_t now);
    void maintain_outbound(std::uint64_t now);
    void service_peers(std::uint64_t now);
    void flush_announcements();
    void prune_closed(std::uint64_t now);

    [[nodiscard]] bool prepare_live_peer(
        LivePeer& peer,
        std::uint64_t now,
        bool initial_sync
    );

    [[nodiscard]] bool process_message(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    [[nodiscard]] bool process_inventory(
        LivePeer& peer,
        const WireMessage& message
    );

    [[nodiscard]] bool process_transaction(
        LivePeer& peer,
        const WireMessage& message
    );

    [[nodiscard]] bool process_block(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    void queue_announcement(
        std::uint32_t type,
        const Hash256& hash
    );

    void schedule_reconnect(
        const PeerAddress& address,
        std::uint64_t now
    );

    void update_peer_counts() noexcept;

    [[nodiscard]] bool sync_wallet_locked();

    [[nodiscard]] Hash256 send_state_hash_locked() const;

    [[nodiscard]] NetworkWalletSendResult
    send_to_address_auto_fee_locked(
        std::string_view destination,
        Amount amount
    );

    consensus::ChainParams params_{};
    std::filesystem::path directory_{};
    NetworkRuntimeConfig config_{};

    mutable std::mutex state_mutex_{};
    NodeRuntime node_;
    wallet::Wallet wallet_;

    AddrManager addrman_;
    PeerDiscovery discovery_;
    PeerListener listener_;

    std::vector<LivePeer> peers_{};
    std::vector<ReconnectCandidate>
        reconnect_candidates_{};

    mutable std::mutex announcement_mutex_{};
    std::vector<PendingAnnouncement>
        announcements_{};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<std::size_t> peer_count_{0U};
    std::atomic<std::size_t> outbound_count_{0U};
    std::atomic<std::size_t> known_address_count_{0U};
    std::atomic<std::uint16_t> listen_port_{0U};

    std::uint64_t runtime_nonce_{0U};
    std::uint64_t ping_counter_{0U};
    std::uint64_t next_outbound_attempt_{0U};
    std::thread worker_{};
};

} // namespace quintum::net
