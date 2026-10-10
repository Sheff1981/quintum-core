#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/runtime.hpp"
#include "net/relay.hpp"
#include "net/sync.hpp"
#include "node/node.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <future>
#include <string>
#include <thread>

namespace {

std::filesystem::path unique_dir(
    std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-stage19-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

quintum::crypto::PrivateKey private_key(
    quintum::Byte scalar)
{
    quintum::crypto::PrivateKey key{};
    key.back() = scalar;
    return key;
}

quintum::Bytes payout_script(
    quintum::Byte scalar)
{
    const auto public_key =
        quintum::crypto::derive_public_key(
            private_key(scalar)
        );

    assert(public_key.has_value());

    return quintum::consensus::make_p2pk_locking_script(
        *public_key
    );
}

struct MatureCoin {
    quintum::Hash256 txid{};
    quintum::TxOutput output{};
};

bool wait_until(
    std::chrono::seconds timeout,
    const std::function<bool()>& predicate)
{
    const auto deadline =
        std::chrono::steady_clock::now() +
        timeout;

    while (std::chrono::steady_clock::now() <
           deadline) {
        if (predicate()) {
            return true;
        }

        std::this_thread::sleep_for(
            std::chrono::milliseconds(25)
        );
    }

    return predicate();
}

MatureCoin prepare_chain_data(
    const quintum::consensus::ChainParams& params,
    const std::filesystem::path& server_dir,
    const std::filesystem::path& client_dir,
    std::uint64_t base_time,
    const quintum::Bytes& payout)
{
    using namespace quintum;

    NodeRuntime server{
        params,
        server_dir
    };
    NodeRuntime client{
        params,
        client_dir
    };

    assert(server.start_at(base_time).ok());
    assert(client.start_at(base_time).ok());

    MatureCoin mature;

    for (std::uint32_t height = 1U;
         height <= 105U;
         ++height) {
        const auto mined =
            server.mine_block_at(
                payout,
                base_time +
                    static_cast<std::uint64_t>(
                        height),
                4'096U
            );

        assert(mined.ok());
        assert(mined.height == height);

        if (height == 1U) {
            mature.txid =
                transaction_id(
                    mined.block.transactions.front()
                );
            mature.output =
                mined.block.transactions.front()
                    .outputs.front();
        }

        if (height <= 100U) {
            const auto submitted =
                client.submit_block_at(
                    mined.block,
                    base_time +
                        static_cast<std::uint64_t>(
                            height)
                );

            assert(submitted.ok());
        }
    }

    assert(*server.chain().height() == 105U);
    assert(*client.chain().height() == 100U);

    return mature;
}

MatureCoin prepare_equal_chain_data(
    const quintum::consensus::ChainParams& params,
    const std::filesystem::path& first_dir,
    const std::filesystem::path& second_dir,
    const std::filesystem::path& third_dir,
    std::uint64_t base_time,
    const quintum::Bytes& payout)
{
    using namespace quintum;

    NodeRuntime first{params, first_dir};
    NodeRuntime second{params, second_dir};
    NodeRuntime third{params, third_dir};

    assert(first.start_at(base_time).ok());
    assert(second.start_at(base_time).ok());
    assert(third.start_at(base_time).ok());

    MatureCoin mature;

    for (std::uint32_t height = 1U;
         height <= 100U;
         ++height) {
        const auto mined =
            first.mine_block_at(
                payout,
                base_time +
                    static_cast<std::uint64_t>(
                        height),
                4'096U
            );

        assert(mined.ok());
        assert(mined.height == height);

        if (height == 1U) {
            mature.txid =
                transaction_id(
                    mined.block.transactions.front()
                );
            mature.output =
                mined.block.transactions.front()
                    .outputs.front();
        }

        const auto second_result =
            second.submit_block_at(
                mined.block,
                base_time +
                    static_cast<std::uint64_t>(
                        height)
            );
        const auto third_result =
            third.submit_block_at(
                mined.block,
                base_time +
                    static_cast<std::uint64_t>(
                        height)
            );

        assert(second_result.ok());
        assert(third_result.ok());
    }

    assert(*first.chain().height() == 100U);
    assert(*second.chain().height() == 100U);
    assert(*third.chain().height() == 100U);
    assert(first.chain().tip_hash() ==
           second.chain().tip_hash());
    assert(first.chain().tip_hash() ==
           third.chain().tip_hash());

    return mature;
}

quintum::Transaction make_spend(
    const MatureCoin& coin,
    quintum::Byte scalar,
    quintum::Amount fee)
{
    using namespace quintum;

    Transaction spend;
    spend.inputs.push_back(
        TxInput{
            .previous_output = OutPoint{
                .txid = coin.txid,
                .index = 0U,
            },
        }
    );

    spend.outputs.push_back(
        TxOutput{
            .value = coin.output.value - fee,
            .locking_script =
                payout_script(scalar),
        }
    );

    assert(consensus::sign_p2pk_input(
               spend,
               0U,
               coin.output,
               private_key(scalar)) ==
           consensus::InputAuthError::none);

    return spend;
}

void test_default_listener_port_fallback()
{
    using namespace quintum;
    using namespace quintum::net;

    auto params =
        consensus::regtest_params();

    PeerListener blocker{params};

    assert(blocker.listen(
               "127.0.0.1",
               0U) ==
           PeerError::none);

    const std::uint16_t blocked_port =
        blocker.local_port();

    assert(blocked_port != 0U);

    params.p2p_port = blocked_port;

    const auto directory =
        unique_dir("listener-fallback");

    NetworkRuntime runtime{
        params,
        directory
    };

    NetworkRuntimeConfig config;
    config.bind_address =
        "127.0.0.1";
    config.target_outbound = 0U;
    config.allow_local_peers = true;
    config.allow_ephemeral_listener_fallback =
        true;
    config.wallet_passphrase =
        "listener-fallback-wallet";

    const auto started =
        runtime.start(
            std::move(config)
        );

    assert(started.ok());

    const auto status =
        runtime.status();

    assert(status.running);
    assert(status.listen_port != 0U);
    assert(status.listen_port != blocked_port);

    runtime.stop();
    blocker.close();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_walletless_seed_runtime_creates_no_wallet()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();

    const auto directory =
        unique_dir("walletless-seed");

    NetworkRuntime runtime{
        params,
        directory
    };

    NetworkRuntimeConfig config;
    config.bind_address =
        "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.wallet_enabled = false;

    const auto started =
        runtime.start(
            std::move(config)
        );

    assert(started.ok());

    const auto status =
        runtime.status();

    assert(status.running);
    assert(!status.wallet_enabled);
    assert(status.receive_address.empty());
    assert(status.wallet_balance ==
           wallet::WalletBalance{});

    assert(!std::filesystem::exists(
        directory / "wallet.dat"
    ));
    assert(!std::filesystem::exists(
        directory / "wallet_state.dat"
    ));
    assert(!std::filesystem::exists(
        directory / "wallet_meta.dat"
    ));

    const auto mined =
        runtime.mine_mempool_block_at(
            payout_script(19U),
            params.genesis.timestamp + 1'000U,
            4'096U
        );

    assert(mined.ok());
    assert(runtime.status().height ==
           std::optional<std::uint32_t>{1U});

    runtime.stop();

    assert(!std::filesystem::exists(
        directory / "wallet.dat"
    ));
    assert(!std::filesystem::exists(
        directory / "wallet_state.dat"
    ));
    assert(!std::filesystem::exists(
        directory / "wallet_meta.dat"
    ));

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_peer_message_flood_is_disconnected()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto directory =
        unique_dir("message-rate");

    NetworkRuntime runtime{
        params,
        directory
    };

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.wallet_enabled = false;
    config.accept_poll_ms = 5U;
    config.io_timeout_ms = 2'000U;
    config.max_messages_per_second = 3U;

    assert(runtime.start(config).ok());

    const auto port =
        runtime.status().listen_port;
    assert(port != 0U);

    const VersionMessage local{
        .protocol_version =
            params.p2p_protocol_version,
        .services = 1U,
        .timestamp =
            params.genesis.timestamp + 10U,
        .nonce = 0x55aa55aaU,
        .start_height = 0U,
    };

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            port,
            local,
            2'000U
        );
    assert(connected.ok());

    assert(wait_until(
        std::chrono::seconds(3),
        [&] {
            return runtime.status().peers == 1U;
        }
    ));

    const Bytes empty;
    for (std::uint32_t i = 0U;
         i < 8U;
         ++i) {
        if (connected.session->send_command(
                "unknown",
                empty) !=
            PeerError::none) {
            break;
        }
    }

    assert(wait_until(
        std::chrono::seconds(3),
        [&] {
            return runtime.status().peers == 0U;
        }
    ));

    connected.session->close();
    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}


void test_malformed_ping_disconnects_peer()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto dir =
        unique_dir("malformed-ping-disconnect");

