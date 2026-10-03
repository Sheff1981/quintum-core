#include "consensus/genesis.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "node/node.hpp"

#include <array>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>

namespace {

std::filesystem::path unique_test_directory()
{
    const auto stamp =
        std::chrono::steady_clock::now()
            .time_since_epoch()
            .count();

    return std::filesystem::temp_directory_path() /
        ("quintum-node-runtime-" + std::to_string(stamp));
}

quintum::crypto::PrivateKey miner_private_key()
{
    quintum::crypto::PrivateKey key{};
    key.back() = 1U;
    return key;
}

quintum::Bytes miner_script()
{
    const auto public_key =
        quintum::crypto::derive_public_key(
            miner_private_key()
        );

    assert(public_key.has_value());
    return quintum::consensus::make_p2pk_locking_script(
        *public_key
    );
}

} // namespace

int main()
{
    using namespace quintum;

    const auto& params = consensus::regtest_params();
    const auto directory = unique_test_directory();

    std::error_code cleanup_error;
    std::filesystem::remove_all(directory, cleanup_error);

    const std::uint64_t base_time =
        params.genesis.timestamp + 1'000U;
    const Bytes payout = miner_script();

    Hash256 height_two_tip{};
    Hash256 first_coinbase_txid{};
    TxOutput first_coinbase_output{};

    {
        NodeRuntime node{params, directory};

        const auto start = node.start_at(base_time);
        assert(start.ok());
        assert(start.created_genesis);
        assert(node.started());
        assert(node.chain().height().has_value());
        assert(*node.chain().height() == 0U);
        assert(node.chain().tip_hash() == params.genesis.hash);
        assert(std::filesystem::exists(node.store().blocks_path()));
        assert(std::filesystem::exists(node.store().state_path()));

        const auto nonce_probe =
            node.mine_block_at(
                payout,
                base_time + 1U,
                0U,
                {},
                12'345U
            );

        assert(!nonce_probe.ok());
        assert(nonce_probe.error ==
               NodeMineError::proof_of_work_exhausted);
        assert(nonce_probe.block.header.nonce ==
               12'345U);
        assert(*node.chain().height() == 0U);

        const auto first =
            node.mine_block_at(
                payout,
                base_time + 1U,
                1'024U
            );

        assert(first.ok());
        assert(first.height == 1U);
        assert(first.block.header.previous_block ==
               params.genesis.hash);
        assert(first.block.header.bits ==
               params.pow.pow_limit_bits);
        assert(first.block.transactions.size() == 1U);
        assert(first.block.transactions.front().is_coinbase());
        assert(first.block.transactions.front().outputs.size() == 1U);
        assert(first.block.transactions.front().outputs.front().value ==
               consensus::block_subsidy(1U));
        assert(first.block.transactions.front().outputs.front().locking_script ==
               payout);
        assert(first.block.header.merkle_root ==
               compute_merkle_root(first.block.transactions).root);
        assert(consensus::check_proof_of_work(
                   first.block.header,
                   params.pow) ==
               consensus::PowCheckError::none);

        first_coinbase_txid =
            transaction_id(first.block.transactions.front());
        first_coinbase_output =
            first.block.transactions.front().outputs.front();

        const auto second =
            node.mine_block_at(
                payout,
                base_time + 2U,
                1'024U
            );

        assert(second.ok());
        assert(second.height == 2U);
        assert(node.chain().height().has_value());
        assert(*node.chain().height() == 2U);
        assert(node.chain().tip_hash().has_value());

        height_two_tip = *node.chain().tip_hash();

        const Bytes invalid_payout{0x00U, 0x01U};
        const auto invalid =
            node.mine_block_at(
                invalid_payout,
                base_time + 3U,
                1'024U
            );

        assert(!invalid.ok());
        assert(invalid.error ==
               NodeMineError::template_failed);
        assert(invalid.template_error ==
               mining::BlockTemplateError::invalid_payout_script);
        assert(*node.chain().height() == 2U);

        Bytes invalid_key_payout(34U, 0U);
        invalid_key_payout.front() =
            consensus::kP2pkLockVersion;

        const auto invalid_key =
            node.mine_block_at(
                invalid_key_payout,
                base_time + 3U,
                1'024U
            );

        assert(!invalid_key.ok());
        assert(invalid_key.template_error ==
               mining::BlockTemplateError::invalid_payout_script);
        assert(*node.chain().height() == 2U);
    }

    {
        NodeRuntime restarted{params, directory};

        const auto start =
            restarted.start_at(base_time + 10U);

        assert(start.ok());
        assert(!start.created_genesis);
        assert(restarted.chain().height().has_value());
        assert(*restarted.chain().height() == 2U);
        assert(restarted.chain().tip_hash().has_value());
        assert(*restarted.chain().tip_hash() ==
               height_two_tip);

        const auto third =
            restarted.mine_block_at(
                payout,
                base_time + 11U,
                1'024U
            );

        assert(third.ok());
        assert(third.height == 3U);
        assert(third.block.header.previous_block ==
               height_two_tip);

        for (std::uint32_t height = 4U;
             height <= 100U;
             ++height) {
            const auto mined =
                restarted.mine_block_at(
                    payout,
                    base_time + 20U +
                        static_cast<std::uint64_t>(height),
                    1'024U
                );

            assert(mined.ok());
            assert(mined.height == height);
        }

        constexpr Amount fee{123U};

        Transaction spend;
        spend.inputs.push_back(TxInput{
            .previous_output = OutPoint{
                .txid = first_coinbase_txid,
                .index = 0U,
            },
        });
        spend.outputs.push_back(TxOutput{
            .value = first_coinbase_output.value - fee,
            .locking_script = payout,
        });

        assert(consensus::sign_p2pk_input(
                   spend,
                   0U,
                   first_coinbase_output,
                   miner_private_key()) ==
               consensus::InputAuthError::none);

        const std::array<Transaction, 1> candidates{spend};

        const auto with_fee =
            restarted.mine_block_at(
                payout,
                base_time + 200U,
                1'024U,
                candidates
            );

        assert(with_fee.ok());
        assert(with_fee.height == 101U);
        assert(with_fee.total_fees == fee);
        assert(with_fee.block.transactions.size() == 2U);
        assert(with_fee.block.transactions[1].inputs.front()
                   .previous_output.txid ==
               first_coinbase_txid);
        assert(with_fee.block.transactions.front()
                   .outputs.front().value ==
               consensus::block_subsidy(101U) + fee);
        assert(consensus::check_proof_of_work(
                   with_fee.block.header,
                   params.pow) ==
               consensus::PowCheckError::none);
        assert(restarted.chain().height().has_value());
        assert(*restarted.chain().height() == 101U);
    }

    {
        NodeRuntime final_restart{params, directory};
        const auto start =
            final_restart.start_at(base_time + 300U);

        assert(start.ok());
        assert(final_restart.chain().height().has_value());
        assert(*final_restart.chain().height() == 101U);
    }

    std::filesystem::remove_all(directory, cleanup_error);

    std::cout << "node runtime tests passed\n";
    return 0;
}
