#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/runtime.hpp"
#include "node/node.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
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

} // namespace

int main()
{
    test_default_listener_port_fallback();
    test_walletless_seed_runtime_creates_no_wallet();
    test_peer_message_flood_is_disconnected();
    test_continuous_runtime_sync_relay_reconnect();
    test_higher_outbound_peer_updates_lower_inbound();
    return 0;
}
