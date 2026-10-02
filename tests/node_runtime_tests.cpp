#include "consensus/genesis.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "node/node.hpp"

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

quintum::Bytes miner_script()
{
    quintum::crypto::PrivateKey key{};
    key.back() = 1U;

    const auto public_key =
        quintum::crypto::derive_public_key(key);

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
        assert(restarted.chain().height().has_value());
        assert(*restarted.chain().height() == 3U);
    }

    {
        NodeRuntime final_restart{params, directory};
        const auto start =
            final_restart.start_at(base_time + 20U);

        assert(start.ok());
        assert(final_restart.chain().height().has_value());
        assert(*final_restart.chain().height() == 3U);
    }

    std::filesystem::remove_all(directory, cleanup_error);

    std::cout << "node runtime tests passed\n";
    return 0;
}
