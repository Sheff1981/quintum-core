#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "node/node.hpp"
#include "net/runtime.hpp"
#include "policy/fees.hpp"
#include "wallet/address.hpp"
#include "wallet/fee_policy.hpp"
#include "wallet/wallet.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <cstdint>
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
        ("quintum-stage24-" +
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
    const auto key =
        quintum::crypto::derive_public_key(
            key_from_scalar(scalar)
        );

    assert(key.has_value());
    return *key;
}

quintum::Bytes payout_from_scalar(
    quintum::Byte scalar)
{
    return quintum::consensus::
        make_p2pk_locking_script(
            public_key_from_scalar(scalar)
        );
}

struct MatureCoin {
    quintum::Hash256 txid{};
    quintum::TxOutput output{};
};

MatureCoin mine_mature_coin(
    quintum::NodeRuntime& node,
    const quintum::Bytes& payout,
    std::uint64_t base_time)
{
    MatureCoin coin;

    for (std::uint32_t height = 1U;
         height <= 100U;
         ++height) {
        const auto mined =
            node.mine_block_at(
                payout,
                base_time +
                    static_cast<std::uint64_t>(
                        height
                    ),
                4'096U
            );

        assert(mined.ok());

        if (height == 1U) {
            coin.txid =
                quintum::transaction_id(
                    mined.block.transactions.front()
                );
            coin.output =
                mined.block.transactions.front()
                    .outputs.front();
        }
    }

    return coin;
}

quintum::Transaction make_spend(
    const MatureCoin& coin,
    quintum::Byte scalar,
    quintum::Amount fee)
{
    using namespace quintum;

    assert(coin.output.value > fee);

    Transaction tx;
    tx.inputs.push_back(
        TxInput{
            .previous_output = OutPoint{
                .txid = coin.txid,
                .index = 0U,
            },
        }
    );

    tx.outputs.push_back(
        TxOutput{
            .value = coin.output.value - fee,
            .locking_script =
                payout_from_scalar(40U),
        }
    );

    assert(consensus::sign_p2pk_input(
               tx,
               0U,
               coin.output,
               key_from_scalar(scalar)) ==
           consensus::InputAuthError::none);

    return tx;
}

