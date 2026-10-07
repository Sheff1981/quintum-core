#pragma once

#include "net/address.hpp"
#include "net/compact_block.hpp"
#include "net/discovery.hpp"
#include "net/dandelion.hpp"
#include "net/nat_mapping.hpp"
#include "net/peer.hpp"
#include "node/node.hpp"
#include "wallet/fee_policy.hpp"
#include "wallet/wallet.hpp"

#include <algorithm>
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

inline constexpr std::uint64_t
    kMaxReconnectBackoffSeconds{300U};

[[nodiscard]] constexpr std::uint64_t
reconnect_backoff_delay(
    std::uint64_t base_seconds,
    std::uint32_t failures) noexcept
{
    if (base_seconds == 0U) {
        return 0U;
    }

    std::uint64_t delay =
        std::min<std::uint64_t>(
            base_seconds,
            kMaxReconnectBackoffSeconds
        );

    for (std::uint32_t i = 0U;
         i < failures &&
         delay < kMaxReconnectBackoffSeconds;
         ++i) {
        if (delay >
            kMaxReconnectBackoffSeconds / 2U) {
            return kMaxReconnectBackoffSeconds;
        }

        delay *= 2U;
    }

    return std::min<std::uint64_t>(
        delay,
        kMaxReconnectBackoffSeconds
    );
}

struct NetworkRuntimeConfig {
    std::string bind_address{"0.0.0.0"};
    std::optional<std::uint16_t> listen_port{};
    bool allow_ephemeral_listener_fallback{false};
    bool allow_local_peers{false};
    std::size_t target_outbound{8U};
    std::size_t max_connections{32U};
    std::uint32_t accept_poll_ms{25U};
    std::uint32_t io_timeout_ms{5'000U};
    std::uint64_t outbound_retry_seconds{1U};
    std::uint64_t reconnect_delay_seconds{5U};
    std::uint64_t ping_interval_seconds{120U};
    std::uint64_t ping_timeout_seconds{30U};
    std::uint64_t block_request_timeout_seconds{30U};
    std::size_t max_block_requests_in_flight{16U};
    std::uint64_t transaction_request_timeout_seconds{15U};
    std::size_t max_transaction_requests_in_flight{128U};
    std::size_t max_deferred_transaction_requests{512U};
    std::uint32_t max_messages_per_second{256U};
    std::uint32_t max_stem_transactions_per_second{32U};
    bool enable_dandelion_relay{true};
    std::uint32_t dandelion_fluff_percent{
        kDefaultDandelionFluffPercent
    };
    std::uint64_t dandelion_embargo_min_seconds{
        kDefaultDandelionEmbargoMinSeconds
    };
    std::uint64_t dandelion_embargo_jitter_seconds{
        kDefaultDandelionEmbargoJitterSeconds
    };
    std::uint64_t dandelion_epoch_seconds{
        kDefaultDandelionEpochSeconds
    };
    std::size_t randomx_mining_threads{0U};
    bool randomx_full_memory_mining{true};
    std::vector<PeerAddress> bootstrap_peers{};
    ProxyRoutes proxies{};
    bool enable_nat_mapping{false};
    bool wallet_enabled{true};
    std::string wallet_passphrase{};
    std::string wallet_recovery_mnemonic{};
    std::uint32_t wallet_recovery_gap_limit{
        wallet::kWalletRecoveryGapLimit
    };
};

enum class NetworkRuntimeStartError {
    none,
    already_running,
    invalid_configuration,
    node_failed,
    wallet_failed,
    address_store_failed,
    listener_failed,
    worker_start_failed,
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
    wallet::WalletRecoveryResult wallet_recovery{};
    bool recovered_wallet{false};
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
    bool wallet_enabled{true};
    std::uint16_t listen_port{0U};
    std::size_t peers{0U};
    std::size_t outbound_peers{0U};
    std::size_t known_addresses{0U};
    NatMappingMethod nat_mapping_method{
        NatMappingMethod::none
    };
    std::uint16_t nat_external_port{0U};
    std::optional<std::uint32_t> height{};
    std::optional<std::uint32_t> peer_best_height{};
    bool synchronizing{false};
    double sync_progress{1.0};
    std::optional<Hash256> tip{};
    std::optional<std::uint32_t> difficulty_bits{};
    std::optional<double> difficulty{};
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
    passphrase_required,
    invalid_passphrase,
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

struct NetworkMiningTemplateResult {
    mining::BlockTemplateResult block_template{};
    std::optional<Hash256> randomx_seed{};

