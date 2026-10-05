#include "consensus/monetary.hpp"
#include "consensus/tx_auth.hpp"

#include <array>
#include <cassert>
#include <limits>

namespace {

quintum::Transaction make_coinbase(quintum::Amount value)
{
    quintum::Transaction tx;
    quintum::TxInput input;
    input.unlocking_script = {0x01U};
    tx.inputs.push_back(input);
    tx.outputs.push_back(quintum::TxOutput{
        .value = value,
        .locking_script = {0x51U},
    });
    return tx;
}

void test_money_constants()
{
    using namespace quintum::consensus;

    static_assert(kAtomicUnitsPerCoin == 100'000'000ULL);
    static_assert(kInitialSubsidy == 5'000'000'000ULL);
    static_assert(kMaxMoney == 2'100'000'000'000'000ULL);
    static_assert(kSubsidyHalvingInterval == 210'000U);
    static_assert(kCoinbaseMaturity == 100U);

    assert(money_range(0U));
    assert(money_range(kMaxMoney));
    assert(!money_range(kMaxMoney + 1U));
}

void test_subsidy_schedule()
{
    using namespace quintum::consensus;

    assert(block_subsidy(0U) == 50ULL * kAtomicUnitsPerCoin);
    assert(block_subsidy(209'999U) == 50ULL * kAtomicUnitsPerCoin);
    assert(block_subsidy(210'000U) == 25ULL * kAtomicUnitsPerCoin);
    assert(block_subsidy(419'999U) == 25ULL * kAtomicUnitsPerCoin);
    assert(block_subsidy(420'000U) == 12ULL * kAtomicUnitsPerCoin + 50'000'000ULL);
    assert(block_subsidy(6'930'000U) == 0U);

    quintum::Amount scheduled_total{0U};
    for (std::uint32_t era = 0U; era < 33U; ++era) {
        scheduled_total +=
            block_subsidy(era * kSubsidyHalvingInterval) *
            kSubsidyHalvingInterval;
    }

    assert(scheduled_total == 2'099'999'997'690'000ULL);
}

void test_coinbase_reward_limit()
{
    using namespace quintum::consensus;

    const auto subsidy = block_subsidy(0U);

    assert(coinbase_reward_is_valid(
        make_coinbase(subsidy),
        0U,
        0U
    ));

    assert(coinbase_reward_is_valid(
        make_coinbase(subsidy + 100U),
        0U,
        100U
    ));

    assert(!coinbase_reward_is_valid(
        make_coinbase(subsidy + 1U),
        0U,
        0U
    ));

    assert(coinbase_reward_is_valid(
        make_coinbase(0U),
        6'930'000U,
        0U
    ));

    assert(!coinbase_reward_is_valid(
        make_coinbase(1U),
        6'930'000U,
        0U
    ));
}

void test_output_money_range()
{
    quintum::Transaction tx;
    tx.inputs.push_back(quintum::TxInput{});
    tx.outputs.push_back(quintum::TxOutput{
        .value = quintum::consensus::kMaxMoney,
        .locking_script = {0x51U},
    });

    const auto total =
        quintum::consensus::transaction_output_total(tx);

    assert(total && *total == quintum::consensus::kMaxMoney);

    tx.outputs.push_back(quintum::TxOutput{
        .value = 1U,
        .locking_script = {0x51U},
    });

    assert(!quintum::consensus::transaction_output_total(tx));
}

} // namespace

void test_randomx_candidate_schedule()
{
    using namespace quintum::consensus;

    static_assert(kRandomXEraBlocks == 1'000'000U);
    static_assert(kRandomXPrimaryEras == 6U);
    static_assert(kRandomXPrimaryEndHeight == 6'000'000U);
    static_assert(kRandomXFounderBasisPoints == 500U);
    static_assert(kRandomXCoinbaseMaturity == 500U);

    assert(randomx_total_subsidy(0U) == 0U);

    assert(randomx_total_subsidy(1U) ==
           50ULL * kAtomicUnitsPerCoin);
    assert(randomx_total_subsidy(1'000'000U) ==
           50ULL * kAtomicUnitsPerCoin);
    assert(randomx_total_subsidy(1'000'001U) ==
           25ULL * kAtomicUnitsPerCoin);
    assert(randomx_total_subsidy(2'000'001U) ==
           1'250'000'000ULL);
    assert(randomx_total_subsidy(3'000'001U) ==
           625'000'000ULL);
    assert(randomx_total_subsidy(4'000'001U) ==
           312'500'000ULL);
    assert(randomx_total_subsidy(5'000'001U) ==
           156'250'000ULL);
    assert(randomx_total_subsidy(6'000'000U) ==
           156'250'000ULL);

    assert(randomx_total_subsidy(6'000'001U) ==
           kRandomXTailSubsidy);
    assert(randomx_total_subsidy(
               std::numeric_limits<std::uint32_t>::max()) ==
           kRandomXTailSubsidy);

    assert(randomx_founder_subsidy(1U) ==
           250'000'000ULL);
    assert(randomx_miner_subsidy(1U) ==
           4'750'000'000ULL);

    assert(randomx_founder_subsidy(5'000'001U) ==
           7'812'500ULL);
    assert(randomx_miner_subsidy(5'000'001U) ==
           148'437'500ULL);

    assert(randomx_founder_subsidy(6'000'001U) == 0U);
    assert(randomx_miner_subsidy(6'000'001U) ==
           kRandomXTailSubsidy);

    quintum::Amount primary_total{0U};
    quintum::Amount founder_total{0U};
    quintum::Amount miner_total{0U};

    for (std::uint32_t era = 0U;
         era < kRandomXPrimaryEras;
         ++era) {
        const std::uint32_t height =
            era * kRandomXEraBlocks + 1U;

        const quintum::Amount blocks =
            static_cast<quintum::Amount>(
                kRandomXEraBlocks);

        primary_total +=
            randomx_total_subsidy(height) *
            blocks;
        founder_total +=
            randomx_founder_subsidy(height) *
            blocks;
        miner_total +=
            randomx_miner_subsidy(height) *
            blocks;
    }

    assert(primary_total ==
           kRandomXPrimaryIssuance);
    assert(founder_total ==
           kRandomXFounderPrimaryIssuance);
    assert(miner_total ==
           kRandomXMinerPrimaryIssuance);
    assert(founder_total + miner_total ==
           primary_total);
    assert(founder_total * 20ULL ==
           primary_total);
}

void test_randomx_consensus_policy()
{
    using namespace quintum;
    using namespace quintum::consensus;

    crypto::PrivateKey founder_private{};
    founder_private.back() = 2U;

    const auto founder_public =
        crypto::derive_public_key(
            founder_private
        );
    assert(founder_public.has_value());

    MonetaryParams params{
        .schedule =
            MonetarySchedule::randomx_v1,
        .max_money =
            kRandomXMoneyRange,
        .coinbase_maturity =
            kRandomXCoinbaseMaturity,
        .founder_payout_enabled = true,
        .founder_public_key =
            *founder_public,
    };

    assert(money_range(
        kRandomXMoneyRange,
        params
    ));
    assert(!money_range(
        kRandomXMoneyRange + 1U,
        params
    ));

    Transaction genesis =
        make_coinbase(0U);

    assert(coinbase_reward_is_valid(
        genesis,
        0U,
        0U,
        params
    ));

    constexpr Amount fee{321U};

    Transaction block_one;
    TxInput input;
    input.unlocking_script = {0x01U};
    block_one.inputs.push_back(input);

    block_one.outputs.push_back(
        TxOutput{
            .value =
                randomx_miner_subsidy(1U) +
                fee,
            .locking_script = {0x51U},
        }
    );
    block_one.outputs.push_back(
        TxOutput{
            .value =
                randomx_founder_subsidy(1U),
            .locking_script =
                make_p2pk_locking_script(
                    *founder_public
                ),
        }
    );

    assert(coinbase_reward_is_valid(
        block_one,
        1U,
        fee,
        params
    ));

    auto missing_founder = block_one;
    missing_founder.outputs.pop_back();

    assert(!coinbase_reward_is_valid(
        missing_founder,
        1U,
        fee,
        params
    ));

    auto wrong_founder_amount = block_one;
    ++wrong_founder_amount.outputs[1].value;

    assert(!coinbase_reward_is_valid(
        wrong_founder_amount,
        1U,
        fee,
        params
    ));

    auto wrong_founder_script = block_one;
    wrong_founder_script.outputs[1].
        locking_script = {0x51U};

    assert(!coinbase_reward_is_valid(
        wrong_founder_script,
        1U,
        fee,
        params
    ));

    params.founder_payout_enabled = false;

    assert(!coinbase_reward_is_valid(
        block_one,
        1U,
        fee,
        params
    ));

    params.founder_payout_enabled = true;

    Transaction tail =
        make_coinbase(
            kRandomXTailSubsidy + fee
        );

    assert(coinbase_reward_is_valid(
        tail,
        kRandomXPrimaryEndHeight + 1U,
        fee,
        params
    ));

    assert(founder_subsidy(
        kRandomXPrimaryEndHeight + 1U,
        params
    ) == 0U);
}

void test_randomx_founder_multisig_custody()
{
    using namespace quintum;
    using namespace quintum::consensus;

    MonetaryParams params{
        .schedule =
            MonetarySchedule::randomx_v1,
        .max_money =
            kRandomXMoneyRange,
        .coinbase_maturity =
            kRandomXCoinbaseMaturity,
        .founder_payout_enabled = true,
        .founder_multisig_enabled = true,
        .founder_multisig_threshold = 2U,
        .founder_multisig_key_count = 3U,
    };

    for (std::size_t i = 0U;
         i < 3U;
         ++i) {
        crypto::PrivateKey key{};
        key.back() =
            static_cast<Byte>(i + 3U);

        const auto public_key =
            crypto::derive_public_key(
                key
            );
        assert(public_key.has_value());

        params.
            founder_multisig_public_keys[i] =
                *public_key;
    }

    const auto script =
        founder_payout_script(params);

    assert(script.has_value());

    const auto policy =
        parse_multisig_locking_script(
            *script
        );

    assert(policy.has_value());
    assert(policy->threshold == 2U);
    assert(policy->public_keys.size() == 3U);

    constexpr Amount fee{123U};

    Transaction coinbase;
    TxInput input;
    input.unlocking_script = {0x01U};
    coinbase.inputs.push_back(input);
    coinbase.outputs.push_back(
        TxOutput{
            .value =
                randomx_miner_subsidy(1U) +
                fee,
            .locking_script = {0x51U},
        }
    );
    coinbase.outputs.push_back(
        TxOutput{
            .value =
                randomx_founder_subsidy(1U),
            .locking_script = *script,
        }
    );

    assert(coinbase_reward_is_valid(
        coinbase,
        1U,
        fee,
        params
    ));

    auto tampered = coinbase;
    tampered.outputs[1].
        locking_script.back() ^= 0x01U;

    assert(!coinbase_reward_is_valid(
        tampered,
        1U,
        fee,
        params
    ));

    params.founder_multisig_threshold =
        4U;

    assert(
        !founder_payout_script(
            params
        ).has_value()
    );
}

int main()
{
    test_money_constants();
    test_subsidy_schedule();
    test_coinbase_reward_limit();
    test_output_money_range();
    test_randomx_candidate_schedule();
    test_randomx_consensus_policy();
    test_randomx_founder_multisig_custody();
    return 0;
}
