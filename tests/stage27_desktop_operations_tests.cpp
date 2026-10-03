#include "consensus/chainparams.hpp"
#include "consensus/monetary.hpp"
#include "net/runtime.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <functional>
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
        ("quintum-stage27-" +
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

void test_runtime_mnemonic_recovery()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto source_dir =
        unique_dir("source");
    const auto recovered_dir =
        unique_dir("recovered");

    std::string phrase;

    {
        NetworkRuntime source{
            params,
            source_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "stage27-source";

        const auto started =
            source.start(
                std::move(config)
            );

        assert(started.ok());
        assert(source.verify_wallet_passphrase(
            "stage27-source"));
        assert(!source.verify_wallet_passphrase(
            "wrong-password"));

        const auto mnemonic =
            source.wallet_recovery_mnemonic();

        assert(mnemonic.has_value());
        phrase = *mnemonic;

        source.stop();
    }

    {
        NetworkRuntime recovered{
            params,
            recovered_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "stage27-recovered";
        config.wallet_recovery_mnemonic =
            phrase;

        const auto started =
            recovered.start(
                std::move(config)
            );

        assert(started.ok());
        assert(started.recovered_wallet);
        assert(started.wallet_recovery.ok());
        assert(recovered.verify_wallet_passphrase(
            "stage27-recovered"));
        assert(!recovered.verify_wallet_passphrase(
            "wrong-password"));

        const auto restored =
            recovered.wallet_recovery_mnemonic();

        assert(restored.has_value());
        assert(*restored == phrase);

        const auto status =
            recovered.status();

        assert(!status.peer_best_height);
        assert(!status.synchronizing);
        assert(status.sync_progress == 1.0);

        recovered.stop();
    }

    remove_tree(source_dir);
    remove_tree(recovered_dir);
}

void test_recovery_never_overwrites_existing_wallet()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto source_dir =
        unique_dir("recovery-source");
    const auto target_dir =
        unique_dir("recovery-target");

    std::string foreign_phrase;

    {
        NetworkRuntime source{
            params,
            source_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "source-password";

        assert(source.start(
                   std::move(config)).ok());

        const auto phrase =
            source.wallet_recovery_mnemonic();

        assert(phrase.has_value());
        foreign_phrase = *phrase;
        source.stop();
    }

    {
        NetworkRuntime original{
            params,
            target_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "original-password";

        assert(original.start(
                   std::move(config)).ok());
        original.stop();
    }

    {
        NetworkRuntime attempted{
            params,
            target_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "replacement-password";
        config.wallet_recovery_mnemonic =
            foreign_phrase;

        const auto started =
            attempted.start(
                std::move(config)
            );

        assert(!started.ok());
        assert(!started.recovered_wallet);
        assert(started.wallet_recovery.error ==
               wallet::WalletRecoveryError::
                   target_exists);
        attempted.stop();
    }

    {
        NetworkRuntime original{
            params,
            target_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "original-password";

        assert(original.start(
                   std::move(config)).ok());
        original.stop();
    }

    remove_tree(source_dir);
    remove_tree(target_dir);
}

void test_recovery_rebinds_orphaned_metadata()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto source_dir =
        unique_dir("metadata-source");
    const auto target_dir =
        unique_dir("metadata-target");

    std::string phrase;

    {
        NetworkRuntime source{
            params,
            source_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "metadata-source-password";

        assert(source.start(
                   std::move(config)).ok());

        const auto mnemonic =
            source.wallet_recovery_mnemonic();

        assert(mnemonic.has_value());
        phrase = *mnemonic;
        source.stop();
    }

    {
        NetworkRuntime old_wallet{
            params,
            target_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "old-wallet-password";

        assert(old_wallet.start(
                   std::move(config)).ok());

        const auto old_status =
            old_wallet.status();

        assert(!old_status.receive_address.empty());

        assert(old_wallet.set_address_label(
                   old_status.receive_address,
                   "Old wallet label") ==
               wallet::WalletMetadataError::none);

        old_wallet.stop();
    }

    std::error_code ec;
    assert(std::filesystem::remove(
        target_dir / "wallet.dat",
        ec));
    assert(!ec);
    assert(std::filesystem::exists(
        target_dir / "wallet_meta.dat"));

    {
        NetworkRuntime recovered{
            params,
            target_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "recovered-password";
        config.wallet_recovery_mnemonic =
            phrase;

        const auto started =
            recovered.start(
                std::move(config)
            );

        assert(started.ok());
        assert(started.recovered_wallet);
        assert(recovered.desktop_snapshot()
                   .address_book.empty());

        recovered.stop();
    }

    {
        NetworkRuntime reopened{
            params,
            target_dir
        };

        NetworkRuntimeConfig config;
        config.listen_port = 0U;
        config.target_outbound = 0U;
        config.wallet_passphrase =
            "recovered-password";

        const auto started =
            reopened.start(
                std::move(config)
            );

        assert(started.ok());
        assert(reopened.desktop_snapshot()
                   .address_book.empty());

        reopened.stop();
    }

    remove_tree(source_dir);
    remove_tree(target_dir);
}

void test_peer_height_drives_sync_status()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto server_dir =
        unique_dir("sync-server");
    const auto client_dir =
        unique_dir("sync-client");

    NetworkRuntime server{
        params,
        server_dir
    };

    NetworkRuntimeConfig server_config;
    server_config.bind_address =
        "127.0.0.1";
    server_config.listen_port = 0U;
    server_config.allow_local_peers = true;
    server_config.target_outbound = 0U;
    server_config.accept_poll_ms = 20U;
    server_config.io_timeout_ms = 5'000U;
    server_config.wallet_passphrase =
        "stage27-sync-server";

    assert(server.start(
               std::move(server_config)).ok());

    for (std::uint32_t i = 0U;
         i < 3U;
         ++i) {
        assert(server.mine_wallet_block(
                   4'096U).ok());
    }

    const auto server_status =
        server.status();

    assert(server_status.height ==
           std::optional<std::uint32_t>{3U});
    assert(server_status.listen_port != 0U);

    NetworkRuntime client{
        params,
        client_dir
    };

    NetworkRuntimeConfig client_config;
    client_config.bind_address =
        "127.0.0.1";
    client_config.listen_port = 0U;
    client_config.allow_local_peers = true;
    client_config.target_outbound = 1U;
    client_config.accept_poll_ms = 20U;
    client_config.io_timeout_ms = 5'000U;
    client_config.outbound_retry_seconds = 1U;
    client_config.reconnect_delay_seconds = 1U;
    client_config.wallet_passphrase =
        "stage27-sync-client";

    client_config.bootstrap_peers.push_back(
        PeerAddress{
            .ipv4 =
                *parse_ipv4("127.0.0.1"),
            .port =
                server_status.listen_port,
            .services = 1U,
        }
    );

    assert(client.start(
               std::move(client_config)).ok());

    assert(wait_until(
        std::chrono::seconds(8),
        [&] {
            const auto status =
                client.status();
            const auto desktop =
                client.desktop_snapshot();

            return status.height ==
                       std::optional<std::uint32_t>{3U} &&
                   status.peer_best_height &&
                   *status.peer_best_height >= 3U &&
                   !status.synchronizing &&
                   status.sync_progress == 1.0 &&
                   desktop.status.height ==
                       std::optional<std::uint32_t>{3U} &&
                   desktop.status.peer_best_height &&
                   *desktop.status.peer_best_height >= 3U &&
                   !desktop.status.synchronizing &&
                   desktop.status.sync_progress == 1.0;
        }
    ));

    client.stop();
    server.stop();

    remove_tree(client_dir);
    remove_tree(server_dir);
}

void test_wallet_mining_uses_owned_payout()
{
    using namespace quintum;
    using namespace quintum::net;

    const auto& params =
        consensus::regtest_params();
    const auto directory =
        unique_dir("mining");

    NetworkRuntime runtime{
        params,
        directory
    };

    NetworkRuntimeConfig config;
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.wallet_passphrase =
        "stage27-miner";

    assert(runtime.start(
               std::move(config)).ok());

    const auto before =
        runtime.status();

    assert(before.height ==
           std::optional<std::uint32_t>{0U});

    const auto mined =
        runtime.mine_wallet_block(
            4'096U
        );

    assert(mined.ok());
    assert(mined.mining.found());
    assert(mined.mining.attempts > 0U);

    const auto after =
        runtime.status();

    assert(after.height ==
           std::optional<std::uint32_t>{1U});
    assert(after.wallet_balance.immature ==
           consensus::kInitialSubsidy);

    runtime.stop();
    remove_tree(directory);
}

} // namespace

int main()
{
    test_runtime_mnemonic_recovery();
    test_recovery_never_overwrites_existing_wallet();
    test_recovery_rebinds_orphaned_metadata();
    test_peer_height_drives_sync_status();
    test_wallet_mining_uses_owned_payout();
    return 0;
}
