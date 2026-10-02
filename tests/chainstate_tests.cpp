#include "chain/chainstate.hpp"
#include "consensus/pow.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <cstdint>

namespace {

quintum::Transaction make_coinbase(
    std::uint32_t height,
    quintum::Amount value,
    quintum::Byte tag = 0U)
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
        quintum::ChainConnectError::unknown_parent
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

void test_heavier_fork_reorg_and_failed_reorg_rollback()
{
    quintum::Chainstate chain;
    quintum::Hash256 zero{};

    const auto genesis_coinbase = make_coinbase(0U, 1000U, 0x01U);
    const auto genesis = make_block(zero, 0U, {genesis_coinbase});
    const auto genesis_hash = quintum::block_hash(genesis.header);

    const auto genesis_result = chain.connect_block(genesis);
    assert(genesis_result.ok());
    assert(genesis_result.activated);
    assert(!genesis_result.reorganized);

    const quintum::OutPoint funding{
        .txid = quintum::transaction_id(genesis_coinbase),
        .index = 0U,
    };

    const auto a_spend = make_spend(funding, 900U);
    const auto a1 = make_block(
        genesis_hash,
        1U,
        {make_coinbase(1U, 50U, 0x11U), a_spend}
    );
    const auto a1_hash = quintum::block_hash(a1.header);

    const auto a1_result = chain.connect_block(a1);
    assert(a1_result.ok() && a1_result.activated);

    const auto a2 = make_block(
        a1_hash,
        2U,
        {make_coinbase(2U, 50U, 0x12U)}
    );
    const auto a2_hash = quintum::block_hash(a2.header);

    const auto a2_result = chain.connect_block(a2);
    assert(a2_result.ok() && a2_result.activated);
    assert(chain.tip_hash() && *chain.tip_hash() == a2_hash);
    assert(chain.height() && *chain.height() == 2U);

    const quintum::OutPoint a_spend_output{
        .txid = quintum::transaction_id(a_spend),
        .index = 0U,
    };
    assert(chain.utxos().contains(a_spend_output));

    // Build an alternative branch from genesis. Equal cumulative work must
    // not cause a reorg; only strictly greater work may replace the tip.
    const auto b_spend = make_spend(funding, 800U);
    const auto b1 = make_block(
        genesis_hash,
        1U,
        {make_coinbase(1U, 50U, 0x21U), b_spend}
    );
    const auto b1_hash = quintum::block_hash(b1.header);

    const auto b1_result = chain.connect_block(b1);
    assert(b1_result.ok());
    assert(!b1_result.activated);
    assert(chain.tip_hash() && *chain.tip_hash() == a2_hash);

    const auto b2 = make_block(
        b1_hash,
        2U,
        {make_coinbase(2U, 50U, 0x22U)}
    );
    const auto b2_hash = quintum::block_hash(b2.header);

    const auto b2_result = chain.connect_block(b2);
    assert(b2_result.ok());
    assert(!b2_result.activated);
    assert(chain.tip_hash() && *chain.tip_hash() == a2_hash);

    const auto b3 = make_block(
        b2_hash,
        3U,
        {make_coinbase(3U, 50U, 0x23U)}
    );
    const auto b3_hash = quintum::block_hash(b3.header);

    const auto b3_result = chain.connect_block(b3);
    assert(b3_result.ok());
    assert(b3_result.activated);
    assert(b3_result.reorganized);
    assert(chain.tip_hash() && *chain.tip_hash() == b3_hash);
    assert(chain.height() && *chain.height() == 3U);
    assert(chain.is_on_active_chain(genesis_hash));
    assert(!chain.is_on_active_chain(a1_hash));
    assert(!chain.is_on_active_chain(a2_hash));
    assert(chain.is_on_active_chain(b1_hash));
    assert(chain.is_on_active_chain(b2_hash));
    assert(chain.is_on_active_chain(b3_hash));

    const quintum::OutPoint b_spend_output{
        .txid = quintum::transaction_id(b_spend),
        .index = 0U,
    };
    assert(!chain.utxos().contains(a_spend_output));
    assert(chain.utxos().contains(b_spend_output));

    const auto stable_tip = *chain.tip_hash();
    const auto stable_work = chain.cumulative_work();
    const auto stable_utxo_size = chain.utxos().size();

    // Build a longer branch containing a context-invalid spend. It remains
    // indexed while weaker/equal, but when it becomes heavier activation must
    // fail atomically and the B branch must stay active.
    quintum::OutPoint missing;
    missing.txid[0] = 0xccU;
    missing.index = 9U;

    const auto c1 = make_block(
        genesis_hash,
        1U,
        {
            make_coinbase(1U, 50U, 0x31U),
            make_spend(missing, 1U),
        }
    );
    const auto c1_hash = quintum::block_hash(c1.header);
    const auto c1_result = chain.connect_block(c1);
    assert(c1_result.ok() && !c1_result.activated);

    const auto c2 = make_block(
        c1_hash,
        2U,
        {make_coinbase(2U, 50U, 0x32U)}
    );
    const auto c2_hash = quintum::block_hash(c2.header);
    assert(chain.connect_block(c2).ok());

    const auto c3 = make_block(
        c2_hash,
        3U,
        {make_coinbase(3U, 50U, 0x33U)}
    );
    const auto c3_hash = quintum::block_hash(c3.header);
    assert(chain.connect_block(c3).ok());

    const auto c4 = make_block(
        c3_hash,
        4U,
        {make_coinbase(4U, 50U, 0x34U)}
    );

    const auto c4_result = chain.connect_block(c4);
    assert(
        c4_result.error ==
        quintum::ChainConnectError::transaction_failed
    );
    assert(
        c4_result.transaction_error ==
        quintum::UtxoApplyError::missing_input
    );

    assert(chain.tip_hash() && *chain.tip_hash() == stable_tip);
    assert(chain.cumulative_work() == stable_work);
    assert(chain.utxos().size() == stable_utxo_size);
    assert(chain.utxos().contains(b_spend_output));
    assert(!chain.utxos().contains(a_spend_output));

    // A descendant of the now-known invalid branch must be rejected early.
    const auto c4_hash = quintum::block_hash(c4.header);
    const auto c5 = make_block(
        c4_hash,
        5U,
        {make_coinbase(5U, 50U, 0x35U)}
    );
    const auto c5_result = chain.connect_block(c5);
    assert(
        c5_result.error ==
        quintum::ChainConnectError::invalid_ancestor
    );

    assert(chain.block_index_size() >= 10U);
}

} // namespace

int main()
{
    test_connect_disconnect_and_atomic_failure();
    test_heavier_fork_reorg_and_failed_reorg_rollback();
    return 0;
}
