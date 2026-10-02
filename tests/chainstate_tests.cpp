#include "chain/chainstate.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "crypto/secp256k1.hpp"
#include "primitives/block.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

namespace {

quintum::crypto::PrivateKey test_private_key()
{
    quintum::crypto::PrivateKey key{};
    key.back() = 1U;
    return key;
}

quintum::Bytes test_locking_script()
{
    const auto public_key =
        quintum::crypto::derive_public_key(
            test_private_key()
        );
    assert(public_key);
    return quintum::consensus::make_p2pk_locking_script(
        *public_key
    );
}

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
        .locking_script = test_locking_script(),
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

quintum::Hash256 append_coinbase_range(
    quintum::Chainstate& chain,
    quintum::Hash256 previous,
    std::uint32_t first_height,
    std::uint32_t last_height,
    quintum::Byte tag)
{
    for (std::uint32_t height = first_height;
         height <= last_height;
         ++height) {
        const auto block = make_block(
            previous,
            height,
            {
                make_coinbase(
                    height,
                    quintum::consensus::block_subsidy(height),
                    tag
                )
            }
        );

        const auto result = chain.connect_block(block);
        assert(result.ok());
        assert(result.activated);

        previous = quintum::block_hash(block.header);
    }

    return previous;
}

void test_connect_disconnect_and_atomic_failure()
{
    quintum::Chainstate chain;
    quintum::Hash256 zero{};

    const auto genesis_coinbase = make_coinbase(
        0U,
        quintum::consensus::block_subsidy(0U),
        0x01U
    );
    const auto genesis = make_block(zero, 0U, {genesis_coinbase});
    const auto genesis_hash = quintum::block_hash(genesis.header);

    const auto genesis_result = chain.connect_block(genesis);
    assert(genesis_result.ok());
    assert(genesis_result.activated);
    assert(chain.height() && *chain.height() == 0U);
    assert(chain.utxos().size() == 1U);

    const quintum::OutPoint funding{
        .txid = quintum::transaction_id(genesis_coinbase),
        .index = 0U,
    };
    assert(chain.utxos().contains(funding));

    // Mature the genesis coinbase: spend becomes legal at height 100.
    const auto height_99_tip = append_coinbase_range(
        chain,
        genesis_hash,
        1U,
        99U,
        0x02U
    );

    assert(chain.height() && *chain.height() == 99U);
    assert(chain.utxos().size() == 100U);

    const auto spend = make_signed_spend(
        funding,
        genesis_coinbase.outputs.front(),
        quintum::consensus::block_subsidy(0U) - 100U
    );

    const auto block_100 = make_block(
        height_99_tip,
        100U,
        {
            make_coinbase(
                100U,
                quintum::consensus::block_subsidy(100U) + 100U,
                0x03U
            ),
            spend,
        }
    );

    const auto block_100_result = chain.connect_block(block_100);
    assert(block_100_result.ok());
    assert(block_100_result.total_fees == 100U);
    assert(block_100_result.activated);
    assert(chain.height() && *chain.height() == 100U);
    assert(chain.utxos().size() == 101U);
    assert(!chain.utxos().contains(funding));

    const auto stable_tip = *chain.tip_hash();
    const auto stable_work = chain.cumulative_work();
    const auto stable_utxo_size = chain.utxos().size();

    // Unknown parent must not touch active state.
    auto wrong_parent = zero;
    wrong_parent[31] = 0x42U;

    const auto bad_parent_block = make_block(
        wrong_parent,
        101U,
        {
            make_coinbase(
                101U,
                quintum::consensus::block_subsidy(101U),
                0x04U
            )
        }
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

    // Context-invalid transaction must make the whole block fail atomically.
    quintum::OutPoint missing;
    missing.txid[0] = 0xaaU;
    missing.index = 7U;

    const auto invalid_block = make_block(
        stable_tip,
        101U,
        {
            make_coinbase(
                101U,
                quintum::consensus::block_subsidy(101U),
                0x05U
            ),
            make_missing_spend(missing, 1U),
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
    assert(chain.tip_hash() && *chain.tip_hash() == stable_tip);
    assert(chain.cumulative_work() == stable_work);
    assert(chain.utxos().size() == stable_utxo_size);

    // Overclaiming coinbase by one atomic unit must fail.
    const auto inflation_block = make_block(
        stable_tip,
        101U,
        {
            make_coinbase(
                101U,
                quintum::consensus::block_subsidy(101U) + 1U,
                0x06U
            )
        }
    );

    const auto inflation_result =
        chain.connect_block(inflation_block);

    assert(
        inflation_result.error ==
        quintum::ChainConnectError::invalid_coinbase_reward
    );
    assert(chain.tip_hash() && *chain.tip_hash() == stable_tip);
    assert(chain.utxos().size() == stable_utxo_size);

    // Disconnect height 100: mature genesis UTXO must return.
    assert(
        chain.disconnect_tip() ==
        quintum::ChainDisconnectError::none
    );
    assert(chain.height() && *chain.height() == 99U);
    assert(chain.utxos().contains(funding));
    assert(chain.utxos().size() == 100U);

    // Return all the way to an empty development state.
    while (!chain.empty()) {
        assert(
            chain.disconnect_tip() ==
            quintum::ChainDisconnectError::none
        );
    }
    assert(chain.utxos().size() == 0U);
}

void test_heavier_fork_reorg_and_failed_reorg_rollback()
{
    quintum::Chainstate chain;
    quintum::Hash256 zero{};

    const auto genesis_coinbase = make_coinbase(
        0U,
        quintum::consensus::block_subsidy(0U),
        0x10U
    );
    const auto genesis = make_block(zero, 0U, {genesis_coinbase});
    const auto genesis_hash = quintum::block_hash(genesis.header);

    const auto genesis_result = chain.connect_block(genesis);
    assert(genesis_result.ok() && genesis_result.activated);

    const quintum::OutPoint funding{
        .txid = quintum::transaction_id(genesis_coinbase),
        .index = 0U,
    };

    // Common history through height 99. The genesis reward is now mature.
    const auto common_tip = append_coinbase_range(
        chain,
        genesis_hash,
        1U,
        99U,
        0x11U
    );

    const auto a_spend = make_signed_spend(
        funding,
        genesis_coinbase.outputs.front(),
        quintum::consensus::block_subsidy(0U) - 100U
    );
    const auto a1 = make_block(
        common_tip,
        100U,
        {
            make_coinbase(
                100U,
                quintum::consensus::block_subsidy(100U) + 100U,
                0x21U
            ),
            a_spend,
        }
    );
    const auto a1_hash = quintum::block_hash(a1.header);

    const auto a1_result = chain.connect_block(a1);
    assert(a1_result.ok() && a1_result.activated);

    const auto a2 = make_block(
        a1_hash,
        101U,
        {
            make_coinbase(
                101U,
                quintum::consensus::block_subsidy(101U),
                0x22U
            )
        }
    );
    const auto a2_hash = quintum::block_hash(a2.header);

    const auto a2_result = chain.connect_block(a2);
    assert(a2_result.ok() && a2_result.activated);
    assert(chain.tip_hash() && *chain.tip_hash() == a2_hash);

    const quintum::OutPoint a_spend_output{
        .txid = quintum::transaction_id(a_spend),
        .index = 0U,
    };
    assert(chain.utxos().contains(a_spend_output));

    // Branch B starts from the same height-99 ancestor.
    const auto b_spend = make_signed_spend(
        funding,
        genesis_coinbase.outputs.front(),
        quintum::consensus::block_subsidy(0U) - 200U
    );
    const auto b1 = make_block(
        common_tip,
        100U,
        {
            make_coinbase(
                100U,
                quintum::consensus::block_subsidy(100U) + 200U,
                0x31U
            ),
            b_spend,
        }
    );
    const auto b1_hash = quintum::block_hash(b1.header);

    const auto b1_result = chain.connect_block(b1);
    assert(b1_result.ok() && !b1_result.activated);
    assert(chain.tip_hash() && *chain.tip_hash() == a2_hash);

    const auto b2 = make_block(
        b1_hash,
        101U,
        {
            make_coinbase(
                101U,
                quintum::consensus::block_subsidy(101U),
                0x32U
            )
        }
    );
    const auto b2_hash = quintum::block_hash(b2.header);

    const auto b2_result = chain.connect_block(b2);
    assert(b2_result.ok() && !b2_result.activated);
    assert(chain.tip_hash() && *chain.tip_hash() == a2_hash);

    const auto b3 = make_block(
        b2_hash,
        102U,
        {
            make_coinbase(
                102U,
                quintum::consensus::block_subsidy(102U),
                0x33U
            )
        }
    );
    const auto b3_hash = quintum::block_hash(b3.header);

    const auto b3_result = chain.connect_block(b3);
    assert(b3_result.ok());
    assert(b3_result.activated);
    assert(b3_result.reorganized);
    assert(chain.tip_hash() && *chain.tip_hash() == b3_hash);
    assert(chain.height() && *chain.height() == 102U);

    const quintum::OutPoint b_spend_output{
        .txid = quintum::transaction_id(b_spend),
        .index = 0U,
    };
    assert(!chain.utxos().contains(a_spend_output));
    assert(chain.utxos().contains(b_spend_output));

    const auto stable_tip = *chain.tip_hash();
    const auto stable_work = chain.cumulative_work();
    const auto stable_utxo_size = chain.utxos().size();

    // Branch C contains a transaction invalid in its own UTXO context.
    quintum::OutPoint missing;
    missing.txid[0] = 0xccU;
    missing.index = 9U;

    const auto c1 = make_block(
        common_tip,
        100U,
        {
            make_coinbase(
                100U,
                quintum::consensus::block_subsidy(100U),
                0x41U
            ),
            make_spend(missing, 1U),
        }
    );
    const auto c1_hash = quintum::block_hash(c1.header);
    const auto c1_result = chain.connect_block(c1);
    assert(c1_result.ok() && !c1_result.activated);

    const auto c2 = make_block(
        c1_hash,
        101U,
        {
            make_coinbase(
                101U,
                quintum::consensus::block_subsidy(101U),
                0x42U
            )
        }
    );
    const auto c2_hash = quintum::block_hash(c2.header);
    assert(chain.connect_block(c2).ok());

    const auto c3 = make_block(
        c2_hash,
        102U,
        {
            make_coinbase(
                102U,
                quintum::consensus::block_subsidy(102U),
                0x43U
            )
        }
    );
    const auto c3_hash = quintum::block_hash(c3.header);
    assert(chain.connect_block(c3).ok());

    const auto c4 = make_block(
        c3_hash,
        103U,
        {
            make_coinbase(
                103U,
                quintum::consensus::block_subsidy(103U),
                0x44U
            )
        }
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

    const auto c4_hash = quintum::block_hash(c4.header);
    const auto c5 = make_block(
        c4_hash,
        104U,
        {
            make_coinbase(
                104U,
                quintum::consensus::block_subsidy(104U),
                0x45U
            )
        }
    );
    const auto c5_result = chain.connect_block(c5);
    assert(
        c5_result.error ==
        quintum::ChainConnectError::invalid_ancestor
    );

    assert(chain.block_index_size() >= 109U);
}

} // namespace

int main()
{
    test_connect_disconnect_and_atomic_failure();
    test_heavier_fork_reorg_and_failed_reorg_rollback();
    return 0;
}