    [[nodiscard]] bool ok() const noexcept
    {
        return block_template.ok();
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

    [[nodiscard]] bool verify_wallet_passphrase(
        std::string_view passphrase
    ) const;

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

    [[nodiscard]] wallet::WalletStoreError
    backup_wallet_bundle(
        const std::filesystem::path& destination,
        bool overwrite = false
    );

    [[nodiscard]] wallet::WalletStoreError
    restore_wallet_bundle(
        const std::filesystem::path& source
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
        const NetworkWalletSendPreview& preview,
        std::string_view passphrase = {}
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

    [[nodiscard]] NodeMineResult mine_wallet_block(
        std::uint64_t max_attempts
    );

    [[nodiscard]] NodeMineResult mine_mempool_block_at(
        const Bytes& payout_script,
        std::uint64_t adjusted_time,
        std::uint64_t max_attempts
    );

    [[nodiscard]] NodeSubmitResult submit_block(
        const Block& block
    );

    [[nodiscard]] std::optional<Block> block(
        const Hash256& hash
    ) const;

    [[nodiscard]] std::optional<Hash256> active_hash(
        std::uint32_t height
    ) const;

    [[nodiscard]] std::optional<std::uint32_t> active_height(
        const Hash256& hash
    ) const;

    [[nodiscard]] Hash256 cumulative_work() const;

    [[nodiscard]] std::vector<Hash256>
    mempool_transaction_ids(
        std::size_t limit = kMaxMempoolTransactions
    ) const;

    [[nodiscard]] std::size_t mempool_bytes() const;

    [[nodiscard]] NetworkMiningTemplateResult
    mining_template(
        const Bytes& payout_script,
        std::uint64_t adjusted_time
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
        std::uint32_t failures{0U};
    };

    struct StemRelayState {
        Hash256 txid{};
        std::uint64_t embargo_deadline{0U};
    };

    struct PendingCompactBlock {
        Hash256 hash{};
        CompactBlock compact{};
        std::vector<std::uint32_t>
            missing_indexes{};
    };

    struct PendingBlockRequest {
        Hash256 hash{};
        std::uint64_t requested_at{0U};
    };

    struct PendingTransactionRequest {
        Hash256 hash{};
        std::uint64_t requested_at{0U};
    };

    struct LivePeer {
        PeerSession session{};
        std::optional<PeerAddress> address{};
        std::uint64_t last_activity{0U};
        std::uint64_t ping_sent_at{0U};
        std::optional<std::uint64_t> pending_ping{};
        std::uint32_t reported_height{0U};
        std::uint64_t message_window_started{0U};
        std::uint32_t messages_in_window{0U};
        std::uint32_t stem_transactions_in_window{0U};
        std::vector<PendingTransactionRequest>
            requested_transactions{};
        std::vector<Hash256> deferred_transactions{};
        std::vector<PendingBlockRequest> requested_blocks{};
        std::vector<PendingCompactBlock>
            pending_compact_blocks{};
    };

    [[nodiscard]] VersionMessage local_version(
        std::uint64_t now
    ) const;

    void run_loop() noexcept;
    void run_nat_loop() noexcept;
    void accept_inbound(std::uint64_t now);
    void maintain_outbound(std::uint64_t now);
    void service_peers(std::uint64_t now);
    void flush_private_transactions(std::uint64_t now);
    void service_stem_embargo(std::uint64_t now);
    void flush_announcements();
    void prune_closed(std::uint64_t now);

    [[nodiscard]] bool prepare_live_peer(
        LivePeer& peer,
        std::uint64_t now,
        bool outbound
    );

    [[nodiscard]] bool process_message(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    [[nodiscard]] bool process_inventory(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    [[nodiscard]] bool process_transaction(
        LivePeer& peer,
        const WireMessage& message
    );

    [[nodiscard]] bool drain_transaction_requests(
        LivePeer& peer,
        std::uint64_t now
    );

    [[nodiscard]] bool process_stem_transaction(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    [[nodiscard]] bool process_block(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    [[nodiscard]] bool process_compact_block(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    [[nodiscard]] bool process_get_block_transactions(
        LivePeer& peer,
        const WireMessage& message
    );

    [[nodiscard]] bool process_block_transactions(
        LivePeer& peer,
        const WireMessage& message,
        std::uint64_t now
    );

    [[nodiscard]] bool process_received_block(
        LivePeer& peer,
        const Block& block,
        const Hash256& hash,
        std::uint64_t now
    );

    void queue_announcement(
        std::uint32_t type,
        const Hash256& hash
    );

    void queue_private_transaction(
        const Hash256& txid
    );

    [[nodiscard]] bool relay_stem_transaction(
        const Hash256& txid,
        LivePeer* source,
        std::uint64_t now
    );

    void erase_stem_relay(
        const Hash256& txid
    );

    void promote_private_transaction(
        const Hash256& txid
    );

    [[nodiscard]] std::vector<Hash256>
    hidden_transaction_ids();

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
    bool wallet_enabled_{true};

    mutable std::mutex state_mutex_{};
    NodeRuntime node_;
    wallet::Wallet wallet_;

    AddrManager addrman_;
    PeerDiscovery discovery_;
    PeerListener listener_;
    NatPortMapper nat_mapper_{};

    std::vector<LivePeer> peers_{};
    std::vector<ReconnectCandidate>
        reconnect_candidates_{};

    mutable std::mutex announcement_mutex_{};
    std::vector<PendingAnnouncement>
        announcements_{};
    std::vector<Hash256>
        pending_private_transactions_{};
    std::vector<StemRelayState>
        stem_relays_{};

    std::atomic<bool> running_{false};
    std::atomic<bool> stop_requested_{false};
    std::atomic<std::size_t> peer_count_{0U};
    std::atomic<std::size_t> outbound_count_{0U};
    std::atomic<std::size_t> known_address_count_{0U};
    std::atomic<std::uint32_t> peer_best_height_{0U};
    std::atomic<bool> have_peer_height_{false};
    std::atomic<std::uint16_t> listen_port_{0U};
    std::atomic<NatMappingMethod>
        nat_mapping_method_{
            NatMappingMethod::none};
    std::atomic<std::uint16_t>
        nat_external_port_{0U};

    std::uint64_t runtime_nonce_{0U};
    std::uint64_t wallet_mining_nonce_{0U};
    std::uint64_t ping_counter_{0U};
    std::uint64_t next_outbound_attempt_{0U};
    std::optional<PeerAddress> origin_stem_route_{};
    std::uint64_t origin_stem_epoch_deadline_{0U};
    std::thread worker_{};
    std::thread nat_worker_{};
};

} // namespace quintum::net
