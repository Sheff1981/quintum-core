#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "node/node.hpp"
#include "net/runtime.hpp"
#include "wallet/address.hpp"
#include "wallet/fee_policy.hpp"
#include "wallet/wallet.hpp"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::filesystem::path unique_dir(
    std::string_view suffix)
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-stage22-" +
         std::string(suffix) + "-" +
         std::to_string(stamp));
}

quintum::crypto::PrivateKey key_from_scalar(
    quintum::Byte scalar)
{
    quintum::crypto::PrivateKey key{};
    key.back() = scalar;
    assert(quintum::crypto::is_valid_private_key(key));
    return key;
}

quintum::crypto::PublicKey public_key_from_scalar(
    quintum::Byte scalar)
{
    const auto public_key =
        quintum::crypto::derive_public_key(
            key_from_scalar(scalar)
        );

    assert(public_key.has_value());
    return *public_key;
}

quintum::Bytes payout_from_scalar(
    quintum::Byte scalar)
{
    return quintum::consensus::
        make_p2pk_locking_script(
            public_key_from_scalar(scalar)
        );
}

const quintum::wallet::WalletTransactionRecord*
find_history(
    const std::vector<
        quintum::wallet::WalletTransactionRecord>& history,
    const quintum::Hash256& txid)
{
    const auto it = std::find_if(
        history.begin(),
        history.end(),
        [&](const auto& record) {
            return record.txid == txid;
        }
    );

    return it == history.end()
        ? nullptr
        : &*it;
}

