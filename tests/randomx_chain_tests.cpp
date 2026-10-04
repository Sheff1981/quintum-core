#include "chain/chainstate.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/randomx_seed.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <cstdint>

namespace {

quintum::Transaction coinbase(
    std::uint32_t height)
{
    quintum::Transaction tx;

    quintum::TxInput input;
    input.unlocking_script = {
        0x01U,
        static_cast<quintum::Byte>(
            height & 0xffU
        ),
    };
    tx.inputs.push_back(
        std::move(input)
    );

    tx.outputs.push_back(
        quintum::TxOutput{
            .value =
                quintum::consensus::
                    block_subsidy(height),
            .locking_script = {0x51U},
        }
    );

    return tx;
}

quintum::Block make_randomx_block(
    const quintum::Hash256& previous,
    std::uint32_t height,
    std::uint64_t timestamp,
    std::uint32_t bits,
    const quintum::Hash256& seed_key)
{
    quintum::Block block;
    block.header.previous_block = previous;
    block.header.timestamp = timestamp;
    block.header.bits = bits;
    block.transactions.push_back(
        coinbase(height)
    );

    quintum::update_merkle_root(block);

    const auto mined =
        quintum::consensus::
            mine_randomx_header(
                block.header,
                seed_key,
                128U
            );

    assert(mined.found());
    return block;
}

quintum::consensus::ChainParams
randomx_chain_params()
{
    return quintum::consensus::ChainParams{
        .network =
            quintum::consensus::Network::regtest,
        .name = "randomx-chain-test",
        .message_start =
            {0x91U, 0x92U, 0x93U, 0x94U},
        .p2p_port = 5U,
        .rpc_port = 6U,
        .pow = quintum::consensus::PowParams{
            .target_spacing_seconds = 120U,
            .retarget_interval = 1U,
            .pow_limit_bits = 0x207fffffU,
            .allow_min_difficulty_blocks = false,
            .no_retargeting = true,
            .difficulty_algorithm =
                quintum::consensus::
                    DifficultyAlgorithm::periodic,
            .asert_half_life_seconds = 0U,
            .asert_anchor_height = 0U,
            .pow_algorithm =
                quintum::consensus::
                    PowAlgorithm::randomx_v2,
        },
    };
}

void test_randomx_chain_validation_and_seed()
{
    const auto params =
        randomx_chain_params();

    quintum::Chainstate chain{params};

    const quintum::Hash256 zero_hash{};
    const auto genesis_seed =
        quintum::consensus::randomx_seed_key(
            0U,
            zero_hash
        );

    const auto genesis =
        make_randomx_block(
            zero_hash,
            0U,
            1'000U,
            params.pow.pow_limit_bits,
            genesis_seed
        );

    const auto genesis_result =
        chain.connect_block(
            genesis,
            2'000U
        );

    assert(genesis_result.ok());
    assert(genesis_result.activated);
    assert(chain.height() &&
           *chain.height() == 0U);

    const auto genesis_id =
        quintum::block_hash(
            genesis.header
        );

    const auto expected_next_seed =
        quintum::consensus::randomx_seed_key(
            0U,
            genesis_id
        );

    const auto next_seed =
        chain.next_randomx_seed_key();

    assert(next_seed.has_value());
    assert(*next_seed ==
           expected_next_seed);
    assert(*next_seed !=
           genesis_seed);

    const auto block1 =
        make_randomx_block(
            genesis_id,
            1U,
            1'120U,
            params.pow.pow_limit_bits,
            *next_seed
        );

    const auto block1_result =
        chain.connect_block(
            block1,
            2'000U
        );

    assert(block1_result.ok());
    assert(block1_result.activated);
    assert(chain.height() &&
           *chain.height() == 1U);

    const auto block2_seed =
        chain.next_randomx_seed_key();

    assert(block2_seed.has_value());
    assert(*block2_seed ==
           expected_next_seed);
}

} // namespace

int main()
{
    test_randomx_chain_validation_and_seed();
    return 0;
}