    NetworkRuntime runtime{params, dir};

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.wallet_enabled = false;
    config.accept_poll_ms = 10U;
    config.io_timeout_ms = 5'000U;
    config.ping_interval_seconds = 30U;
    config.ping_timeout_seconds = 5U;

    assert(runtime.start(config).ok());

    VersionMessage remote;
    remote.protocol_version =
        params.p2p_protocol_version;
    remote.services = kServiceNetwork;
    remote.timestamp =
        params.genesis.timestamp + 90'000U;
    remote.nonce = 0x47004700ULL;
    remote.start_height = 0U;
    remote.listen_port = 0U;

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            runtime.status().listen_port,
            remote,
            5'000U
        );

    assert(connected.ok());

    assert(wait_until(
        std::chrono::seconds(5),
        [&] {
            return runtime.status().peers == 1U;
        }
    ));

    // ping requires exactly one serialized uint64 nonce. A truncated payload
    // is malformed protocol input and must deterministically disconnect the
    // offending peer instead of being retried or consuming node resources.
    const Bytes malformed_ping{
        Byte{0x47U},
        Byte{0x00U},
        Byte{0x47U},
    };

    assert(connected.session->send_command(
               "ping",
               malformed_ping) ==
           PeerError::none);

    assert(wait_until(
        std::chrono::seconds(5),
        [&] {
            return runtime.status().peers == 0U;
        }
    ));

    connected.session->close();
    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}



void test_compact_block_resource_caps_are_fail_closed()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();

    // An explicit compact-state budget may never exceed the block request
    // budget it belongs to.
    {
        const auto dir =
            unique_dir("compact-resource-cap-invalid");

        NetworkRuntime runtime{params, dir};

        NetworkRuntimeConfig config;
        config.bind_address = "127.0.0.1";
        config.listen_port = 0U;
        config.allow_local_peers = true;
        config.target_outbound = 0U;
        config.wallet_enabled = false;
        config.max_block_requests_in_flight = 4U;
        config.max_pending_compact_blocks = 5U;

        const auto started = runtime.start(config);
        assert(!started.ok());
        assert(started.error ==
               NetworkRuntimeStartError::
                   invalid_configuration);
        assert(!runtime.running());

        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    // Zero is an inheritance sentinel, not an unbounded/disabled cap. This
    // preserves existing custom block-request configurations while keeping
    // compact reconstruction state bounded by the same per-peer budget.
    {
        const auto dir =
            unique_dir("compact-resource-cap-inherit");

        NetworkRuntime runtime{params, dir};

        NetworkRuntimeConfig config;
        config.bind_address = "127.0.0.1";
        config.listen_port = 0U;
        config.allow_local_peers = true;
        config.target_outbound = 0U;
        config.wallet_enabled = false;
        config.max_block_requests_in_flight = 4U;
        config.max_pending_compact_blocks = 0U;

        assert(runtime.start(config).ok());
        runtime.stop();

        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }
}