void test_min_relay_fee_rejects_underpriced_transaction()
{
    using namespace quintum;

    const auto directory =
        unique_dir("relay");
    const auto& params =
        consensus::regtest_params();
    const std::uint64_t base_time =
        params.genesis.timestamp + 80'000U;

    NodeRuntime node{
        params,
        directory
    };

    assert(node.start_at(base_time).ok());

    const auto coin =
        mine_mature_coin(
            node,
            payout_from_scalar(1U),
            base_time
        );

    auto low_fee =
        make_spend(
            coin,
            1U,
            1U
        );

    const auto size =
        serialized_transaction_size(
            low_fee
        );
    assert(size.has_value());

    const auto required =
        policy::fee_for_size(
            *size,
            policy::kDefaultMinRelayFeeRatePerKb
        );
    assert(required.has_value());
    assert(*required > 1U);

    const auto rejected =
        node.submit_transaction(
            low_fee
        );

    assert(!rejected.ok());
    assert(rejected.mempool.error ==
           MempoolError::fee_below_minimum);
    assert(rejected.mempool.fee == 1U);
    assert(rejected.mempool.required_fee ==
           *required);
    assert(node.mempool().size() == 0U);

    auto exact =
        make_spend(
            coin,
            1U,
            *required
        );

    const auto accepted =
        node.submit_transaction(
            exact
        );

    assert(accepted.ok());
    assert(accepted.mempool.fee ==
           *required);
    assert(accepted.mempool.required_fee ==
           *required);

    // Relay/mempool fee is policy only. The same otherwise-valid
    // low-fee transaction remains valid inside a mined block.
    const std::array<Transaction, 1> direct_block_tx{
        low_fee
    };

    const auto mined =
        node.mine_block_at(
            payout_from_scalar(70U),
            base_time + 1'000U,
            4'096U,
            direct_block_tx
        );

    assert(mined.ok());
    assert(mined.total_fees == 1U);
    assert(node.mempool().size() == 0U);

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_recommended_rate_never_below_relay_floor()
{
    using namespace quintum;
    using namespace quintum::wallet;

    Mempool mempool{
        MempoolPolicy{
            .min_relay_fee_rate_per_kb =
                5'000U,
        }
    };

    assert(mempool.min_relay_fee_rate_per_kb() ==
           5'000U);
    assert(recommended_fee_rate(mempool) ==
           5'000U);
}

void test_wallet_auto_fee_quotes_and_creates_exact_fee()
{
    using namespace quintum;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("wallet-auto");
    const auto& params =
        consensus::regtest_params();
    const std::uint64_t base_time =
        params.genesis.timestamp + 90'000U;

    NodeRuntime node{
        params,
        directory
    };
    assert(node.start_at(base_time).ok());

    Wallet wallet{
        params,
        directory
    };

    const auto started =
        wallet.start(
            "stage24-password"
        );
    assert(started.ok());

    const auto decoded =
        decode_address(
            params.network,
            started.receive_address
        );
    assert(decoded.ok());

    const auto payout =
        consensus::make_p2pk_locking_script(
            decoded.public_key
        );

    (void)mine_mature_coin(
        node,
        payout,
        base_time
    );

    const auto synced =
        wallet.sync(
            node.chain(),
            node.mempool()
        );
    assert(synced.ok());
    assert(synced.balance.available > 0U);

    const std::string destination =
        encode_address(
            params.network,
            public_key_from_scalar(55U)
        );

    const Amount amount =
        consensus::block_subsidy(1U) / 2U;

    const auto quote =
        wallet.quote_auto_fee(
            destination,
            amount,
            node.chain(),
            node.mempool()
        );

    assert(quote.ok());
    assert(quote.fee_rate_per_kb >=
           node.mempool()
               .min_relay_fee_rate_per_kb());
    assert(quote.serialized_size > 0U);
    assert(quote.fee > 0U);
    assert(quote.change > 0U);

    const auto expected_fee =
        policy::fee_for_size(
            quote.serialized_size,
            quote.fee_rate_per_kb
        );
    assert(expected_fee.has_value());
    assert(quote.fee == *expected_fee);

    const auto created =
        wallet.create_transaction_auto_fee(
            destination,
            amount,
            node.chain(),
            node.mempool()
        );

    assert(created.ok());
    assert(created.fee == quote.fee);
    assert(created.fee_rate_per_kb ==
           quote.fee_rate_per_kb);
    assert(created.serialized_size ==
           quote.serialized_size);

    const auto actual_size =
        serialized_transaction_size(
            created.transaction
        );
    assert(actual_size.has_value());
    assert(*actual_size ==
           created.serialized_size);

    const auto accepted =
        node.submit_transaction(
            created.transaction
        );
    assert(accepted.ok());

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

void test_runtime_exposes_quote_and_auto_send()
{
    using namespace quintum;
    using namespace quintum::net;
    using namespace quintum::wallet;

    const auto directory =
        unique_dir("runtime-auto");
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
    config.wallet_passphrase =
        "stage24-runtime-password";

    assert(runtime.start(
               std::move(config)).ok());

    const auto initial =
        runtime.status();

    assert(initial.min_relay_fee_rate_per_kb ==
           policy::kDefaultMinRelayFeeRatePerKb);
    assert(initial.recommended_fee_rate_per_kb >=
           initial.min_relay_fee_rate_per_kb);

    const auto owned =
        decode_address(
            params.network,
            initial.receive_address
        );
    assert(owned.ok());

    const Bytes owned_payout =
        consensus::make_p2pk_locking_script(
            owned.public_key
        );
    const Bytes external_payout =
        payout_from_scalar(60U);

    const std::uint64_t base_time =
        params.genesis.timestamp + 100'000U;

    const auto first =
        runtime.mine_mempool_block_at(
            owned_payout,
            base_time + 1U,
            4'096U
        );
    assert(first.ok());

    for (std::uint32_t height = 2U;
         height <= 100U;
         ++height) {
        const auto mined =
            runtime.mine_mempool_block_at(
                external_payout,
                base_time +
                    static_cast<std::uint64_t>(
                        height
                    ),
                4'096U
            );
        assert(mined.ok());
    }

    const std::string destination =
        encode_address(
            params.network,
            public_key_from_scalar(61U)
        );
    const Amount amount =
        consensus::block_subsidy(1U) / 3U;

    const auto quote =
        runtime.quote_send_fee(
            destination,
            amount
        );

    assert(quote.ok());
    assert(quote.fee > 0U);
    assert(quote.fee_rate_per_kb >=
           runtime.status()
               .min_relay_fee_rate_per_kb);

    const auto sent =
        runtime.send_to_address_auto_fee(
            destination,
            amount
        );

    assert(sent.ok());
    assert(sent.wallet.fee ==
           quote.fee);
    assert(sent.wallet.fee_rate_per_kb ==
           quote.fee_rate_per_kb);
    assert(sent.node.mempool.required_fee <=
           sent.wallet.fee);

    runtime.stop();

    std::error_code ec;
    std::filesystem::remove_all(
        directory,
        ec
    );
}

} // namespace

int main()
{
    test_min_relay_fee_rejects_underpriced_transaction();
    test_recommended_rate_never_below_relay_floor();
    test_wallet_auto_fee_quotes_and_creates_exact_fee();
    test_runtime_exposes_quote_and_auto_send();
    return 0;
}
