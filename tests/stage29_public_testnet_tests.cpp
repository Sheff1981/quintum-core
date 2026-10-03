#include "net/runtime.hpp"

#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
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
        ("quintum-stage29-" +
         std::string{suffix} + "-" +
         std::to_string(stamp));
}

void remove_tree(
    const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::remove_all(path, ec);
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

quintum::net::NetworkRuntimeConfig isolated_config(
    std::string password)
{
    quintum::net::NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.allow_local_peers = true;
    config.target_outbound = 0U;
    config.accept_poll_ms = 20U;
    config.io_timeout_ms = 5'000U;
    config.outbound_retry_seconds = 1U;
    config.reconnect_delay_seconds = 1U;
    config.ping_interval_seconds = 1U;
    config.ping_timeout_seconds = 4U;
    config.wallet_passphrase = std::move(password);
    return config;
}

quintum::net::PeerAddress loopback_peer(
    std::uint16_t port)
{
    using namespace quintum::net;

    const auto ip = parse_ipv4("127.0.0.1");
    assert(ip.has_value());

    return PeerAddress{
        .ipv4 = *ip,
        .port = port,
        .services = 1U,
    };
}

void mine_blocks(
    quintum::net::NetworkRuntime& runtime,
    std::uint32_t count)
{
    for (std::uint32_t i = 0U; i < count; ++i) {
        const auto mined =
            runtime.mine_wallet_block(4'096U);
        assert(mined.ok());
    }
}

void test_three_node_partition_reorg_reconnect_restart()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();

    const auto a_dir = unique_dir("node-a");
    const auto b_dir = unique_dir("node-b");
    const auto c_dir = unique_dir("node-c");

    const std::string a_password{"stage29-a"};
    const std::string b_password{"stage29-b"};
    const std::string c_password{"stage29-c"};

    std::uint16_t first_a_port{0U};

    // Establish a common chain on three independent runtimes.
    {
        NetworkRuntime a{params, a_dir};
        auto a_config =
            isolated_config(a_password);

        assert(a.start(
                   std::move(a_config)).ok());

        first_a_port = a.status().listen_port;
        assert(first_a_port != 0U);

        mine_blocks(a, 3U);

        NetworkRuntime b{params, b_dir};
        auto b_config =
            isolated_config(b_password);
        b_config.target_outbound = 1U;
        b_config.bootstrap_peers.push_back(
            loopback_peer(first_a_port)
        );

        NetworkRuntime c{params, c_dir};
        auto c_config =
            isolated_config(c_password);
        c_config.target_outbound = 1U;
        c_config.bootstrap_peers.push_back(
            loopback_peer(first_a_port)
        );

        assert(b.start(
                   std::move(b_config)).ok());
        assert(c.start(
                   std::move(c_config)).ok());

        assert(wait_until(
            std::chrono::seconds(15),
            [&] {
                const auto a_status = a.status();
                const auto b_status = b.status();
                const auto c_status = c.status();

                return a_status.height ==
                           std::optional<std::uint32_t>{3U} &&
                       b_status.height == a_status.height &&
                       c_status.height == a_status.height &&
                       b_status.tip == a_status.tip &&
                       c_status.tip == a_status.tip &&
                       b_status.outbound_peers == 1U &&
                       c_status.outbound_peers == 1U;
            }
        ));

        c.stop();
        b.stop();
        a.stop();
    }

    Hash256 short_tip{};
    Hash256 heavy_tip{};

    // Partition the network and mine competing branches from height 3.
    {
        NetworkRuntime a{params, a_dir};
        auto config =
            isolated_config(a_password);
        assert(a.start(std::move(config)).ok());
        assert(a.status().height ==
               std::optional<std::uint32_t>{3U});

        mine_blocks(a, 2U);
        assert(a.status().height ==
               std::optional<std::uint32_t>{5U});
        assert(a.status().tip.has_value());
        short_tip = *a.status().tip;
        a.stop();
    }

    std::uint16_t b_port{0U};

    NetworkRuntime b{params, b_dir};
    {
        auto config =
            isolated_config(b_password);
        assert(b.start(std::move(config)).ok());
        assert(b.status().height ==
               std::optional<std::uint32_t>{3U});

        mine_blocks(b, 4U);
        assert(b.status().height ==
               std::optional<std::uint32_t>{7U});
        assert(b.status().tip.has_value());
        heavy_tip = *b.status().tip;
        assert(heavy_tip != short_tip);

        b_port = b.status().listen_port;
        assert(b_port != 0U);
    }

    // The shorter partition reconnects outbound to the heavier peer.
    // Headers-first sync must perform a real reorg to the heavier branch.
    NetworkRuntime a{params, a_dir};
    {
        auto config =
            isolated_config(a_password);
        config.target_outbound = 1U;
        config.bootstrap_peers.push_back(
            loopback_peer(b_port)
        );

        assert(a.start(std::move(config)).ok());

        assert(wait_until(
            std::chrono::seconds(20),
            [&] {
                const auto status = a.status();
                return status.height ==
                           std::optional<std::uint32_t>{7U} &&
                       status.tip ==
                           std::optional<Hash256>{heavy_tip} &&
                       status.outbound_peers == 1U;
            }
        ));
    }

    // A third node, previously stopped at the common ancestor, catches up
    // through A. This forms B <-> A <-> C rather than one direct pair.
    {
        NetworkRuntime c{params, c_dir};
        auto config =
            isolated_config(c_password);
        config.target_outbound = 1U;
        config.bootstrap_peers.push_back(
            loopback_peer(a.status().listen_port)
        );

        assert(c.start(std::move(config)).ok());

        assert(wait_until(
            std::chrono::seconds(20),
            [&] {
                const auto status = c.status();
                return status.height ==
                           std::optional<std::uint32_t>{7U} &&
                       status.tip ==
                           std::optional<Hash256>{heavy_tip};
            }
        ));

        c.stop();
    }

    // Restart C without supplying any bootstrap address. peers.dat must be
    // sufficient for automatic reconnect; no manual IP is re-entered.
    NetworkRuntime c_restarted{params, c_dir};
    {
        auto config =
            isolated_config(c_password);
        config.target_outbound = 1U;

        assert(c_restarted.start(
                   std::move(config)).ok());

        assert(wait_until(
            std::chrono::seconds(20),
            [&] {
                const auto status =
                    c_restarted.status();
                return status.outbound_peers == 1U &&
                       status.height ==
                           std::optional<std::uint32_t>{7U} &&
                       status.tip ==
                           std::optional<Hash256>{heavy_tip};
            }
        ));
    }

    // A new block created at one edge must relay over two hops B -> A -> C.
    mine_blocks(b, 1U);

    Hash256 final_tip{};
    assert(wait_until(
        std::chrono::seconds(20),
        [&] {
            const auto b_status = b.status();
            const auto a_status = a.status();
            const auto c_status =
                c_restarted.status();

            return b_status.height ==
                       std::optional<std::uint32_t>{8U} &&
                   a_status.height == b_status.height &&
                   c_status.height == b_status.height &&
                   a_status.tip == b_status.tip &&
                   c_status.tip == b_status.tip;
        }
    ));

    assert(b.status().tip.has_value());
    final_tip = *b.status().tip;

    c_restarted.stop();
    a.stop();
    b.stop();

    // All three persistent chainstates must reopen on the converged tip.
    for (const auto* item :
         {&a_dir, &b_dir, &c_dir}) {
        NetworkRuntime restarted{
            params,
            *item
        };

        std::string password =
            item == &a_dir
                ? a_password
                : (item == &b_dir
                       ? b_password
                       : c_password);

        auto config =
            isolated_config(
                std::move(password)
            );

        assert(restarted.start(
                   std::move(config)).ok());

        const auto status =
            restarted.status();

        assert(status.height ==
               std::optional<std::uint32_t>{8U});
        assert(status.tip ==
               std::optional<Hash256>{final_tip});

        restarted.stop();
    }

    remove_tree(a_dir);
    remove_tree(b_dir);
    remove_tree(c_dir);
}


void test_real_testnet_params_bootstrap_and_peer_store()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::testnet_params();

    const auto seed_dir =
        unique_dir("testnet-seed");
    const auto peer_dir =
        unique_dir("testnet-peer");

    NetworkRuntime seed{params, seed_dir};
    auto seed_config =
        isolated_config("stage29-testnet-seed");
    assert(seed.start(
               std::move(seed_config)).ok());

    const auto seed_status = seed.status();
    assert(seed_status.height ==
           std::optional<std::uint32_t>{0U});
    assert(seed_status.tip ==
           std::optional<Hash256>{
               params.genesis.hash});
    assert(seed_status.listen_port != 0U);

    {
        NetworkRuntime peer{params, peer_dir};
        auto peer_config =
            isolated_config("stage29-testnet-peer");
        peer_config.target_outbound = 1U;
        peer_config.bootstrap_peers.push_back(
            loopback_peer(seed_status.listen_port)
        );

        assert(peer.start(
                   std::move(peer_config)).ok());

        assert(wait_until(
            std::chrono::seconds(15),
            [&] {
                const auto status = peer.status();
                return status.outbound_peers == 1U &&
                       status.height ==
                           std::optional<std::uint32_t>{0U} &&
                       status.tip ==
                           std::optional<Hash256>{
                               params.genesis.hash} &&
                       status.known_addresses >= 1U;
            }
        ));

        peer.stop();
    }

    // Prove the first bootstrap survives restart in peers.dat.
    {
        NetworkRuntime peer{params, peer_dir};
        auto restart_config =
            isolated_config("stage29-testnet-peer");
        restart_config.target_outbound = 1U;

        assert(peer.start(
                   std::move(restart_config)).ok());

        assert(wait_until(
            std::chrono::seconds(15),
            [&] {
                const auto status = peer.status();
                return status.outbound_peers == 1U &&
                       status.tip ==
                           std::optional<Hash256>{
                               params.genesis.hash};
            }
        ));

        peer.stop();
    }

    seed.stop();

    remove_tree(peer_dir);
    remove_tree(seed_dir);
}

} // namespace

int main()
{
    test_real_testnet_params_bootstrap_and_peer_store();
    test_three_node_partition_reorg_reconnect_restart();
    return 0;
}