void test_continuous_runtime_sync_relay_reconnect()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();

    const auto server_dir =
        unique_dir("server");
    const auto client_dir =
        unique_dir("client");

    const std::uint64_t base_time =
        params.genesis.timestamp + 1'000U;

    const auto payout =
        payout_script(9U);

    const MatureCoin coin =
        prepare_chain_data(
            params,
            server_dir,
            client_dir,
            base_time,
            payout
        );

    NetworkRuntimeConfig server_config;
    server_config.bind_address =
        "127.0.0.1";
    server_config.listen_port = 0U;
    server_config.allow_local_peers = true;
    server_config.target_outbound = 0U;
    server_config.accept_poll_ms = 20U;
    server_config.io_timeout_ms = 5'000U;
    server_config.ping_interval_seconds = 1U;
    server_config.ping_timeout_seconds = 4U;

    NetworkRuntime server{
        params,
        server_dir
    };

    const auto server_start =
        server.start(server_config);

    assert(server_start.ok());

    const auto initial_server_status =
        server.status();

    assert(initial_server_status.running);
    assert(initial_server_status.listen_port != 0U);
    assert(initial_server_status.height ==
           std::optional<std::uint32_t>{105U});

    NetworkRuntimeConfig client_config =
        server_config;

    client_config.target_outbound = 1U;
    client_config.reconnect_delay_seconds = 1U;
    client_config.outbound_retry_seconds = 1U;

    client_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 =
                *parse_ipv4("127.0.0.1"),
            .port =
                initial_server_status.listen_port,
            .services = 1U,
            .last_seen = base_time,
        }
    );

    NetworkRuntime client{
        params,
        client_dir
    };

    const auto client_start =
        client.start(client_config);

    assert(client_start.ok());

    assert(wait_until(
        std::chrono::seconds(20),
        [&] {
            const auto status =
                client.status();

            return status.outbound_peers == 1U &&
                   status.height ==
                       std::optional<std::uint32_t>{
                           105U} &&
                   status.tip ==
                       server.status().tip;
        }
    ));

    assert(wait_until(
        std::chrono::seconds(5),
        [&] {
            return server.status().peers >= 1U;
        }
    ));

    // Let the connection stay idle long enough to force asynchronous
    // ping/pong liveness on both runtimes.
    std::this_thread::sleep_for(
        std::chrono::seconds(2)
    );

    assert(client.status().outbound_peers == 1U);
    assert(server.status().peers >= 1U);

    constexpr Amount fee{555U};
    const auto spend =
        make_spend(
            coin,
            9U,
            fee
        );

    const Hash256 txid =
        transaction_id(spend);

    const auto accepted =
        server.submit_transaction(spend);

    assert(accepted.ok());
    assert(accepted.mempool.fee == fee);

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            return client
                .has_mempool_transaction(txid);
        }
    ));

    const auto mined =
        server.mine_mempool_block_at(
            payout,
            base_time + 1'000U,
            4'096U
        );

    assert(mined.ok());
    assert(mined.height == 106U);
    assert(mined.total_fees == fee);

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            const auto server_status =
                server.status();
            const auto client_status =
                client.status();

            return client_status.height ==
                       std::optional<std::uint32_t>{
                           106U} &&
                   client_status.tip ==
                       server_status.tip &&
                   client_status.
                       mempool_transactions == 0U;
        }
    ));

    const std::uint16_t reconnect_port =
        server.status().listen_port;

    server.stop();
    assert(!server.running());

    assert(wait_until(
        std::chrono::seconds(8),
        [&] {
            return client.status().peers == 0U;
        }
    ));

    NetworkRuntime restarted_server{
        params,
        server_dir
    };

    NetworkRuntimeConfig restart_config =
        server_config;

    restart_config.listen_port =
        reconnect_port;

    const auto restart =
        restarted_server.start(
            restart_config
        );

    assert(restart.ok());

    assert(wait_until(
        std::chrono::seconds(12),
        [&] {
            return client.status().
                       outbound_peers == 1U &&
                   restarted_server.status().
                       peers >= 1U;
        }
    ));

    const auto after_reconnect =
        restarted_server.mine_mempool_block_at(
            payout,
            base_time + 2'000U,
            4'096U
        );

    assert(after_reconnect.ok());
    assert(after_reconnect.height == 107U);

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            const auto server_status =
                restarted_server.status();
            const auto client_status =
                client.status();

            return client_status.height ==
                       std::optional<std::uint32_t>{
                           107U} &&
                   client_status.tip ==
                       server_status.tip;
        }
    ));

    // Mine on the client. The server accepts and re-announces the same
    // active block. The originating client must learn that its peer now
    // has this height instead of leaving peer_best_height stuck at the
    // handshake height.
    const auto client_mined =
        client.mine_mempool_block_at(
            payout,
            base_time + 3'000U,
            4'096U
        );

    assert(client_mined.ok());
    assert(client_mined.height == 108U);

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            const auto server_status =
                restarted_server.status();
            const auto client_status =
                client.status();

            return server_status.height ==
                       std::optional<std::uint32_t>{
                           108U} &&
                   client_status.height ==
                       std::optional<std::uint32_t>{
                           108U} &&
                   client_status.peer_best_height ==
                       std::optional<std::uint32_t>{
                           108U};
        }
    ));

    client.stop();
    restarted_server.stop();

    assert(!client.running());
    assert(!restarted_server.running());

    std::error_code ec;
    std::filesystem::remove_all(
        server_dir,
        ec
    );
    std::filesystem::remove_all(
        client_dir,
        ec
    );
}


