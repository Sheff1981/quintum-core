#include "chain/chainstate.hpp"
#include "consensus/chainparams.hpp"
#include "consensus/difficulty.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

quintum::Transaction make_coinbase(
    std::uint32_t height,
    quintum::Byte tag)
{
    quintum::Transaction tx;

    quintum::TxInput input;
    input.unlocking_script = {
        0x01U,
        static_cast<quintum::Byte>(height & 0xffU),
        static_cast<quintum::Byte>((height >> 8U) & 0xffU),
        static_cast<quintum::Byte>((height >> 16U) & 0xffU),
        static_cast<quintum::Byte>((height >> 24U) & 0xffU),
        tag,
    };
    tx.inputs.push_back(input);

    tx.outputs.push_back(quintum::TxOutput{
        .value = quintum::consensus::block_subsidy(height),
        .locking_script = {0x51U},
    });

    return tx;
}

quintum::Block make_block(
    const quintum::Hash256& previous,
    std::uint32_t height,
    std::uint64_t timestamp,
    std::uint32_t bits,
    quintum::Byte tag)
{
    quintum::Block block;
    block.header.previous_block = previous;
    block.header.timestamp = timestamp;
    block.header.bits = bits;
    block.transactions.push_back(
        make_coinbase(height, tag)
    );
    quintum::update_merkle_root(block);

    const auto mined =
        quintum::consensus::mine_header(
            block.header,
            100'000U
        );

    assert(mined.found());
    return block;
}

quintum::consensus::ChainParams small_retarget_params(
    bool allow_min_difficulty)
{
    return quintum::consensus::ChainParams{
        .network = quintum::consensus::Network::regtest,
        .name = "difficulty-test",
        .message_start = {0xaaU, 0xbbU, 0xccU, 0xddU},
        .p2p_port = 1U,
        .rpc_port = 2U,
        .pow = quintum::consensus::PowParams{
            .target_spacing_seconds = 10U,
            .retarget_interval = 4U,
            .pow_limit_bits = 0x2100ffffU,
            .allow_min_difficulty_blocks =
                allow_min_difficulty,
            .no_retargeting = false,
        },
    };
}

void test_network_parameter_sets()
{
    const auto& main =
        quintum::consensus::mainnet_params();
    const auto& test =
        quintum::consensus::testnet_params();
    const auto& reg =
        quintum::consensus::regtest_params();

    assert(main.name == "mainnet");
    assert(test.name == "testnet");
    assert(reg.name == "regtest");

    assert(main.message_start != test.message_start);
    assert(main.message_start != reg.message_start);
    assert(test.message_start != reg.message_start);

    assert(main.p2p_port != test.p2p_port);
    assert(main.p2p_port != reg.p2p_port);
    assert(test.p2p_port != reg.p2p_port);

    assert(main.pow.target_spacing_seconds == 600U);
    assert(main.pow.retarget_interval == 2016U);
    assert(main.pow.pow_limit_bits == 0x1e0ffff0U);
    assert(!main.pow.allow_min_difficulty_blocks);
    assert(!main.pow.no_retargeting);

    assert(test.pow.target_spacing_seconds == 600U);
    assert(test.pow.retarget_interval == 2016U);
    assert(test.pow.allow_min_difficulty_blocks);
    assert(!test.pow.no_retargeting);

    assert(reg.pow.pow_limit_bits == 0x2100ffffU);
    assert(reg.pow.no_retargeting);
}

void test_retarget_vectors()
{
    const auto& params =
        quintum::consensus::mainnet_params().pow;

    const std::uint64_t target_timespan =
        params.target_spacing_seconds *
        params.retarget_interval;

    const auto unchanged =
        quintum::consensus::calculate_retarget_bits(
            0x1e0ffff0U,
            1'000'000U,
            1'000'000U + target_timespan,
            params
        );

    assert(unchanged.ok());
    assert(unchanged.bits == 0x1e0ffff0U);

    const auto fastest =
        quintum::consensus::calculate_retarget_bits(
            0x1e0ffff0U,
            1'000'000U,
            1'000'001U,
            params
        );

    assert(fastest.ok());
    assert(fastest.bits == 0x1e03fffcU);

    const auto capped_slow =
        quintum::consensus::calculate_retarget_bits(
            0x1e0ffff0U,
            1'000'000U,
            1'000'000U + (target_timespan * 10U),
            params
        );

    assert(capped_slow.ok());
    assert(capped_slow.bits == 0x1e0ffff0U);

    const auto harder_previous =
        quintum::consensus::calculate_retarget_bits(
            0x1e07fff8U,
            1'000'000U,
            1'000'000U + (target_timespan * 2U),
            params
        );

    assert(harder_previous.ok());
    assert(harder_previous.bits == 0x1e0ffff0U);

    assert(
        quintum::consensus::target_within_pow_limit(
            0x1e0ffff0U,
            params
        )
    );

    assert(
        !quintum::consensus::target_within_pow_limit(
            0x2100ffffU,
            params
        )
    );
}

