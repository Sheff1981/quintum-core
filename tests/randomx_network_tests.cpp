#include "consensus/chainparams.hpp"
#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "net/runtime.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
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
        ("quintum-randomx-network-" +
         std::string{suffix} + "-" +
         std::to_string(stamp));
}

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

quintum::Bytes payout_script()
{
    quintum::crypto::PrivateKey key{};
    key.back() = 0x2aU;

    const auto public_key =
        quintum::crypto::derive_public_key(key);

    assert(public_key.has_value());

    return quintum::consensus::
        make_p2pk_locking_script(
            *public_key
        );
}

quintum::consensus::ChainParams
isolated_randomx_params()
{
    auto params =
        quintum::consensus::
            randomx_testnet_params();

    // Keep the exact public RandomX consensus rules and Genesis,
    // but suppress public bootstrap during this deterministic local
    // integration test.
    params.network =
        quintum::consensus::Network::regtest;
    params.name = "randomx-network-test";
    params.message_start =
        {0xd1U, 0x6aU, 0x4fU, 0x93U};
    params.p2p_port = 0U;
    params.rpc_port = 0U;

    return params;
}

void test_randomx_walletless_p2p_relay_and_restart()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto params =
        isolated_randomx_params();

    const auto server_dir =
        unique_dir("server");
    const auto client_dir =
        unique_dir("client");

    NetworkRuntimeConfig server_config;
    server_config.bind_address =
        "127.0.0.1";
    server_config.listen_port = 0U;
    server_config.allow_local_peers = true;
    server_config.target_outbound = 0U;
    server_config.wallet_enabled = false;
    server_config.accept_poll_ms = 20U;
    server_config.io_timeout_ms = 10'000U;
    server_config.outbound_retry_seconds = 1U;
    server_config.reconnect_delay_seconds = 1U;

    NetworkRuntime server{
        params,
        server_dir
    };

    const auto server_start =
        server.start(server_config);

    assert(server_start.ok());
    assert(server.status().height ==
           std::optional<std::uint32_t>{0U});
    assert(!server.status().wallet_enabled);

    NetworkRuntimeConfig client_config =
        server_config;
    client_config.target_outbound = 1U;
    client_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 =
                *parse_ipv4("127.0.0.1"),
            .port =
                server.status().listen_port,
            .services = 1U,
            .last_seen =
                params.genesis.timestamp,
        }
    );

    NetworkRuntime client{
        params,
        client_dir
    };

    const auto client_start =
        client.start(client_config);

    assert(client_start.ok());
    assert(!client.status().wallet_enabled);

    assert(wait_until(
        std::chrono::seconds(20),
        [&] {
            return client.status().
                       outbound_peers == 1U &&
                   server.status().peers >= 1U;
        }
    ));

    // Protocol v2 lets the bootstrap node learn the inbound peer's
    // socket-observed IPv4 plus its claimed listening port.
    assert(wait_until(
        std::chrono::seconds(5),
        [&] {
            return server.status().
                       known_addresses >= 1U;
        }
    ));

    const auto peer_b_dir =
        unique_dir("peer-b");

    NetworkRuntimeConfig peer_b_config =
        server_config;
    peer_b_config.target_outbound = 2U;
    peer_b_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 =
                *parse_ipv4("127.0.0.1"),
            .port =
                server.status().listen_port,
            .services = 1U,
            .last_seen =
                params.genesis.timestamp,
        }
    );

    NetworkRuntime peer_b{
        params,
        peer_b_dir
    };

    const auto peer_b_start =
        peer_b.start(peer_b_config);

    assert(peer_b_start.ok());

    // Peer B first connects to the seed, learns Peer A from getaddr,
    // then opens a direct second outbound connection to Peer A.
    assert(wait_until(
        std::chrono::seconds(30),
        [&] {
            return peer_b.status().
                       outbound_peers >= 2U &&
                   client.status().peers >= 2U;
        }
    ));

    server.stop();

    // The bootstrap node is gone. The two ordinary nodes must remain
    // connected directly.
    assert(wait_until(
        std::chrono::seconds(10),
        [&] {
            return client.status().peers >= 1U &&
                   peer_b.status().peers >= 1U;
        }
    ));

    const auto mined =
        client.mine_mempool_block_at(
            payout_script(),
            params.genesis.timestamp +
                params.pow.target_spacing_seconds,
            20'000U
        );

    assert(mined.ok());
    assert(mined.height == 1U);
    assert(mined.block.transactions.size() == 1U);

    const auto& coinbase =
        mined.block.transactions.front();

    assert(coinbase.outputs.size() == 2U);
    assert(
        coinbase.outputs[0].value ==
        consensus::randomx_miner_subsidy(1U)
    );
    assert(
        coinbase.outputs[1].value ==
        consensus::randomx_founder_subsidy(1U)
    );

    const auto founder_key =
        consensus::parse_p2pk_locking_script(
            coinbase.outputs[1].locking_script
        );

    assert(founder_key.has_value());
    assert(*founder_key ==
           params.monetary.founder_public_key);

    assert(wait_until(
        std::chrono::seconds(30),
        [&] {
            const auto client_status =
                client.status();
            const auto peer_b_status =
                peer_b.status();

            return peer_b_status.height ==
                       std::optional<std::uint32_t>{1U} &&
                   peer_b_status.tip ==
                       client_status.tip;
        }
    ));

    const auto surviving_tip =
        client.status().tip;

    peer_b.stop();
    client.stop();

    assert(!std::filesystem::exists(
        client_dir / "wallet.dat"
    ));
    assert(!std::filesystem::exists(
        client_dir / "wallet_state.dat"
    ));
    assert(!std::filesystem::exists(
        client_dir / "wallet_meta.dat"
    ));

    NetworkRuntime restarted_client{
        params,
        peer_b_dir
    };

    auto restart_config =
        server_config;

    const auto restarted =
        restarted_client.start(
            restart_config
        );

    assert(restarted.ok());
    assert(restarted_client.status().height ==
           std::optional<std::uint32_t>{1U});
    assert(restarted_client.status().tip ==
           surviving_tip);
    assert(!restarted_client.status().
               wallet_enabled);

    restarted_client.stop();

    assert(!std::filesystem::exists(
        server_dir / "wallet.dat"
    ));
    assert(!std::filesystem::exists(
        server_dir / "wallet_state.dat"
    ));
    assert(!std::filesystem::exists(
        server_dir / "wallet_meta.dat"
    ));

    std::error_code ec;
    std::filesystem::remove_all(
        server_dir,
        ec
    );
    std::filesystem::remove_all(
        client_dir,
        ec
    );
    std::filesystem::remove_all(
        peer_b_dir,
        ec
    );
}

} // namespace

int main()
{
    test_randomx_walletless_p2p_relay_and_restart();
    return 0;
}