void test_dandelion_three_node_relay_and_block_confirmation()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();

    const auto origin_dir =
        unique_dir("dandelion-origin");
    const auto middle_dir =
        unique_dir("dandelion-middle");
    const auto edge_dir =
        unique_dir("dandelion-edge");

    const std::uint64_t base_time =
        params.genesis.timestamp + 50'000U;

    const auto payout =
        payout_script(13U);

    const MatureCoin coin =
        prepare_equal_chain_data(
            params,
            origin_dir,
            middle_dir,
            edge_dir,
            base_time,
            payout
        );

    NetworkRuntimeConfig base_config;
    base_config.bind_address =
        "127.0.0.1";
    base_config.listen_port = 0U;
    base_config.allow_local_peers = true;
    base_config.accept_poll_ms = 10U;
    base_config.io_timeout_ms = 5'000U;
    base_config.outbound_retry_seconds = 1U;
    base_config.reconnect_delay_seconds = 1U;
    base_config.ping_interval_seconds = 30U;
    base_config.ping_timeout_seconds = 5U;
    base_config.enable_dandelion_relay = true;
    base_config.dandelion_fluff_percent = 0U;
    base_config.dandelion_embargo_min_seconds = 2U;
    base_config.dandelion_embargo_jitter_seconds = 0U;

    NetworkRuntimeConfig edge_config =
        base_config;
    edge_config.target_outbound = 0U;

    NetworkRuntime edge{
        params,
        edge_dir
    };
    assert(edge.start(edge_config).ok());

    NetworkRuntimeConfig middle_config =
        base_config;
    middle_config.target_outbound = 1U;
    middle_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 =
                *parse_ipv4("127.0.0.1"),
            .port =
                edge.status().listen_port,
            .services = 1U,
            .last_seen = base_time,
        }
    );

    NetworkRuntime middle{
        params,
        middle_dir
    };
    assert(middle.start(middle_config).ok());

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            return middle.status().
                       outbound_peers == 1U &&
                   edge.status().peers >= 1U;
        }
    ));

    NetworkRuntimeConfig origin_config =
        base_config;
    origin_config.target_outbound = 1U;
    origin_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 =
                *parse_ipv4("127.0.0.1"),
            .port =
                middle.status().listen_port,
            .services = 1U,
            .last_seen = base_time,
        }
    );

    NetworkRuntime origin{
        params,
        origin_dir
    };
    assert(origin.start(origin_config).ok());

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            return origin.status().
                       outbound_peers == 1U &&
                   middle.status().peers >= 2U;
        }
    ));

    constexpr Amount fee{777U};
    const auto spend =
        make_spend(
            coin,
            13U,
            fee
        );
    const Hash256 txid =
        transaction_id(spend);

    const auto submitted =
        origin.submit_transaction(
            spend
        );

    assert(submitted.ok());
    assert(submitted.mempool.fee == fee);

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            return middle.
                       has_mempool_transaction(
                           txid) &&
                   edge.
                       has_mempool_transaction(
                           txid);
        }
    ));

    const auto mined =
        edge.mine_mempool_block_at(
            payout,
            base_time + 1'000U,
            4'096U
        );

    assert(mined.ok());
    assert(mined.height == 101U);
    assert(mined.total_fees == fee);

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            const auto a = origin.status();
            const auto b = middle.status();
            const auto c = edge.status();

            return a.height ==
                       std::optional<std::uint32_t>{
                           101U} &&
                   b.height ==
                       std::optional<std::uint32_t>{
                           101U} &&
                   c.height ==
                       std::optional<std::uint32_t>{
                           101U} &&
                   a.tip == c.tip &&
                   b.tip == c.tip &&
                   a.mempool_transactions == 0U &&
                   b.mempool_transactions == 0U &&
                   c.mempool_transactions == 0U;
        }
    ));

    origin.stop();
    middle.stop();
    edge.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        origin_dir,
        ec
    );
    std::filesystem::remove_all(
        middle_dir,
        ec
    );
    std::filesystem::remove_all(
        edge_dir,
        ec
    );
}

void test_higher_outbound_peer_updates_lower_inbound()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();

    const auto higher_dir =
        unique_dir("higher-outbound");
    const auto lower_dir =
        unique_dir("lower-inbound");

    const std::uint64_t base_time =
        params.genesis.timestamp + 20'000U;

    const auto payout =
        payout_script(11U);

    (void)prepare_chain_data(
        params,
        higher_dir,
        lower_dir,
        base_time,
        payout
    );

    NetworkRuntimeConfig lower_config;
    lower_config.bind_address =
        "127.0.0.1";
    lower_config.listen_port = 0U;
    lower_config.allow_local_peers = true;
    lower_config.target_outbound = 0U;
    lower_config.accept_poll_ms = 20U;
    lower_config.io_timeout_ms = 5'000U;
    lower_config.ping_interval_seconds = 1U;
    lower_config.ping_timeout_seconds = 4U;

    NetworkRuntime lower{
        params,
        lower_dir
    };

    const auto lower_start =
        lower.start(lower_config);

    assert(lower_start.ok());
    assert(lower.status().height ==
           std::optional<std::uint32_t>{100U});

    NetworkRuntimeConfig higher_config =
        lower_config;

    higher_config.target_outbound = 1U;
    higher_config.reconnect_delay_seconds = 1U;
    higher_config.outbound_retry_seconds = 1U;

    higher_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 =
                *parse_ipv4("127.0.0.1"),
            .port =
                lower.status().listen_port,
            .services = 1U,
            .last_seen = base_time,
        }
    );

    NetworkRuntime higher{
        params,
        higher_dir
    };

    const auto higher_start =
        higher.start(higher_config);

    assert(higher_start.ok());
    assert(higher.status().height ==
           std::optional<std::uint32_t>{105U});

    assert(wait_until(
        std::chrono::seconds(20),
        [&] {
            const auto low =
                lower.status();
            const auto high =
                higher.status();

            return low.height ==
                       std::optional<std::uint32_t>{105U} &&
                   low.tip == high.tip &&
                   low.peers == 1U &&
                   high.outbound_peers == 1U;
        }
    ));

    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            return higher.status().
                       peer_best_height ==
                   std::optional<std::uint32_t>{105U};
        }
    ));

    higher.stop();
    lower.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        higher_dir,
        ec
    );
    std::filesystem::remove_all(
        lower_dir,
        ec
    );
}