void test_regtest_no_retargeting()
{
    const auto& params =
        quintum::consensus::regtest_params().pow;

    const auto result =
        quintum::consensus::calculate_retarget_bits(
            0x2100ffffU,
            10U,
            9'999'999U,
            params
        );

    assert(result.ok());
    assert(result.bits == 0x2100ffffU);
}

void test_chainstate_rejects_wrong_bits()
{
    const auto params =
        small_retarget_params(false);

    quintum::Chainstate chain{params};
    quintum::Hash256 previous{};

    const std::uint64_t times[] = {
        100U, 105U, 110U, 115U
    };

    for (std::uint32_t height = 0U;
         height < 4U;
         ++height) {
        const auto block = make_block(
            previous,
            height,
            times[height],
            0x2100ffffU,
            static_cast<quintum::Byte>(height)
        );

        const auto result =
            chain.connect_block(block);

        assert(result.ok());
        assert(result.activated);

        previous =
            quintum::block_hash(block.header);
    }

    // Target interval is 40 seconds. The observed 15 seconds is
    // above the 10-second minimum, so target scales by 15/40.
    const auto calculated =
        quintum::consensus::calculate_retarget_bits(
            0x2100ffffU,
            100U,
            115U,
            params.pow
        );

    assert(calculated.ok());
    const std::uint32_t expected_bits = calculated.bits;


    const auto wrong = make_block(
        previous,
        4U,
        125U,
        0x2100ffffU,
        0x44U
    );

    const auto wrong_result =
        chain.connect_block(wrong);

    assert(
        wrong_result.error ==
        quintum::ChainConnectError::unexpected_difficulty
    );
    assert(chain.height() && *chain.height() == 3U);

    const auto correct = make_block(
        previous,
        4U,
        125U,
        expected_bits,
        0x45U
    );

    const auto correct_result =
        chain.connect_block(correct);

    assert(correct_result.ok());
    assert(correct_result.activated);
    assert(chain.height() && *chain.height() == 4U);
}

void test_testnet_min_difficulty_and_restore()
{
    const auto params =
        small_retarget_params(true);

    quintum::Chainstate chain{params};
    quintum::Hash256 previous{};

    for (std::uint32_t height = 0U;
         height < 4U;
         ++height) {
        const auto block = make_block(
            previous,
            height,
            100U + (height * 5U),
            0x2100ffffU,
            static_cast<quintum::Byte>(0x50U + height)
        );

        assert(chain.connect_block(block).ok());
        previous =
            quintum::block_hash(block.header);
    }

    constexpr std::uint32_t hard_bits =
        0x205fffa0U;

    const auto hard = make_block(
        previous,
        4U,
        125U,
        hard_bits,
        0x60U
    );
    assert(chain.connect_block(hard).ok());
    previous = quintum::block_hash(hard.header);

    // More than 2 * spacing after parent: testnet minimum target.
    const auto delayed_bits =
        chain.next_work_required(146U);

    assert(delayed_bits);
    assert(*delayed_bits == 0x2100ffffU);

    const auto delayed = make_block(
        previous,
        5U,
        146U,
        *delayed_bits,
        0x61U
    );

    const auto delayed_result =
        chain.connect_block(delayed);

    assert(delayed_result.ok());
    assert(delayed_result.activated);
    previous =
        quintum::block_hash(delayed.header);

    // A normally timed block must restore the last non-minimum target.
    const auto restored_bits =
        chain.next_work_required(156U);

    assert(restored_bits);
    assert(*restored_bits == hard_bits);

    const auto restored = make_block(
        previous,
        6U,
        156U,
        *restored_bits,
        0x62U
    );

    const auto restored_result =
        chain.connect_block(restored);

    assert(restored_result.ok());
    assert(restored_result.activated);
    assert(chain.height() && *chain.height() == 6U);
}

} // namespace

int main()
{
    test_network_parameter_sets();
    test_retarget_vectors();
    test_regtest_no_retargeting();
    test_chainstate_rejects_wrong_bits();
    test_testnet_min_difficulty_and_restore();
    return 0;
}
