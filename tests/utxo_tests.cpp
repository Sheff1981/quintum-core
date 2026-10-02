#include "chain/utxo.hpp"
#include "consensus/monetary.hpp"

#include <cassert>

namespace {

quintum::Transaction make_coinbase(quintum::Amount value)
{
    quintum::Transaction tx;
    quintum::TxInput input;
    input.unlocking_script = {0x01U};
    tx.inputs.push_back(input);

    quintum::TxOutput output;
    output.value = value;
    output.locking_script = {0x51U};
    tx.outputs.push_back(output);
    return tx;
}

quintum::Transaction make_spend(
    const quintum::OutPoint& previous,
    quintum::Amount first_value,
    quintum::Amount second_value)
{
    quintum::Transaction tx;

    quintum::TxInput input;
    input.previous_output = previous;
    input.unlocking_script = {0x51U};
    tx.inputs.push_back(input);

    tx.outputs.push_back(quintum::TxOutput{
        .value = first_value,
        .locking_script = {0x51U},
    });
    tx.outputs.push_back(quintum::TxOutput{
        .value = second_value,
        .locking_script = {0x51U},
    });

    return tx;
}

void test_apply_spend_double_spend_and_undo()
{
    quintum::UtxoSet utxos;

    const auto coinbase = make_coinbase(1000U);
    const auto coinbase_result = utxos.apply_transaction(coinbase, 1U);
    assert(coinbase_result.ok());
    assert(coinbase_result.fee == 0U);
    assert(utxos.size() == 1U);

    const quintum::OutPoint funding{
        .txid = quintum::transaction_id(coinbase),
        .index = 0U,
    };
    assert(utxos.contains(funding));

    const auto spend = make_spend(funding, 600U, 300U);

    const auto premature = utxos.apply_transaction(spend, 100U);
    assert(
        premature.error ==
        quintum::UtxoApplyError::premature_coinbase_spend
    );
    assert(utxos.contains(funding));

    const auto spend_result = utxos.apply_transaction(spend, 101U);
    assert(spend_result.ok());
    assert(spend_result.fee == 100U);
    assert(utxos.size() == 2U);
    assert(!utxos.contains(funding));

    const auto double_spend = utxos.apply_transaction(spend, 101U);
    assert(double_spend.error == quintum::UtxoApplyError::missing_input);
    assert(utxos.size() == 2U);

    assert(utxos.undo_transaction(spend_result.undo));
    assert(utxos.size() == 1U);
    assert(utxos.contains(funding));

    assert(utxos.undo_transaction(coinbase_result.undo));
    assert(utxos.size() == 0U);
}

void test_money_range_rejected()
{
    quintum::UtxoSet utxos;

    const auto coinbase = make_coinbase(
        quintum::consensus::kMaxMoney + 1U
    );

    const auto result = utxos.apply_transaction(coinbase, 1U);
    assert(
        result.error ==
        quintum::UtxoApplyError::money_out_of_range
    );
    assert(utxos.size() == 0U);
}

void test_failed_transaction_is_atomic()
{
    quintum::UtxoSet utxos;

    const auto coinbase = make_coinbase(500U);
    const auto coinbase_result = utxos.apply_transaction(coinbase, 1U);
    assert(coinbase_result.ok());

    const quintum::OutPoint funding{
        .txid = quintum::transaction_id(coinbase),
        .index = 0U,
    };

    quintum::Transaction tx;
    tx.inputs.push_back(quintum::TxInput{
        .previous_output = funding,
        .unlocking_script = {0x51U},
    });

    quintum::OutPoint missing;
    missing.txid[0] = 0xaaU;
    missing.index = 7U;
    tx.inputs.push_back(quintum::TxInput{
        .previous_output = missing,
        .unlocking_script = {0x51U},
    });

    tx.outputs.push_back(quintum::TxOutput{
        .value = 400U,
        .locking_script = {0x51U},
    });

    const auto result = utxos.apply_transaction(tx, 101U);
    assert(result.error == quintum::UtxoApplyError::missing_input);
    assert(utxos.size() == 1U);
    assert(utxos.contains(funding));
}

} // namespace

int main()
{
    test_apply_spend_double_spend_and_undo();
    test_money_range_rejected();
    test_failed_transaction_is_atomic();
    return 0;
}