void test_chainwork_runtime_prefers_shorter_heavier_peer()
{
    using namespace quintum;
    using namespace quintum::net;

    auto params =
        consensus::regtest_params();

    // Test-only difficulty schedule: every second block retargets. A fast
    // two-block branch therefore accumulates more work than a deliberately
    // slower four-block branch. Production/testnet parameters are untouched.
    params.pow.target_spacing_seconds = 10U;
    params.pow.retarget_interval = 2U;
    params.pow.allow_min_difficulty_blocks = false;
    params.pow.no_retargeting = false;

    const auto heavy_dir =
        unique_dir("chainwork-heavy-short");
    const auto light_dir =
        unique_dir("chainwork-light-tall");

    const std::uint64_t genesis_time =
        params.genesis.timestamp;
    const auto payout = payout_script(29U);

    Hash256 heavy_tip{};
    Hash256 heavy_work{};
    Hash256 light_work{};

    {
        NodeRuntime heavy{params, heavy_dir};
        NodeRuntime light{params, light_dir};

        assert(heavy.start_at(genesis_time + 10'000U).ok());
        assert(light.start_at(genesis_time + 10'000U).ok());

        const auto heavy_one =
            heavy.mine_block_at(
                payout,
                genesis_time + 1U,
                65'536U
            );
        assert(heavy_one.ok());

        const auto heavy_two =
            heavy.mine_block_at(
                payout,
                genesis_time + 2U,
                65'536U
            );
        assert(heavy_two.ok());
        assert(heavy.chain().height() ==
               std::optional<std::uint32_t>{2U});

        for (const std::uint64_t timestamp : {
                 genesis_time + 20U,
                 genesis_time + 30U,
                 genesis_time + 50U,
                 genesis_time + 60U}) {
            const auto mined =
                light.mine_block_at(
                    payout,
                    timestamp,
                    65'536U
                );
            assert(mined.ok());
        }

        assert(light.chain().height() ==
               std::optional<std::uint32_t>{4U});

        heavy_tip = *heavy.chain().tip_hash();
        heavy_work = heavy.chain().cumulative_work();
        light_work = light.chain().cumulative_work();

        // The entire point of this fixture: height and accumulated PoW point
        // in opposite directions.
        assert(light_work < heavy_work);
    }

    NetworkRuntimeConfig light_config;
    light_config.bind_address = "127.0.0.1";
    light_config.listen_port = 0U;
    light_config.allow_local_peers = true;
    light_config.target_outbound = 0U;
    light_config.wallet_enabled = false;
    light_config.accept_poll_ms = 10U;
    light_config.io_timeout_ms = 5'000U;
    light_config.ping_interval_seconds = 30U;
    light_config.ping_timeout_seconds = 5U;

    NetworkRuntime light{params, light_dir};
    assert(light.start(light_config).ok());
    assert(light.status().height ==
           std::optional<std::uint32_t>{4U});

    NetworkRuntimeConfig heavy_config =
        light_config;
    heavy_config.target_outbound = 1U;
    heavy_config.outbound_retry_seconds = 1U;
    heavy_config.reconnect_delay_seconds = 1U;
    heavy_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 = *parse_ipv4("127.0.0.1"),
            .port = light.status().listen_port,
            .services = 1U,
            .last_seen = genesis_time + 100U,
        }
    );

    NetworkRuntime heavy{params, heavy_dir};
    assert(heavy.start(heavy_config).ok());
    assert(heavy.status().height ==
           std::optional<std::uint32_t>{2U});

    // The outbound node is shorter but heavier. Height-only setup would make
    // it request the weaker height-4 branch while the inbound node stayed
    // passive. Chainwork negotiation must reverse the sync direction so the
    // taller/weaker node reorganizes to the height-2 heavier tip.
    assert(wait_until(
        std::chrono::seconds(20),
        [&] {
            const auto low_work = light.status();
            const auto high_work = heavy.status();

            return low_work.height ==
                       std::optional<std::uint32_t>{2U} &&
                   low_work.tip ==
                       std::optional<Hash256>{heavy_tip} &&
                   low_work.tip == high_work.tip &&
                   low_work.peers == 1U &&
                   high_work.outbound_peers == 1U;
        }
    ));

    assert(light.cumulative_work() == heavy_work);
    assert(heavy.cumulative_work() == heavy_work);

    assert(wait_until(
        std::chrono::seconds(5),
        [&] {
            const auto heavy_status = heavy.status();
            return heavy_status.peer_best_height ==
                       std::optional<std::uint32_t>{2U} &&
                   !heavy_status.synchronizing &&
                   heavy_status.sync_progress == 1.0;
        }
    ));

    heavy.stop();
    light.stop();

    std::error_code ec;
    std::filesystem::remove_all(heavy_dir, ec);
    std::filesystem::remove_all(light_dir, ec);
}


void test_reconnect_backoff_grows_and_caps()
{
    using namespace quintum::net;

    assert(reconnect_backoff_delay(5U, 0U) == 5U);
    assert(reconnect_backoff_delay(5U, 1U) == 10U);
    assert(reconnect_backoff_delay(5U, 2U) == 20U);
    assert(reconnect_backoff_delay(5U, 3U) == 40U);
    assert(reconnect_backoff_delay(5U, 6U) ==
           kMaxReconnectBackoffSeconds);
    assert(reconnect_backoff_delay(500U, 0U) ==
           kMaxReconnectBackoffSeconds);
    assert(reconnect_backoff_delay(0U, 10U) == 0U);
}



void test_status_polling_does_not_wait_for_initial_sync()
{
    using namespace quintum;
    using namespace quintum::net;
    const auto& params = consensus::regtest_params();
    const auto dir = unique_dir("status-during-stalled-sync");
    NetworkRuntime runtime{params, dir};
    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.wallet_enabled = false;
    config.accept_poll_ms = 10U;
    config.io_timeout_ms = 5'000U;
    assert(runtime.start(config).ok());
    const auto before = runtime.status();
    assert(before.height.has_value());

    VersionMessage remote;
    remote.protocol_version = kProtocolVersion;
    remote.services = kServiceNetwork;
    remote.timestamp = params.genesis.timestamp + 81'000U;
    remote.nonce = 0x535441545553ULL;
    remote.start_height = *before.height + 1U;
    remote.listen_port = 39444U;
    auto connected = connect_and_handshake(
        params, "127.0.0.1", before.listen_port, remote, 5'000U);
    assert(connected.ok());

    // Runtime drives sync from this higher peer on the bounded worker.
    // Deliberately leave headers unanswered; chain snapshots stay available.
    WireMessage request;
    assert(connected.session->receive_command(request) == PeerError::none);
    if (request.command == "getaddr" || request.command == "getaddrv2") {
        const bool v2 = request.command == "getaddrv2";
        const auto payload = v2
            ? serialize_addresses_v2(std::span<const PeerAddress>{})
            : serialize_addresses(std::span<const PeerAddress>{});
        assert(connected.session->send_command(v2 ? "addrv2" : "addr", payload)
               == PeerError::none);
        assert(connected.session->receive_command(request) == PeerError::none);
    }
    assert(request.command == "getheaders");
    auto poll = std::async(std::launch::async, [&] {
        return runtime.status_nonblocking();
    });
    assert(poll.wait_for(std::chrono::seconds(1)) == std::future_status::ready);
    const auto during = poll.get();
    assert(during.running);
    assert(during.listen_port == before.listen_port);
    assert(during.height == before.height);
    assert(during.difficulty == before.difficulty);

    connected.session->close();
    // A transport failure must not damage the readable chain snapshot.
    assert(wait_until(std::chrono::seconds(5), [&] {
        return runtime.status_nonblocking().height == before.height;
    }));
    runtime.stop();
    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}

void test_stalled_transaction_requests_are_bounded_and_expire()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto dir =
        unique_dir("transaction-request-timeout");

    NetworkRuntime runtime{
        params,
        dir
    };

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.wallet_enabled = false;
    config.accept_poll_ms = 10U;
    config.io_timeout_ms = 5'000U;
    config.ping_interval_seconds = 30U;
    config.ping_timeout_seconds = 5U;
    // Keep the test deadline comfortably above scheduler/sanitizer jitter so
    // the peer can answer the first getdata before expiry. The final phase
    // still leaves the second batch unanswered and verifies bounded expiry.
    config.transaction_request_timeout_seconds = 3U;
    config.max_transaction_requests_in_flight = 4U;

    assert(runtime.start(config).ok());

    VersionMessage remote;
    remote.protocol_version =
        params.p2p_protocol_version;
    remote.services = kServiceNetwork;
    remote.timestamp =
        params.genesis.timestamp + 81'000U;
    remote.nonce = 0x41004100ULL;
    remote.start_height = 0U;
    remote.listen_port = 0U;

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            runtime.status().listen_port,
            remote,
            5'000U
        );

    assert(connected.ok());

    assert(wait_until(
        std::chrono::seconds(5),
        [&] {
            return runtime.status().peers == 1U;
        }
    ));

    std::vector<InventoryItem> announced;
    announced.reserve(6U);

    for (Byte i = 1U; i <= 6U; ++i) {
        Hash256 txid{};
        txid.front() = static_cast<Byte>(0x40U + i);

        announced.push_back(
            InventoryItem{
                .type = kInventoryTransaction,
                .hash = txid,
            }
        );
    }

    assert(connected.session->send_command(
               "inv",
               serialize_inventory(announced)) ==
           PeerError::none);

    WireMessage request;
    assert(connected.session->receive_command(
               request) ==
           PeerError::none);
    assert(request.command == "getdata");

    const auto requested =
        parse_inventory(request.payload);

    assert(requested.has_value());
    assert(requested->size() == 4U);

    for (std::size_t i = 0U;
         i < requested->size();
         ++i) {
        assert((*requested)[i].type ==
               kInventoryTransaction);
        assert((*requested)[i].hash ==
               announced[i].hash);
    }

    // Free the first four slots. The two deferred announcements must not be
    // lost: the runtime should automatically drain them into a second
    // getdata request.
    assert(connected.session->send_command(
               "notfound",
               serialize_inventory(*requested)) ==
           PeerError::none);

    WireMessage second_request;
    assert(connected.session->receive_command(
               second_request) ==
           PeerError::none);
    assert(second_request.command == "getdata");

    const auto second_requested =
        parse_inventory(second_request.payload);

    assert(second_requested.has_value());
    assert(second_requested->size() == 2U);
    assert((*second_requested)[0].hash ==
           announced[4].hash);
    assert((*second_requested)[1].hash ==
           announced[5].hash);

    // Leave the second batch unanswered. Its own deadline must close the
    // connection instead of letting transaction request state live forever.
    assert(wait_until(
        std::chrono::seconds(6),
        [&] {
            return runtime.status().peers == 0U;
        }
    ));

    connected.session->close();
    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


void test_stalled_block_requests_are_bounded_and_expire()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto dir =
        unique_dir("block-request-timeout");

    NetworkRuntime runtime{
        params,
        dir
    };

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.wallet_enabled = false;
    config.accept_poll_ms = 10U;
    config.io_timeout_ms = 5'000U;
    config.ping_interval_seconds = 30U;
    config.ping_timeout_seconds = 5U;
    config.block_request_timeout_seconds = 1U;
    config.max_block_requests_in_flight = 4U;

    assert(runtime.start(config).ok());

    VersionMessage remote;
    remote.protocol_version =
        params.p2p_protocol_version;
    // Deliberately omit the Stage 38 chainwork capability so this fixture
    // exercises only steady-state inventory/download behavior.
    remote.services = kServiceNetwork;
    remote.timestamp =
        params.genesis.timestamp + 80'000U;
    remote.nonce = 0x39003900ULL;
    remote.start_height = 0U;
    remote.listen_port = 0U;

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            runtime.status().listen_port,
            remote,
            5'000U
        );

    assert(connected.ok());

    assert(wait_until(
        std::chrono::seconds(5),
        [&] {
            return runtime.status().peers == 1U;
        }
    ));

    std::vector<InventoryItem> announced;
    announced.reserve(6U);

    for (Byte i = 1U; i <= 6U; ++i) {
        Hash256 hash{};
        hash.front() = i;

        announced.push_back(
            InventoryItem{
                .type = kInventoryBlock,
                .hash = hash,
            }
        );
    }

    assert(connected.session->send_command(
               "inv",
               serialize_inventory(announced)) ==
           PeerError::none);

    WireMessage request;
    assert(connected.session->receive_command(
               request) ==
           PeerError::none);
    assert(request.command == "getdata");

    const auto requested =
        parse_inventory(request.payload);

    assert(requested.has_value());
    assert(requested->size() == 4U);

    for (std::size_t i = 0U;
         i < requested->size();
         ++i) {
        assert((*requested)[i].type ==
               kInventoryBlock);
        assert((*requested)[i].hash ==
               announced[i].hash);
    }

    // Keep the socket open but intentionally never answer the block request.
    // The runtime must not let a responsive-looking peer pin block downloads
    // forever; the independent block-request deadline closes it.
    assert(wait_until(
        std::chrono::seconds(6),
        [&] {
            return runtime.status().peers == 0U;
        }
    ));

    connected.session->close();
    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(dir, ec);
}


