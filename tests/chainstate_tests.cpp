#include "chain/chainstate.hpp"
#include "consensus/pow.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <cstdint>

namespace {

quintum::Transaction make_coinbase(
    std::uint32_t height,
    quintum::Amount value)
{
    quintum::Transaction tx;

    quintum::TxInput input;
    input.unlocking_script = {
        0x01U,
        static_cast<quintum::Byte>(height & 0xffU),
        static_cast<quintum::Byte>((height >> 8U) & 0xffU),
        static_cast<quintum::Byte>((height >> 16U) & 0xffU),
        static_cast<quintum::Byte>((height >> 24U) & 0xffU),
    };
    tx.inputs.push_back(input);

    tx.outputs.push_back(quintum::TxOutput{
        .value = value,
        .locking_script = {0x51U},
    });

    return tx;
}

quintum::Transaction make_spend(
    const quintum::OutPoint& previous,
    quintum::Amount value)
{
    quintum::Transaction tx;

    tx.inputs.push_back(quintum::TxInput{
        .previous_output = previous,
        .unlocking_script = {0x51U},
    });

    tx.outputs.push_back(quintum::TxOutput{
        .value = value,
        .locking_script = {0x51U},
    });

    return tx;
}

quintum::Block make_block(
    const quintum::Hash256& previous,
    std::uint32_t height,
    std::vector<quintum::Transaction> transactions)
{
    quintum::Block block;
    block.header.previous_block = previous;
    block.header.timestamp = 1700000000ULL + height;
    block.header.bits = 0x2100ffffU;
    block.transactions = std::move(transactions);
    quintum::update_merkle_root(block);

    const auto mined =
        quintum::consensus::mine_header(block.header, 1000U);

    assert(mined.found());
    return block;
}

void test_connect_disconnect_and_atomic_failure()
{
    quintum::Chainstate chain;

    assert(chain.empty());
    assert(!chain.height().has_value());
    assert(!chain.tip_hash().has_value());
    assert(chain.utxos().size() == 0U);

    quintum::Hash256 zero{};

    const auto genesis_coinbase = make_coinbase(0U, 1000U);
    const auto genesis = make_block(
        zero,
        0U,
        {genesis_coinbase}
    );

    const auto genesis_result = chain.connect_block(genesis);
    assert(genesis_result.ok());
    assert(genesis_result.total_fees == 0U);
    assert(chain.size() == 1U);
    assert(chain.height() && *chain.height() == 0U);
    assert(chain.tip_hash() && *chain.tip_hash() == quintum::block_hash(genesis.header));
    assert(chain.utxos().size() == 1U);

    const auto genesis_work = chain.cumulative_work();

    const quintum::OutPoint funding{
        .txid = quintum::transaction_id(genesis_coinbase),
        .index = 0U,
    };
    assert(chain.utxos().contains(funding));

    const auto spend = make_spend(funding, 900U);
    const auto block_one = make_block(
        quintum::block_hash(genesis.header),
        1U,
        {make_coinbase(1U, 50U), spend}
    );

    const auto block_one_result = chain.connect_block(block_one);
    assert(block_one_result.ok());
    assert(block_one_result.total_fees == 100U);
    assert(chain.size() == 2U);
    assert(chain.height() && *chain.height() == 1U);
    assert(chain.utxos().size() == 2U);
    assert(!chain.utxos().contains(funding));
    assert(chain.cumulative_work() != genesis_work);

    const auto stable_tip = *chain.tip_hash();
    const auto stable_work = chain.cumulative_work();
    const auto stable_utxo_size = chain.utxos().size();

    // Wrong parent: must be rejected without touching live state.
    auto wrong_parent = zero;
    wrong_parent[31] = 0x42U;

    const auto bad_parent_block = make_block(
        wrong_parent,
        2U,
        {make_coinbase(2U, 50U)}
    );

    const auto bad_parent_result =
        chain.connect_block(bad_parent_block);

    assert(
        bad_parent_result.error ==
        quintum::ChainConnectError::bad_previous_block
    );
    assert(chain.tip_hash() && *chain.tip_hash() == stable_tip);
    assert(chain.cumulative_work() == stable_work);
    assert(chain.utxos().size() == stable_utxo_size);

    // Transaction 0 is valid coinbase, transaction 1 references a missing UTXO.
    // The whole block must fail atomically; coinbase output must not leak into state.
    quintum::OutPoint missing;
    missing.txid[0] = 0xaaU;
    missing.index = 7U;

    const auto invalid_block = make_block(
        stable_tip,
        2U,
        {
            make_coinbase(2U, 50U),
            make_spend(missing, 1U),
        }
    );

    const auto invalid_result =
        chain.connect_block(invalid_block);

    assert(
        invalid_result.error ==
        quintum::ChainConnectError::transaction_failed
    );
    assert(
        invalid_result.transaction_error ==
        quintum::UtxoApplyError::missing_input
    );
    assert(invalid_result.transaction_index == 1U);
    assert(chain.size() == 2U);
    assert(chain.tip_hash() && *chain.tip_hash() == stable_tip);
    assert(chain.cumulative_work() == stable_work);
    assert(chain.utxos().size() == stable_utxo_size);

    // Disconnect block 1: genesis UTXO must be restored exactly.
    assert(
        chain.disconnect_tip() ==
        quintum::ChainDisconnectError::none
    );
    assert(chain.size() == 1U);
    assert(chain.height() && *chain.height() == 0U);
    assert(chain.cumulative_work() == genesis_work);
    assert(chain.utxos().size() == 1U);
    assert(chain.utxos().contains(funding));

    // Disconnect development genesis: return to empty state.
    assert(
        chain.disconnect_tip() ==
        quintum::ChainDisconnectError::none
    );
    assert(chain.empty());
    assert(chain.utxos().size() == 0U);

    assert(
        chain.disconnect_tip() ==
        quintum::ChainDisconnectError::empty_chain
    );
}

} // namespace

int main()
{
    test_connect_disconnect_and_atomic_failure();
    return 0;
}