void test_fee_policy_math_and_mempool_estimate()
{
    using namespace quintum;
    using namespace quintum::wallet;

    assert(fee_for_size(
               0U,
               1'000U) ==
           std::optional<Amount>{0U});

    assert(fee_for_size(
               1U,
               1'000U) ==
           std::optional<Amount>{1U});

    assert(fee_for_size(
               1'000U,
               1'000U) ==
           std::optional<Amount>{1'000U});

    assert(fee_for_size(
               1'001U,
               1'000U) ==
           std::optional<Amount>{1'001U});

    assert(!fee_for_size(
        std::numeric_limits<std::size_t>::max(),
        consensus::kMaxMoney
    ));

    Mempool empty;
    assert(recommended_fee_rate(empty) ==
           kDefaultFeeRatePerKb);

    const auto chain_directory =
        unique_dir("fee-chain");
    const auto wallet_directory =
        unique_dir("fee-wallet");

    const auto& params =
        consensus::regtest_params();

    const std::uint64_t base_time =
        params.genesis.timestamp + 10'000U;

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(base_time).ok());

    Wallet wallet{
        params,
        wallet_directory
    };

    assert(wallet.start().ok());

    const auto imported =
        wallet.import_private_key(
            key_from_scalar(41U)
        );
    assert(imported.ok());

    const Bytes owned_payout =
        consensus::make_p2pk_locking_script(
            public_key_from_scalar(41U)
        );

    const Bytes external_payout =
        payout_from_scalar(42U);

    assert(node.mine_block_at(
               owned_payout,
               base_time + 1U,
               4'096U).ok());

    for (std::uint32_t height = 2U;
         height <= 100U;
         ++height) {
        assert(node.mine_block_at(
                   external_payout,
                   base_time + height,
                   4'096U).ok());
    }

    assert(wallet.sync(
               node.chain(),
               node.mempool()).ok());

    const std::string recipient =
        encode_address(
            consensus::Network::regtest,
            public_key_from_scalar(43U)
        );

    constexpr Amount fee{50'000U};

    const auto created =
        wallet.create_transaction(
            recipient,
            consensus::kAtomicUnitsPerCoin,
            fee,
            node.chain(),
            node.mempool()
        );

    assert(created.ok());

    const auto accepted =
        node.submit_transaction(
            created.transaction
        );

    assert(accepted.ok());

    const auto size =
        serialized_transaction_size(
            created.transaction
        );

    assert(size.has_value());
    assert(*size > 0U);

    const Amount expected_rate =
        static_cast<Amount>(
            (fee * 1'000U +
             static_cast<Amount>(*size) - 1U) /
            static_cast<Amount>(*size)
        );

    assert(recommended_fee_rate(
               node.mempool()) ==
           std::max(
               kDefaultFeeRatePerKb,
               expected_rate
           ));

    std::error_code ec;
    std::filesystem::remove_all(
        chain_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
}

void test_persistent_history_and_incremental_index()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto chain_directory =
        unique_dir("history-chain");
    const auto wallet_directory =
        unique_dir("history-wallet");

    const auto& params =
        consensus::regtest_params();

    const std::uint64_t base_time =
        params.genesis.timestamp + 20'000U;

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(base_time).ok());

    Hash256 funding_txid{};
    Hash256 spend_txid{};

    {
        Wallet wallet{
            params,
            wallet_directory
        };

        assert(wallet.start().ok());

        const auto imported =
            wallet.import_private_key(
                key_from_scalar(51U)
            );

        assert(imported.ok());

        const Bytes owned_payout =
            consensus::make_p2pk_locking_script(
                public_key_from_scalar(51U)
            );

        const Bytes external_payout =
            payout_from_scalar(52U);

        const auto first =
            node.mine_block_at(
                owned_payout,
                base_time + 1U,
                4'096U
            );

        assert(first.ok());
        funding_txid =
            transaction_id(
                first.block.transactions.front()
            );

        for (std::uint32_t height = 2U;
             height <= 100U;
             ++height) {
            assert(node.mine_block_at(
                       external_payout,
                       base_time + height,
                       4'096U).ok());
        }

        const auto first_sync =
            wallet.sync(
                node.chain(),
                node.mempool()
            );

        assert(first_sync.ok());
        assert(first_sync.index_rebuilt);
        assert(first_sync.blocks_scanned == 101U);

        const auto history =
            wallet.history();

        assert(history.size() == 1U);

        const auto* funding =
            find_history(
                history,
                funding_txid
            );

        assert(funding != nullptr);
        assert(funding->status ==
               WalletTransactionStatus::confirmed);
        assert(funding->coinbase);
        assert(funding->received ==
               consensus::kInitialSubsidy);
        assert(funding->spent == 0U);
        assert(!funding->fee.has_value());
        assert(funding->block_height ==
               std::optional<std::uint32_t>{1U});
        assert(funding->confirmations == 100U);

        const auto second_sync =
            wallet.sync(
                node.chain(),
                node.mempool()
            );

        assert(second_sync.ok());
        assert(!second_sync.index_rebuilt);
        assert(second_sync.blocks_scanned == 0U);
    }

    {
        Wallet reopened{
            params,
            wallet_directory
        };

        assert(reopened.start().ok());

        const auto sync =
            reopened.sync(
                node.chain(),
                node.mempool()
            );

        assert(sync.ok());
        assert(!sync.index_rebuilt);
        assert(sync.blocks_scanned == 0U);
        assert(reopened.history().size() == 1U);

        const std::string recipient =
            encode_address(
                consensus::Network::regtest,
                public_key_from_scalar(53U)
            );

        constexpr Amount amount{
            10U *
            consensus::kAtomicUnitsPerCoin
        };
        constexpr Amount fee{1'234U};

        const auto created =
            reopened.create_transaction(
                recipient,
                amount,
                fee,
                node.chain(),
                node.mempool()
            );

        assert(created.ok());
        spend_txid = created.txid;

        const auto submitted =
            node.submit_transaction(
                created.transaction
            );

        assert(submitted.ok());

        const auto pending_sync =
            reopened.sync(
                node.chain(),
                node.mempool()
            );

        assert(pending_sync.ok());
        assert(pending_sync.blocks_scanned == 0U);

        const auto pending_history =
            reopened.history();

        const auto* pending =
            find_history(
                pending_history,
                spend_txid
            );

        assert(pending != nullptr);
        assert(pending->status ==
               WalletTransactionStatus::unconfirmed);
        assert(!pending->coinbase);
        assert(pending->spent ==
               consensus::kInitialSubsidy);
        assert(pending->received ==
               created.change);
        assert(pending->fee ==
               std::optional<Amount>{fee});
        assert(!pending->block_height.has_value());
        assert(pending->confirmations == 0U);

        {
            Wallet persisted_history{
                params,
                wallet_directory
            };

            assert(persisted_history.start().ok());

            Mempool empty_after_restart;

            const auto persisted_sync =
                persisted_history.sync(
                    node.chain(),
                    empty_after_restart
                );

            assert(persisted_sync.ok());

            const auto persisted =
                persisted_history.history();

            const auto* inactive =
                find_history(
                    persisted,
                    spend_txid
                );

            assert(inactive != nullptr);
            assert(inactive->status ==
                   WalletTransactionStatus::inactive);
            assert(inactive->confirmations == 0U);
            assert(inactive->fee ==
                   std::optional<Amount>{fee});
        }

        const auto confirmed =
            node.mine_mempool_block_at(
                payout_from_scalar(54U),
                base_time + 101U,
                4'096U
            );

        assert(confirmed.ok());

        const auto confirmed_sync =
            reopened.sync(
                node.chain(),
                node.mempool()
            );

        assert(confirmed_sync.ok());
        assert(!confirmed_sync.index_rebuilt);
        assert(confirmed_sync.blocks_scanned == 1U);

        const auto confirmed_history =
            reopened.history();

        const auto* confirmed_record =
            find_history(
                confirmed_history,
                spend_txid
            );

        assert(confirmed_record != nullptr);
        assert(confirmed_record->status ==
               WalletTransactionStatus::confirmed);
        assert(confirmed_record->fee ==
               std::optional<Amount>{fee});
        assert(confirmed_record->block_height ==
               std::optional<std::uint32_t>{101U});
        assert(confirmed_record->confirmations == 1U);
    }

    {
        Wallet reopened{
            params,
            wallet_directory
        };

        assert(reopened.start().ok());

        const auto sync =
            reopened.sync(
                node.chain(),
                node.mempool()
            );

        assert(sync.ok());
        assert(!sync.index_rebuilt);
        assert(sync.blocks_scanned == 0U);

        const auto history =
            reopened.history();

        assert(find_history(
                   history,
                   funding_txid) != nullptr);

        const auto* spend =
            find_history(
                history,
                spend_txid
            );

        assert(spend != nullptr);
        assert(spend->status ==
               WalletTransactionStatus::confirmed);
        assert(spend->confirmations == 1U);
    }

    // The index contains no secrets and is fully derivable.
    // A damaged cache must not make the wallet unusable.
    {
        const auto state_path =
            wallet_directory /
            "wallet_state.dat";

        std::fstream file(
            state_path,
            std::ios::binary |
                std::ios::in |
                std::ios::out
        );

        assert(file.good());

        file.seekp(0, std::ios::end);
        const auto size = file.tellp();
        assert(size > 16);

        file.seekg(12, std::ios::beg);
        char value{0};
        file.read(&value, 1);
        assert(file.good());

        value = static_cast<char>(
            static_cast<unsigned char>(value) ^
            0x01U
        );

        file.seekp(12, std::ios::beg);
        file.write(&value, 1);
        assert(file.good());
    }

    {
        Wallet recovered_index{
            params,
            wallet_directory
        };

        assert(recovered_index.start().ok());

        const auto sync =
            recovered_index.sync(
                node.chain(),
                node.mempool()
            );

        assert(sync.ok());
        assert(sync.index_rebuilt);
        assert(sync.blocks_scanned == 102U);

        const auto history =
            recovered_index.history();

        assert(find_history(
                   history,
                   funding_txid) != nullptr);
        assert(find_history(
                   history,
                   spend_txid) != nullptr);
    }

    std::error_code ec;
    std::filesystem::remove_all(
        chain_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
}

void test_network_runtime_exposes_wallet_history_and_fee_rate()
{
    using namespace quintum;
    using namespace quintum::net;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("runtime-api");

    const auto& params =
        consensus::regtest_params();

    NetworkRuntime runtime{
        params,
        directory
    };

    NetworkRuntimeConfig config;
    config.bind_address = "127.0.0.1";
    config.listen_port = 0U;
    config.target_outbound = 0U;
    config.accept_poll_ms = 10U;

    const auto started =
        runtime.start(config);

    assert(started.ok());

    const auto initial =
        runtime.status();

    assert(initial.recommended_fee_rate_per_kb ==
           kDefaultFeeRatePerKb);

    const auto decoded =
        decode_address(
            consensus::Network::regtest,
            initial.receive_address
        );

    assert(decoded.ok());

    const Bytes payout =
        consensus::make_p2pk_locking_script(
            decoded.public_key
        );

    const auto mined =
        runtime.mine_mempool_block_at(
            payout,
            params.genesis.timestamp + 30'000U,
            4'096U
        );

    assert(mined.ok());

    const auto history =
        runtime.wallet_history();

    assert(history.size() == 1U);
    assert(history.front().status ==
           WalletTransactionStatus::confirmed);
    assert(history.front().coinbase);
    assert(history.front().received ==
           consensus::kInitialSubsidy);

    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_wallet_index_rebuilds_after_reorg()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto chain_a_directory =
        unique_dir("reorg-a");
    const auto chain_b_directory =
        unique_dir("reorg-b");
    const auto wallet_directory =
        unique_dir("reorg-wallet");

    const auto& params =
        consensus::regtest_params();

    const std::uint64_t base_time =
        params.genesis.timestamp + 40'000U;

    NodeRuntime node_a{
        params,
        chain_a_directory
    };
    NodeRuntime node_b{
        params,
        chain_b_directory
    };

    assert(node_a.start_at(base_time).ok());
    assert(node_b.start_at(base_time).ok());

    Wallet wallet{
        params,
        wallet_directory
    };

    assert(wallet.start().ok());

    const auto imported =
        wallet.import_private_key(
            key_from_scalar(61U)
        );
    assert(imported.ok());

    const Bytes owned_payout =
        consensus::make_p2pk_locking_script(
            public_key_from_scalar(61U)
        );

    const auto a1 =
        node_a.mine_block_at(
            owned_payout,
            base_time + 1U,
            4'096U
        );
    assert(a1.ok());

    const Hash256 abandoned_txid =
        transaction_id(
            a1.block.transactions.front()
        );

    const auto a2 =
        node_a.mine_block_at(
            payout_from_scalar(62U),
            base_time + 2U,
            4'096U
        );
    assert(a2.ok());

    const auto indexed =
        wallet.sync(
            node_a.chain(),
            node_a.mempool()
        );

    assert(indexed.ok());
    assert(indexed.index_rebuilt);
    assert(find_history(
               wallet.history(),
               abandoned_txid) != nullptr);

    std::vector<Block> stronger_branch;

    for (std::uint32_t height = 1U;
         height <= 3U;
         ++height) {
        const auto mined =
            node_b.mine_block_at(
                payout_from_scalar(63U),
                base_time +
                    10U +
                    static_cast<std::uint64_t>(
                        height),
                4'096U
            );

        assert(mined.ok());
        stronger_branch.push_back(
            mined.block
        );
    }

    for (std::size_t i = 0U;
         i < stronger_branch.size();
         ++i) {
        const auto submitted =
            node_a.submit_block_at(
                stronger_branch[i],
                base_time + 20U +
                    static_cast<std::uint64_t>(i)
            );

        assert(submitted.ok());
    }

    assert(node_a.chain().height() ==
           std::optional<std::uint32_t>{3U});
    assert(node_a.chain().tip_hash() ==
           node_b.chain().tip_hash());

    const auto after_reorg =
        wallet.sync(
            node_a.chain(),
            node_a.mempool()
        );

    assert(after_reorg.ok());
    assert(after_reorg.index_rebuilt);
    assert(after_reorg.blocks_scanned == 4U);
    assert(find_history(
               wallet.history(),
               abandoned_txid) == nullptr);
    assert(wallet.balance() ==
           WalletBalance{});

    std::error_code ec;
    std::filesystem::remove_all(
        chain_a_directory,
        ec
    );
    std::filesystem::remove_all(
        chain_b_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
}

void test_imported_key_invalidates_persisted_index_across_restart()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto chain_directory =
        unique_dir("import-crash-chain");
    const auto wallet_directory =
        unique_dir("import-crash-wallet");

    const auto& params =
        consensus::regtest_params();

    const std::uint64_t base_time =
        params.genesis.timestamp + 50'000U;

    NodeRuntime node{
        params,
        chain_directory
    };

    assert(node.start_at(base_time).ok());

    const Bytes imported_payout =
        consensus::make_p2pk_locking_script(
            public_key_from_scalar(71U)
        );

    const auto historical =
        node.mine_block_at(
            imported_payout,
            base_time + 1U,
            4'096U
        );

    assert(historical.ok());

    const Hash256 historical_txid =
        transaction_id(
            historical.block.transactions.front()
        );

    {
        Wallet wallet{
            params,
            wallet_directory
        };

        assert(wallet.start().ok());

        const auto baseline =
            wallet.sync(
                node.chain(),
                node.mempool()
            );

        assert(baseline.ok());
        assert(baseline.index_rebuilt);
        assert(wallet.history().empty());

        const auto imported =
            wallet.import_private_key(
                key_from_scalar(71U)
            );

        assert(imported.ok());

        // Simulate an application exit immediately after import:
        // there is deliberately no wallet.sync() here.
    }

    {
        Wallet restarted{
            params,
            wallet_directory
        };

        assert(restarted.start().ok());

        const auto sync =
            restarted.sync(
                node.chain(),
                node.mempool()
            );

        assert(sync.ok());
        assert(sync.index_rebuilt);
        assert(sync.blocks_scanned == 2U);

        const auto history =
            restarted.history();

        const auto* record =
            find_history(
                history,
                historical_txid
            );

        assert(record != nullptr);
        assert(record->received ==
               consensus::kInitialSubsidy);
    }

    std::error_code ec;
    std::filesystem::remove_all(
        chain_directory,
        ec
    );
    std::filesystem::remove_all(
        wallet_directory,
        ec
    );
}

} // namespace

int main()
{
    test_fee_policy_math_and_mempool_estimate();
    test_persistent_history_and_incremental_index();
    test_network_runtime_exposes_wallet_history_and_fee_rate();
    test_wallet_index_rebuilds_after_reorg();
    test_imported_key_invalidates_persisted_index_across_restart();
    return 0;
}