void test_encrypted_stem_transaction_relay()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto server_dir =
        unique_dir("stem-runtime-server");
    const auto mirror_dir =
        unique_dir("stem-runtime-mirror");
    const std::uint64_t base_time =
        params.genesis.timestamp + 70'000U;
    const auto payout = payout_script(23U);

    const auto coin =
        prepare_chain_data(
            params,
            server_dir,
            mirror_dir,
            base_time,
            payout
        );

    NetworkRuntime server{
        params,
        server_dir
    };

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.wallet_enabled = false;
    config.enable_dandelion_relay = true;
    config.dandelion_fluff_percent = 0U;
    config.dandelion_embargo_min_seconds = 2U;
    config.dandelion_embargo_jitter_seconds = 0U;

    const auto started =
        server.start(config);
    assert(started.ok());
    assert(server.status().height ==
           std::optional<std::uint32_t>{105U});

    VersionMessage version;
    version.protocol_version =
        params.p2p_protocol_version;
    version.services =
        kServiceNetwork |
        kServiceEncryptedTransport |
        kServiceDandelionRelay;
    version.timestamp = base_time + 1'000U;
    version.nonce = 0x34123412ULL;
    version.start_height = 105U;
    version.listen_port = 0U;

    auto connected =
        connect_and_handshake(
            params,
            "127.0.0.1",
            server.status().listen_port,
            version,
            5'000U
        );

    assert(connected.ok());
    assert(connected.session->encrypted());

    const auto spend =
        make_spend(coin, 23U, 444U);
    const auto txid =
        transaction_id(spend);

    assert(connected.session->send_command(
               "stemtx",
               serialize_transaction_payload(spend)) ==
           PeerError::none);

    assert(wait_until(
        std::chrono::seconds(8),
        [&] {
            return server.has_mempool_transaction(
                txid
            );
        }
    ));

    connected.session->close();
    server.stop();

    std::error_code ec;
    std::filesystem::remove_all(server_dir, ec);
    std::filesystem::remove_all(mirror_dir, ec);
}


