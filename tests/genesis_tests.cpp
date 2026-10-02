#include "chain/chainstate.hpp"
#include "consensus/chainparams.hpp"
#include "consensus/genesis.hpp"
#include "consensus/monetary.hpp"
#include "consensus/pow.hpp"
#include "consensus/tx_auth.hpp"
#include "primitives/block.hpp"
#include "primitives/transaction.hpp"

#include <cassert>
#include <string>

namespace {

std::string to_hex(const quintum::Hash256& value)
{
    static constexpr char digits[] = "0123456789abcdef";

    std::string out;
    out.reserve(value.size() * 2U);

    for (const auto byte : value) {
        out.push_back(digits[(byte >> 4U) & 0x0fU]);
        out.push_back(digits[byte & 0x0fU]);
    }

    return out;
}

std::string script_as_ascii(const quintum::Bytes& script)
{
    return std::string{
        script.begin(),
        script.end()
    };
}

void verify_network_genesis(
    const quintum::consensus::ChainParams& params,
    std::string_view expected_hash,
    std::string_view expected_merkle,
    std::uint64_t expected_nonce)
{
    assert(params.genesis.enforce);
    assert(params.genesis.nonce == expected_nonce);
    assert(to_hex(params.genesis.hash) == expected_hash);
    assert(
        to_hex(params.genesis.merkle_root) ==
        expected_merkle
    );

    assert(quintum::consensus::verify_genesis(params));

    const auto block =
        quintum::consensus::create_genesis_block(params);

    assert(block.transactions.size() == 1U);
    assert(block.header.version == 1U);
    assert(block.header.timestamp ==
           params.genesis.timestamp);
    assert(block.header.bits == params.genesis.bits);
    assert(block.header.nonce == params.genesis.nonce);
    assert(block.header.merkle_root ==
           params.genesis.merkle_root);
    assert(quintum::block_hash(block.header) ==
           params.genesis.hash);

    const auto& coinbase = block.transactions.front();

    assert(coinbase.is_coinbase());
    assert(coinbase.inputs.size() == 1U);
    assert(coinbase.outputs.size() == 1U);

    assert(
        script_as_ascii(
            coinbase.inputs.front().unlocking_script) ==
        params.genesis.message
    );

    assert(
        coinbase.outputs.front().value ==
        quintum::consensus::block_subsidy(0U)
    );

    assert(
        quintum::consensus::is_provably_unspendable(
            coinbase.outputs.front().locking_script)
    );

    // A single-transaction block has txid == Merkle root.
    assert(
        quintum::transaction_id(coinbase) ==
        params.genesis.merkle_root
    );

    assert(
        quintum::consensus::check_proof_of_work(
            block.header,
            params.pow) ==
        quintum::consensus::PowCheckError::none
    );
}

void test_all_genesis_vectors()
{
    verify_network_genesis(
        quintum::consensus::mainnet_params(),
        "0000008b82073109dc079e6c5b7eac0c2fba8a5633822f87fc33719633ebab8c",
        "b1d9e1aedbe90d5b88148d4af6b0a64d6fb20c286d72192713147869e69580c5",
        591'080ULL
    );

    verify_network_genesis(
        quintum::consensus::testnet_params(),
        "0000039bf09b49dfa4c9bcb924c0c38257fdeef9952021baabceb86fa099e872",
        "d5be95629c8ce60e22e817bc54accc3ed75983d5267b89724cd808f422a8c179",
        969'294ULL
    );

    verify_network_genesis(
        quintum::consensus::regtest_params(),
        "211c0cdb97dfb8bc2f0190e40132d1eb3be5ab9a03c9717aa3b2e90a98b3fcd6",
        "01b9f141fff566d6d50e700ff0c59f07ff0a89123921340bd946f30386c09d89",
        0ULL
    );
}

void test_chainstate_requires_exact_genesis()
{
    const auto& params =
        quintum::consensus::regtest_params();

    quintum::Chainstate chain{params};

    auto wrong =
        quintum::consensus::create_genesis_block(params);

    ++wrong.header.nonce;

    const auto wrong_result =
        chain.connect_block(
            wrong,
            params.genesis.timestamp
        );

    assert(
        wrong_result.error ==
        quintum::ChainConnectError::wrong_genesis
    );
    assert(chain.empty());
    assert(chain.block_index_size() == 0U);

    const auto genesis =
        quintum::consensus::create_genesis_block(params);

    const auto result =
        chain.connect_block(
            genesis,
            params.genesis.timestamp
        );

    assert(result.ok());
    assert(result.activated);
    assert(chain.height() && *chain.height() == 0U);
    assert(chain.tip_hash() &&
           *chain.tip_hash() == params.genesis.hash);
}

void test_genesis_reward_has_no_owner()
{
    const auto block =
        quintum::consensus::create_genesis_block(
            quintum::consensus::mainnet_params()
        );

    const auto& output =
        block.transactions.front().outputs.front();

    quintum::Transaction spend;
    spend.inputs.push_back(quintum::TxInput{
        .previous_output = quintum::OutPoint{
            .txid = quintum::transaction_id(
                block.transactions.front()
            ),
            .index = 0U,
        },
    });

    spend.outputs.push_back(quintum::TxOutput{
        .value = output.value,
        .locking_script = {0x01U},
    });

    assert(
        quintum::consensus::verify_input_authorization(
            spend,
            0U,
            output
        ) ==
        quintum::consensus::InputAuthError::
            provably_unspendable
    );
}

} // namespace

int main()
{
    test_all_genesis_vectors();
    test_chainstate_requires_exact_genesis();
    test_genesis_reward_has_no_owner();
    return 0;
}
