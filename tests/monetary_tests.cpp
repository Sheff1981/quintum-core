#include "consensus/monetary.hpp"

#include <cassert>

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

int main()
{
    test_money_constants();
    test_subsidy_schedule();
    test_coinbase_reward_limit();
    test_output_money_range();
    return 0;
}