void test_repeated_partition_reconnect_converges_to_heavier_chain()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params = consensus::regtest_params();
    const auto left_dir = unique_dir("partition-soak-left");
    const auto right_dir = unique_dir("partition-soak-right");
    const auto payout = payout_script(31U);
    std::uint64_t now = params.genesis.timestamp + 100'000U;

    // Three independent partition/reconnect rounds. Each round begins from
    // an identical durable tip, mines competing branches while disconnected,
    // then reconnects and requires both runtimes to converge to the branch
    // with greater cumulative work.
    for (std::uint32_t round = 0U; round < 3U; ++round) {
        {
            NodeRuntime left{params, left_dir};
            NodeRuntime right{params, right_dir};
            assert(left.start_at(now).ok());
            assert(right.start_at(now).ok());
            assert(left.chain().tip_hash() ==
                   right.chain().tip_hash());

            for (std::uint32_t i = 0U; i < 2U; ++i) {
                const auto mined = left.mine_block_at(
                    payout, ++now, 4'096U);
                assert(mined.ok());
            }

            for (std::uint32_t i = 0U; i < 3U; ++i) {
                const auto mined = right.mine_block_at(
                    payout, ++now, 4'096U);
                assert(mined.ok());
            }

            assert(left.chain().tip_hash() !=
                   right.chain().tip_hash());
            assert(left.chain().cumulative_work() <
                   right.chain().cumulative_work());
        }

        NetworkRuntimeConfig right_config;
        right_config.bind_address = "127.0.0.1";
        right_config.listen_port = 0U;
        right_config.allow_local_peers = true;
        right_config.target_outbound = 0U;
        right_config.wallet_enabled = false;
        right_config.accept_poll_ms = 10U;
        right_config.io_timeout_ms = 5'000U;
        right_config.ping_interval_seconds = 30U;
        right_config.ping_timeout_seconds = 5U;

        NetworkRuntime right{params, right_dir};
        assert(right.start(right_config).ok());

        NetworkRuntimeConfig left_config = right_config;
        left_config.target_outbound = 1U;
        left_config.outbound_retry_seconds = 1U;
        left_config.reconnect_delay_seconds = 1U;
        left_config.bootstrap_peers.push_back(
            PeerAddress{
                .ipv4 = *parse_ipv4("127.0.0.1"),
                .port = right.status().listen_port,
                .services = 1U,
                .last_seen = now,
            });

        NetworkRuntime left{params, left_dir};
        assert(left.start(left_config).ok());

        assert(wait_until(
            std::chrono::seconds(20),
            [&] {
                const auto a = left.status();
                const auto b = right.status();
                return a.tip == b.tip &&
                       a.height == b.height &&
                       left.cumulative_work() ==
                           right.cumulative_work() &&
                       a.outbound_peers == 1U &&
                       b.peers >= 1U;
            }));

        const auto converged_tip = right.status().tip;
        const auto converged_height = right.status().height;
        assert(converged_tip.has_value());
        assert(converged_height.has_value());

        left.stop();
        right.stop();

        // Convergence must be durable, not merely in-memory network state.
        NodeRuntime left_restart{params, left_dir};
        NodeRuntime right_restart{params, right_dir};
        assert(left_restart.start_at(++now).ok());
        assert(right_restart.start_at(now).ok());
        assert(left_restart.chain().tip_hash() ==
               converged_tip);
        assert(right_restart.chain().tip_hash() ==
               converged_tip);
        assert(left_restart.chain().height() ==
               converged_height);
        assert(right_restart.chain().height() ==
               converged_height);
        assert(left_restart.chain().utxos().size() ==
               right_restart.chain().utxos().size());
    }

    std::error_code ec;
    std::filesystem::remove_all(left_dir, ec);
    std::filesystem::remove_all(right_dir, ec);
}

void test_initial_sync_does_not_lock_status_and_can_cancel()
{
    using namespace quintum;
    using namespace quintum::net;
    using namespace std::chrono_literals;
    const auto& params = consensus::regtest_params();
    const auto directory = unique_dir("initial-cancellation");
    PeerListener listener{params};
    assert(listener.listen("127.0.0.1", 0U) == PeerError::none);
    std::promise<void> requested, release;
    auto release_future = release.get_future();
    std::thread server([&] {
        VersionMessage remote;
        remote.protocol_version = params.p2p_protocol_version;
        remote.services = kServiceNetwork;
        remote.nonce = 0x726573706f6e64U;
        remote.start_height = 1U;
        remote.listen_port = listener.local_port();
        auto accepted = listener.accept_and_handshake(remote, 5'000U);
        assert(accepted.ok());
        WireMessage request;
        assert(accepted.session->receive_command(request) == PeerError::none);
        // Discovery now precedes header synchronization. Answer the address
        // request so this test can exercise cancellation during getheaders.
        if (request.command == "getaddr" || request.command == "getaddrv2") {
            WireMessage response;
            response.command = request.command == "getaddrv2" ? "addrv2" : "addr";
            response.payload = request.command == "getaddrv2"
                ? serialize_addresses_v2(std::span<const PeerAddress>{})
                : serialize_addresses(std::span<const PeerAddress>{});
            assert(accepted.session->send_command(response.command, response.payload) == PeerError::none);
            assert(accepted.session->receive_command(request) == PeerError::none);
        }
        assert(request.command == "getheaders");
        requested.set_value();
        release_future.wait();
        accepted.session->close();
    });
    NetworkRuntime runtime{params, directory};
    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.wallet_enabled = false;
    config.enable_nat_mapping = false;
    config.target_outbound = 1U;
    config.bootstrap_peers = {PeerAddress{
        .ipv4 = *parse_ipv4("127.0.0.1"), .port = listener.local_port(),
        .services = kServiceNetwork}};
    assert(runtime.start(config).ok());
    assert(requested.get_future().wait_for(5s) == std::future_status::ready);
    auto status = std::async(std::launch::async, [&] { return runtime.status(); });
    // The server deliberately withholds its response. Status must not wait
    // for that network operation or any initial header validation.
    assert(status.wait_for(500ms) == std::future_status::ready);
    assert(status.get().height == std::optional<std::uint32_t>{0U});
    auto stopped = std::async(std::launch::async, [&] { runtime.stop(); });
    assert(stopped.wait_for(2s) == std::future_status::ready);
    stopped.get();
    release.set_value();
    server.join();
    NodeRuntime restarted{params, directory};
    assert(restarted.start().ok());
    assert(restarted.chain().height() == std::optional<std::uint32_t>{0U});
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

} // namespace

int main()
{
    test_initial_sync_does_not_lock_status_and_can_cancel();
    test_status_polling_does_not_wait_for_initial_sync();
    test_default_listener_port_fallback();
    test_walletless_seed_runtime_creates_no_wallet();
    test_peer_message_flood_is_disconnected();
    test_malformed_ping_disconnects_peer();
    test_compact_block_resource_caps_are_fail_closed();
    test_continuous_runtime_sync_relay_reconnect();
    test_dandelion_three_node_relay_and_block_confirmation();
    test_higher_outbound_peer_updates_lower_inbound();
    test_chainwork_runtime_prefers_shorter_heavier_peer();
    test_reconnect_backoff_grows_and_caps();
    test_stalled_transaction_requests_are_bounded_and_expire();
    test_stalled_block_requests_are_bounded_and_expire();
    test_encrypted_stem_transaction_relay();
    test_repeated_partition_reconnect_converges_to_heavier_chain();
    return 0;
}
