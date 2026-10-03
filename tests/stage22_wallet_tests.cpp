#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "node/node.hpp"
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

} // namespace

int main()
{
    test_fee_policy_math_and_mempool_estimate();
    test_persistent_history_and_incremental_index();
    return 0;
}
