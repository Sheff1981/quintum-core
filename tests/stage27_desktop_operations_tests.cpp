#include "consensus/chainparams.hpp"
#include "consensus/monetary.hpp"
#include "net/runtime.hpp"

#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <string_view>

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
    test_wallet_mining_uses_owned_payout();
    return 0;
}
